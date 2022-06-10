/*
 * Programme: cgi-runas
 *
 * Run CGI scripts as their owner.
 *
 * See MANUAL.rst and <https://https://github.com/odkr/cgi-runas> for details.
 *
 * Copyright 2022 Odin Kroeger
 *
 * This programme is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This programme is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// The configuration.
#include "config.h"


/*
 * CONSTANTS
 * =========
 */

/*
 * Constant: CR_SELF_EXE
 *
 * `/proc/self/exe`. Needed to locate the executable.
 */
#define CR_SELF_EXE "/proc/self/exe"

/*
 * Constant: CR_PATH_MAX
 *
 * A safe fallback value for the maximum length of a path,
 * in case `PATH_MAX` and `pathconf(<path>, _PC_PATH_MAX)`
 * are indeterminate. See <path_max> for details.
 */
#define CR_PATH_MAX 256

/*
 * Constant: SECURE_PATH_MAX
 *
 * The maximum length for `SECURE_PATH`.
 */
#define SECURE_PATH_MAX 1024


/*
 * MACROS
 * ======
 */

/*
 * Macro: STREQ
 *
 * Check whether two strings are equal.
 *
 * Arguments:
 *
 *    a - A string.
 *    b - Another string.
 *
 * Returns:
 *
 *    Non-zero - The strings are equal.
 *    0 - The strings are *not* equal.
 */
#define STREQ(a, b) (strcmp((a), (b)) == 0)

/*
 * Macro: STRNE
 *
 * Check whether two strings are *not* equal.
 *
 * Arguments:
 *
 *    a - A string.
 *    b - Another string.
 *
 * Returns:
 *
 *    Non-zero - The strings are *not* equal.
 *    0 - The strings are equal.
 */
#define STRNE(a, b) (strcmp((a), (b)) != 0)

/*
 * Macro: STRSTARTW
 *
 * Check wether string A starts with string B.
 *
 * Arguments:
 *
 *    a - A string.
 *    b - Another string.
 *
 * Returns:
 *
 *    Non-zero - A does start with B.
 *    0 - A does *not* start with B.
 */
#define STRSTARTW(a, b) (strncmp(a, b, strlen(b)) == 0)

/*
 * Macro: STRSTARTW
 *
 * Check wether string A does *not* start with string B.
 *
 * Arguments:
 *
 *    a - A string.
 *    b - Another string.
 *
 * Returns:
 *
 *    Non-zero - A does *not* start with B.
 *    0 - A does start with B.
 */
#define STRNOSTARTW(a, b) (strncmp(a, b, strlen(b)) != 0)


/*
 * GLOBALS
 * =======
 */

/*
 * Global: safe_env_vars
 *
 * A list of patterns that match safe environment variables.
 * Patterns that end with a '=' match the whole variable name,
 * other patterns match the beginning of the variable name.
 *
 * Do *not* add the empty string.
 * The list must be terminated with `NULL`.
 *
 * It has been adapted from Apache's suExec.
 *
 * See also:
 *
 *    - <unsafe_env_vars>
 */
const char *const safe_env_vars[] =
{
	// Variable name starts with:
	"HTTP_",
	"SSL_",

	// Variable name is:
	"AUTH_TYPE=",
	"CONTENT_LENGTH=",
	"CONTENT_TYPE=",
	"CONTEXT_DOCUMENT_ROOT=",
	"CONTEXT_PREFIX=",
	"DATE_GMT=",
	"DATE_LOCAL=",
	"DOCUMENT_NAME=",
	"DOCUMENT_PATH_INFO=",
	"DOCUMENT_ROOT=",
	"DOCUMENT_URI=",
	"GATEWAY_INTERFACE=",
	"HTTPS=",
	"LAST_MODIFIED=",
	"PATH_INFO=",
	"PATH_TRANSLATED=",
	"QUERY_STRING=",
	"QUERY_STRING_UNESCAPED=",
	"REMOTE_ADDR=",
	"REMOTE_HOST=",
	"REMOTE_IDENT=",
	"REMOTE_PORT=",
	"REMOTE_USER=",
	"REDIRECT_ERROR_NOTES=",
	"REDIRECT_HANDLER=",
	"REDIRECT_QUERY_STRING=",
	"REDIRECT_REMOTE_USER=",
	"REDIRECT_SCRIPT_FILENAME=",
	"REDIRECT_STATUS=",
	"REDIRECT_URL=",
	"REQUEST_METHOD=",
	"REQUEST_URI=",
	"REQUEST_SCHEME=",
	"SCRIPT_FILENAME=",
	"SCRIPT_NAME=",
	"SCRIPT_URI=",
	"SCRIPT_URL=",
	"SERVER_ADMIN=",
	"SERVER_NAME=",
	"SERVER_ADDR=",
	"SERVER_PORT=",
	"SERVER_PROTOCOL=",
	"SERVER_SIGNATURE=",
	"SERVER_SOFTWARE=",
	"UNIQUE_ID=",
	"USER_NAME=",
	"TZ=",

	// Terminator. DO *NOT* REMOVE!
	NULL
};

/*
 * Global: unsafe_env_vars
 *
 * A list of patterns that match unsafe environment variables.
 * See <safe_env_vars> for the sytax. <unsafe_env_vars>
 * takes precedence over <safe_env_vars>.
 *
 * Do *not* add the empty string.
 * The list must be terminated with `NULL`.
 *
 * It has been adapted from Apache's suExec.
 *
 * See also:
 *
 *    - <safe_env_vars>
 */
const char *const unsafe_env_vars[] =
{
	// Variable name starts with:

	// Variable name is:
	"HTTP_PROXY",

	// Terminator. DO *NOT* REMOVE!
	NULL
};

/* 
 * Global: prog_path
 *
 * The path to the programme's executable.
 * Set by <main>.
 */
char *prog_path = NULL;

/* 
 * Global: prog_name
 *
 * The filename of the programme's executable.
 * Set by <main>.
 * Also used by <panic>.
 */ 
char *prog_name = NULL;


/*
 * DATA TYPES
 * ==========
 */

/* 
 * Type: list_t
 *
 * An item in a linked list.
 *
 * See also:
 *
 *    - <push>.
 */
typedef struct list_s {
	void          *data;
	struct list_s *prev;
} list_t;


/*
 * FUNCTIONS
 * =========
 */

/* Function: panic
 *
 * Print an error message to STDERR and exit the programme.
 *
 * Arguments:
 * 
 *    status  - Status to exit with.
 *    message - Message to print.
 *    ...     - Arguments for the message (think `printf`).
 *
 * Globals:
 *
 *    <prog_name> - The filename of the executable.
 *                  If not `NULL`, `prog_name`, a colon, and a space
 *                  are printed before the message.
 */
void panic (const int status, const char *message, ...) {
	if (prog_name) fprintf(stderr, "%s: ", prog_name);
	va_list argp;
	va_start(argp, message);
	vfprintf(stderr, message, argp);
	va_end(argp);
	fprintf(stderr, "\n");
	exit(status);
}

/*
 * Function: push
 *
 * Add an item to the end of a linked list.
 *
 * Arguments:
 *
 *    list - A pointer to a linked list.
 *    item - A pointer to an item.
 *
 * Returns:
 *
 *    1 - On success.
 *    0 - On failure.
 *        If `errno` is set, `malloc` failed;
 *        otherwise, the list pointer was `NULL`.
 *
 * Caveats:
 *
 *    The list is modified inplace and the pointer to the list is moved to the next item!
 *
 * Example:
 *
 *    === C ===
 *    list_t *foo = NULL;
 *    char *bar = "bar";
 *    push(&foo, bar);
 *
 *    list_t *item = foo;
 *    while (item) {
 *        printf("%s\n", item->data);
 *        item = item->prev;
 *    }
 *    =========
 */
int push (list_t **head, void *item) {
	if (!head) return 0;
	list_t *next = malloc(sizeof(list_t));
	if (!next) return 0;
	next->prev = *head;
	next->data = item;
	*head = next;
	return 1;
}

/*
 * Function: dir_names
 *
 * Get the parent directory of a file, the parent directory of that
 * directory, and so on up to the root directory ("/"), a relative
 * directory root ("."), or a given directory.
 *
 *
 * Arguments:
 *
 *    start - A file.
 *    stop  - Directory at which to stop.
 *            Set to `NULL` to traverse up to "/" or ".".
 *
 * Returns:
 *
 *    A list of directories or `NULL` on failure.
 *    If `errno` is set, `malloc` failed;
 *    otherwise, the list pointer was `NULL`.
 *
 * Caveats:
 *
 *    - Memory allocated to directories must be freed manually.
 *    - The paths given should be canonical.
 *
 * Example:
 *
 *    === C ===
 *    list_t *dirs = *dirnames("/some/dir", NULL);
 *    if (!dirs) {
 *        if (errno) panic(71, strerror(errno));
 *        else panic(78, "list head is NULL, this is a bug.")
 *    }
 *    list_t *item = dirs;
 *    while (item) {
 *        printf("%s\n", item->data);
 *        item = item->prev;
 *    }
 *    =========
 */
list_t *dir_names (char *start, char *stop) {
	list_t *dirs = NULL;
	char *dir = start;

	do {
		// Yes, it has to be that way.
		char *cpy = strdup(dir);
		if (!cpy) return NULL;
		char *ptr = dirname(cpy);
		dir = strdup(ptr);
		free(cpy);
		if (!dir) return NULL;
		if (!push(&dirs, dir)) return NULL;
	} while (
		(!stop || STRNE(dir, stop)) &&
		STRNE(dir, "/") && 
		STRNE(dir, ".")
	);

	return dirs;
}

/*
 * Function: path_max
 *
 * Get maximum length of a path on the filesystem that a file is located on.
 * 
 *
 * Argument:
 *
 *    path  - Path to a file.
 *
 *
 * Returns:
 *
 *    A length or -1 on failure.
 *    `path_max` can only fail if `stat` fails for the given path.
 *    `errno` is set accordginly.
 *
 *
 * Constants:
 *
 *    PATH_MAX    - Maximum length of a path the system allows.
 *    CR_PATH_MAX - A safe maximum length for paths,
 *                  in case the system does not set a limit.
 *
 * Caveats:
 *
 *    POSIX allows for `PATH_MAX` and `pathconf(<path>, _PC_PATH_MAX)` to be
 *    indeterminate. Therefore, `CR_PATH_MAX` defines a fallback.
 *
 *    Moreover, there is confusion about what `pathconf(<path>, _PC_PATH_MAX)`
 *    should return. POSIX.1-2008 implies that it should return the "actual
 *    value of [PATH_MAX] at runtime". This is the behaviour implemented in
 *    glibc. But the Linux manual says that `pathconf(<path>, _PC_PATH_MAX)`
 *    returns "the maximum length of a relative pathname when [the given]
 *    path [...] is the current working directory". That being so, `path_max`
 *    may be inaccurate if the current working directory is not the root
 *    directory.
 *
 *    Also, most operating systems only *partly* enforce `PATH_MAX`, that is,
 *    though some system calls may error or fail silently if they encouter
 *    a path that longer than `PATH_MAX` or `pathconf(<path>, _PC_PATH_MAX)`,
 *    they may still allow the user to create such paths.
 *
 * See also:
 *
 *    - <https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/limits.h.html>
 *    - <https://pubs.opengroup.org/onlinepubs/9699919799/functions/pathconf.html>
 *    - <https://www.gnu.org/software/libc/manual/html_node/Pathconf.html>
 *    - <https://man7.org/linux/man-pages/man0/limits.h.0p.html>
 *    - <https://man7.org/linux/man-pages/man3/pathconf.3.html>
 *    - <https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html>
 */
int path_max (char *path) {
	int path_max = CR_PATH_MAX;

	struct stat fs;
	if (stat(path, &fs) != 0)
		return -1;
	
	char *dir;
	if (fs.st_mode & S_ISDIR)
		dir = path;
	else
		dir = dirname(path);
	
	int pc_path_max = pathconf(dir, _PC_PATH_MAX);
	if (-1 < pc_path_max && pc_path_max < path_max)
		path_max = pc_path_max;

	if (-1 < PATH_MAX && PATH_MAX < pc_path_max)
		path_max = PATH_MAX;

	return path_max;
}

/*
 * Function: realpath_f
 *
 * Get the canonical path of a file,
 * but abort the programme if an error occors.
 *
 * Argument:
 *
 *    path  - Path to a file.
 *
 * Returns:
 *
 *    A pointer to the canonical path of a file.
 *
 * Caveats:
 *
 *    Uses <path_max> to ensure that the right file is returned.
 */
char *realpath_f (char *path) {
	int max, len;

	max = path_max(path);
	if (max == -1)
		panic(69, "stat %s: %s", path, strerror(errno));

	len = strlen(path);
	if (len == 0)
		panic(78, "got the empty string as path.");
	if (len > max)
		panic(69, "%s: path is too long.", path);

	char *restrict real = realpath(path, NULL);
	if (!real)
		panic(69, "realpath %s: %s.", path, strerror(errno));

	max = path_max(real);
	if (max == -1)
		panic(69, "stat %s: %s", path, strerror(errno));
	
	len = strlen(real);
	if (len == 0)
		panic(69, "%s: canonical path is the empty string.", path);
	if (len > max)
		panic(69, "%s: canonical path is too long.", path);

	return real;
}

/*
 * Function: is_excl_owner_f
 *
 * Abort the programme unless a given file's parent directory,
 * that directory's parent directory, and so on, up to a given
 * directory are owned by the given UID and the given GID and
 * are not world-writable.
 *
 * Arguments:
 *
 *    uid   - A user ID.
 *    gid   - A group ID.
 *    start - A file.
 *    stop  - Directory at which to stop.
 *            Set to `NULL` to traverse up to "/" or ".".
 *
 * See also:
 *
 *    - <dirnames>
 */
void is_excl_owner_f (int uid, int gid, char *start, char *stop) {
	list_t *dirs = dir_names(start, stop);
	if (!dirs) {
		if (errno) panic(71, strerror(errno));
		else panic(78, "encountered null pointer to list.");
	}

	list_t *item = dirs;
	while (item) {
		struct stat dir_fs;
		char *dir = item->data;
		list_t *prev = item->prev;

		if (stat(dir, &dir_fs) != 0)
			panic(69, "%s: %s.", dir, strerror(errno));
		if (dir_fs.st_uid != uid)
			panic(69, "%s: not owned by UID %d.", dir, uid);
		if (dir_fs.st_uid != gid)
			panic(69, "%s: not owned by GID %d.", dir, gid);
		if (dir_fs.st_mode & S_IWOTH)
			panic(69, "%s: is world-writable.", dir);

		free(dir);
		free(item);
		item = prev;
	}
}

/*
 * Function: is_subdir_f
 *
 * Abort the programme unless one directory is a sub-directory of another one.
 *
 * Directories count as their own sub-directory.
 *
 * Arguments:
 *
 *    sub   - Directory to check.
 *    super - The other directory. 
 */
void is_subdir_f (char *sub, char *super) {
	char sep = sub[strlen(super)];
	if (STRNOSTARTW(sub, super) || (sep != '/' && sep != '\0'))
		panic(69, "%s: not in %s.", sub, super);
}

/*
 * Function: is_portable_name
 *
 * Check if a string is a syntactically valid user- or groupname.
 *
 * Argument:
 *
 *    str - A string.
 *
 * Caveats:
 *
 *    Deviating from POSIX.1-2018, `is_portable_name` requiers the
 *    the first character of a name to a letter or the underscore ("_").
 *
 * Returns:
 *
 *     0 - The string is a syntactically valid user- or groupname.
 *    -1 - The string is the empty string.
 *    -2 - The string is not a portable name.
 *
 * See also:
 *
 *    - <https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_437>
 */
int is_portable_name (char *str) {
	int len = strlen(str);
	if (len == 0)
		return -1;

	int ord = (char) str[0];	
	if (
		!(65 <= ord && ord <= 90)  &&
		!(97 <= ord && ord <= 122) &&
		  95 != ord
	)
		return -2;

	int i;
	for (i = 0; i < len; i++) {
		ord = (char) str[i];
		if (
			!(48 <= ord && ord <= 57)  &&
			!(65 <= ord && ord <= 90)  &&
			!(97 <= ord && ord <= 122) &&
			  45 != ord &&
			  46 != ord &&
			  95 != ord
		)
			return -2;
	}
	
	return 0;
}


/*
 * MAIN
 * ====
 */

int main (void) {
	/*
	 * Prelude
	 * -------
	 */

	// There is only one set of these variables, as opposed to one for each 
	// user/group that we look up, because getpw*/getgr* return pointers to
	// a memory locaton that they re-use! That is, even if we used more
	// pointers, each of them would always point to the user/group record
	// that has been looked up most recently.
	struct group *grp;
	struct passwd *pwd;

	// The environment.
	extern char **environ;
	
	// Make sure that errno is 0.
	errno = 0;


	/*
	 * Clear environment
	 * -----------------
	 */
	
	// Words of wisdom from the authors of suexec.c:
	//
	// > While cleaning the environment, the environment should be clean.
	// > (E.g. malloc() may get the name of a file for writing debugging info.
	// > Bad news if MALLOC_DEBUG_FILE is set to /etc/passwd.)

	char **env_p = environ;
	char *empty = NULL;
	
	clearenv();


	/*
	 * Change working directory
	 * ------------------------
	 */
	
	// Needed for `path_max` to be accurate.
	if (chdir("/") != 0)
		panic(69, "chdir /: %s", strerror(errno));

	/*
	 * Self-discovery
	 * --------------
	 */

	// This section depends on /proc.
	// There is no other, reliable, way to find a process' executable.

	if (!prog_path)
		prog_path = realpath_f(CR_SELF_EXE);

	if (!prog_name)
		prog_name = basename(prog_path);


	/*
	 * Create safe environment
	 * -----------------------
	 */

	while (*env_p) {
		int len = strlen(*env_p);
		if (len == 0)
			continue;
		const char *const *safe = safe_env_vars;
		while (*safe) {
			if (STRSTARTW(*env_p, *safe)) {
				const char *const *unsafe = unsafe_env_vars;
				while (*unsafe) {
					if (STRSTARTW(*env_p, *unsafe))
						goto next;
					*unsafe++;
				}

				char *sep = strstr(*env_p, "=");
				if (!sep || sep == *env_p)
					goto next;

				char *val = sep + 1;
				if (val == *env_p + len)
					goto next;

				// Overwrite "=" with a null byte, so that
				// *env_p turns into the variable name.
				sep[0] = '\0';
				
				if (setenv(*env_p, val, 0) != 0)
					panic(69, "failed to set %s: %s.", env_p, strerror(errno));
				goto next;
			}
			*safe++;
		}
		next:
		*env_p++;
	}

	if (setenv("PATH", SECURE_PATH, 1) != 0)
		panic(69, "failed to set PATH: %s.", strerror(errno));


	/*
	 * Check configuration
	 * -------------------
	 */

	#ifdef CGI_HANDLER
		if (STREQ(CGI_HANDLER, ""))
			panic(64, "CGI_HANDLER: is the empty string.");

		char *restrict cgi_handler = realpath_f(CGI_HANDLER);
		if (STRNE(CGI_HANDLER, cgi_handler))
			panic(69, "CGI_HANDLER: %s: not a canonical path.", CGI_HANDLER);
		free(cgi_handler); cgi_handler = NULL;
		
		is_excl_owner_f(0, 0, CGI_HANDLER, NULL);

		struct stat cgi_handler_fs;
		if (stat(CGI_HANDLER, &cgi_handler_fs) != 0)
			panic(69, "%s: %s.", CGI_HANDLER, strerror(errno));

		if (cgi_handler_fs.st_uid != 0)
			panic(69, "CGI_HANDLER: %s: UID not 0.", CGI_HANDLER);
		if (cgi_handler_fs.st_gid !=0)
			panic(69, "CGI_HANDLER: %s: GID not 0.", CGI_HANDLER);
		if (cgi_handler_fs.st_mode & S_IWOTH)
			panic(69, "CGI_HANDLER: %s: is world-writable.", CGI_HANDLER);
		if (!(cgi_handler_fs.st_mode & S_ISREG))
			panic(69, "CGI_HANDLER: %s: not a regular file.", CGI_HANDLER);
		if (!(cgi_handler_fs.st_mode & S_IXOTH))
			panic(69, "CGI_HANDLER: %s: not world-executable.", CGI_HANDLER);
		if (cgi_handler_fs.st_mode & S_ISUID)
			panic(69, "CGI_HANDLER: %s: has its set-UID-bit set.", CGI_HANDLER);
		if (cgi_handler_fs.st_mode & S_ISGID)
			panic(69, "CGI_HANDLER: %s: has its set-GID-bit set.", CGI_HANDLER);
	#else
		panic(64, "CGI_HANDLER: not defined.")
	#endif
			
	#ifdef MIN_EFFECTIVE_UID
		if (MIN_EFFECTIVE_UID < 1)
			panic(64, "MIN_EFFECTIVE_UID: must be greater than 0.");
		#ifdef UID_MAX
			if (MIN_EFFECTIVE_UID > UID_MAX)
				panic(64, "MIN_EFFECTIVE_UID: must not be greater than %d.", UID_MAX);
		#endif
					
		#ifdef MAX_EFFECTIVE_UID
			if (MIN_EFFECTIVE_GID >= MAX_EFFECTIVE_UID)
				panic(64, "MIN_EFFECTIVE_GID: must be smaller than MAX_EFFECTIVE_UID.");
		#endif
	#else
		panic(64, "MIN_EFFECTIVE_UID: not defined.")
	#endif

	#ifdef MIN_EFFECTIVE_GID
		if (MIN_EFFECTIVE_GID < 1)
			panic(64, "MIN_EFFECTIVE_GID: must be greater than 0.");
		#ifdef GID_MAX
			if (MIN_EFFECTIVE_GID > GID_MAX)
				panic(64, "MIN_EFFECTIVE_GID: must not be greater than %d.", GID_MAX);
		#endif
							
		#ifdef MAX_EFFECTIVE_GID
			if (MIN_EFFECTIVE_GID >= MAX_EFFECTIVE_GID)
				panic(64, "MIN_EFFECTIVE_GID: must be smaller than MAX_EFFECTIVE_GID.");
		#endif
	#else
		panic(64, "MIN_EFFECTIVE_GID: not defined.")
	#endif

	#ifdef MAX_EFFECTIVE_UID
		if (MAX_EFFECTIVE_UID < 1)
			panic(64, "MAX_EFFECTIVE_UID: must be greater than 0.");
		#ifdef UID_MAX
			if (MAX_EFFECTIVE_UID > UID_MAX)
				panic(64, "MAX_EFFECTIVE_UID: must not be greater than %d.", UID_MAX);
		#endif
	#else
		panic(64, "MAX_EFFECTIVE_UID: not defined.")
	#endif

	#ifdef MAX_EFFECTIVE_GID
		if (MAX_EFFECTIVE_GID < 1)
			panic(64, "MAX_EFFECTIVE_GID: must be greater than 0.");
		#ifdef GID_MAX
			if (MAX_EFFECTIVE_GID > GID_MAX)
				panic(64, "MAX_EFFECTIVE_GID: must not be greater than %d.", GID_MAX);
		#endif
	#else
		panic(64, "MAX_EFFECTIVE_GID: not defined.")
	#endif

	#ifdef SCRIPT_BASE_DIR
		if (STREQ(SCRIPT_BASE_DIR, ""))
			panic(64, "SCRIPT_BASE_DIR: is the empty string.");

		char *restrict script_base_dir = realpath_f(SCRIPT_BASE_DIR);
		if (STRNE(SCRIPT_BASE_DIR, script_base_dir))
			panic(69, "%s: not a canonical path.", SCRIPT_BASE_DIR);
		free(script_base_dir); script_base_dir = NULL;
	
		is_excl_owner_f(0, 0, SCRIPT_BASE_DIR, NULL);

		struct stat script_base_dir_fs;
		if (stat(SCRIPT_BASE_DIR, &script_base_dir_fs) != 0)
			panic(69, "%s: %s.", SCRIPT_BASE_DIR, strerror(errno));

		if (script_base_dir_fs.st_uid != 0)
			panic(69, "%s: UID not 0.", SCRIPT_BASE_DIR);
		if (script_base_dir_fs.st_gid !=0)
			panic(69, "%s: GID not 0.", SCRIPT_BASE_DIR);
		if (script_base_dir_fs.st_mode & S_IWOTH)
			panic(69, "%s: is world-writable.", SCRIPT_BASE_DIR);
		// if (!(script_base_dir_fs.st_mode & S_ISDIR))
		// 	panic(69, "%s: not a directory.", SCRIPT_BASE_DIR);
		if (!(script_base_dir_fs.st_mode & S_IXOTH))
			panic(69, "%s: is not world-executable.", SCRIPT_BASE_DIR);
	#else
		panic(64, "SCRIPT_BASE_DIR: not defined.")
	#endif

	#ifdef SCRIPT_SUFFIX
		if (STREQ(SCRIPT_SUFFIX, ""))
			panic(64, "SCRIPT_SUFFIX: is the empty string.");
	#else
		panic(64, "SCRIPT_SUFFIX: not defined.")
	#endif

	#ifdef SECURE_PATH
		if (strlen(SECURE_PATH) > SECURE_PATH_MAX)
			panic(64, "SECURE_PATH: is too long.");
	#else
		panic(64, "SECURE_PATH: not defined.")
	#endif

	#ifdef WWW_USER
		if (STREQ(WWW_USER, ""))
			panic(64, "WWW_USER: is the empty string.");
		if (is_portable_name(WWW_USER) != 0)
			panic(64, "%s: invalid username.", WWW_USER);
		pwd = getpwnam(WWW_USER); 
		if (!pwd)
			panic(67, "%s: no such user.", WWW_USER);
	#else
		panic(64, "WWW_USER: not defined.")
	#endif

	#ifdef WWW_GROUP
		if (STREQ(WWW_GROUP, ""))
			panic(64, "WWW_GROUP: is the empty string.");
		if (is_portable_name(WWW_GROUP) != 0)
			panic(64, "%s: invalid username.", WWW_GROUP);
		grp = getgrnam(WWW_GROUP);
		if (!grp)
			panic(67, "%s: no such group.", WWW_GROUP);
	#else
		panic(64, "WWW_GROUP: not defined.")
	#endif

	// Needed later.
	int www_uid = pwd->pw_uid;
	int www_gid = grp->gr_gid;

	pwd = NULL;
	grp = NULL;


	/*
	 * Self-check
	 * ----------
	 */

	// If this executable were insecure, these checks wouldn't be run
	// to begin with, of course. Their purpose is to force the user
	// to secure their setup.

	is_excl_owner_f(0, 0, prog_path, NULL);

	struct stat prog_fs;
	if (stat(prog_path, &prog_fs) != 0)
		panic(69, "%s: %s.", prog_path, strerror(errno));

	if (prog_fs.st_uid != 0)
		panic(69, "%s: UID not 0.", prog_path);
	if (prog_fs.st_gid != www_gid)
		panic(69, "%s: GID not %d.", prog_path, www_gid);
	if (prog_fs.st_mode & S_IWGRP)
		panic(69, "%s: is group-writable.", prog_path);
	if (prog_fs.st_mode & S_IWOTH)
		panic(69, "%s: is world-writable.", prog_path);
	if (prog_fs.st_mode & S_IXOTH)
		panic(69, "%s: is world-executable.", prog_path);
	if (prog_fs.st_mode & S_ISGID)
		panic(69, "%s: has its set-GID-bit set.", prog_path);

	/*
	 * Get script's path
	 * -----------------
	 */

	char *path_translated = NULL;
	path_translated = getenv("PATH_TRANSLATED");
	if (path_translated == NULL)
		panic(64, "PATH_TRANSLATED: not set.");
	if (STREQ(path_translated, ""))
		panic(64, "PATH_TRANSLATED: is empty.");
	
	char *restrict script_path = realpath_f(path_translated);
	if (STRNE(path_translated, script_path))
		panic(69, "PATH_TRANSLATED: %s: not a canonical path.", path_translated);


	/*
	 * Check script's UID and GID
	 * --------------------------
	 */

	struct stat script_fs;
	if (stat(script_path, &script_fs) != 0)
		panic(69, "%s: %s.", script_path, strerror(errno));

	if (script_fs.st_uid == 0)
		panic(69, "%s: UID is 0.", script_path);
	if (script_fs.st_gid == 0)
		panic(69, "%s: GID is 0.", script_path);
	if (script_fs.st_uid < MIN_EFFECTIVE_UID || script_fs.st_uid > MAX_EFFECTIVE_UID)
		panic(69, "%s: UID is privileged.", script_path);
	if (script_fs.st_gid < MIN_EFFECTIVE_GID || script_fs.st_uid > MAX_EFFECTIVE_GID)
		panic(69, "%s: GID is privileged.", script_path);

	pwd = getpwuid(script_fs.st_uid);
	if (!pwd)
		panic(67, "%s's UID %d: no such user.", script_path, script_fs.st_uid);
	if (is_portable_name(pwd->pw_name) != 0)
		panic(67, "%s: invalid username.", pwd->pw_name);
	grp = getgrgid(script_fs.st_gid);
	if (!grp)
		panic(67, "%s's GID %d: no such group.", script_path, script_fs.st_uid);
	if (is_portable_name(grp->gr_name) != 0)
		panic(67, "%s: invalid groupname.", grp->gr_name);
	if (script_fs.st_gid != pwd->pw_gid)
		panic(67, "%s's GID %d: not %s's primary group.", script_path, script_fs.st_gid, pwd->pw_name);

	pwd = NULL;
	grp = NULL;


	/*
	 * Drop privileges
	 * ---------------
	 */

	// This section uses setgroups(), which is non-POSIX.

	const gid_t groups[] = {};
	if (setgroups(0, groups) != 0)
		panic(69, "setgroups: %s.", strerror(errno));
	if (setgid(script_fs.st_gid) != 0)
		panic(69, "setgid %d: %s.", script_fs.st_gid, strerror(errno));
	if (setuid(script_fs.st_uid) != 0)
		panic(69, "setuid %s: %s.", script_fs.st_uid, strerror(errno));
	if (setuid(0) != -1)
		panic(69, "setuid 0: %s.", strerror(errno));


	/*
	 * Check if run by webserver
	 * -------------------------
	 */

	uid_t prog_uid = getuid();
	if (prog_uid != www_uid)
		panic(77, "UID %d: not permitted.", prog_uid);

	gid_t prog_gid = getgid();
	if (prog_gid != www_gid)
		panic(77, "GID %d: not permitted.", prog_gid);


	/*
	 * Does PATH_TRANSLATED point to a safe file?
	 * ------------------------------------------
	 */

	is_subdir_f(script_path, SCRIPT_BASE_DIR);

	char *restrict home_dir = realpath_f(pwd->pw_dir);
	if (STRNE(pwd->pw_dir, home_dir))
		panic(69, "%s: not a canonical path.", pwd->pw_dir);
	free(home_dir); home_dir = NULL;

	is_subdir_f(script_path, pwd->pw_dir);

	char *document_root = NULL;
	document_root = getenv("DOCUMENT_ROOT");
	if (document_root == NULL)
		panic(64, "DOCUMENT_ROOT: not set.");
	if (STREQ(document_root, ""))
		panic(64, "DOCUMENT_ROOT: is empty.");
	
	char *restrict canonical_document_root = realpath_f(document_root);
	if (STRNE(document_root, canonical_document_root))
		panic(69, "DOCUMENT_ROOT: %s: not a canonical path.", document_root);
	free(canonical_document_root); canonical_document_root = NULL;

	is_subdir_f(script_path, document_root);
	free(document_root); document_root = NULL;

	is_excl_owner_f(script_fs.st_uid, script_fs.st_gid, script_path, pwd->pw_dir);
	is_excl_owner_f(0, 0, pwd->pw_dir, NULL);

	if (script_fs.st_mode & S_IWOTH)
		panic(69, "%s: is world-writable.", script_path);
	if (script_fs.st_mode & S_IXOTH)
		panic(69, "%s: is world-executable.", script_path);
	if (script_fs.st_mode & S_ISUID)
		panic(69, "%s: has its set-UID-bit set.", script_path);
	if (script_fs.st_mode & S_ISGID)
		panic(69, "%s: has its set-GID-bit set.", script_path);


	/*
	 * Does PATH_TRANSLATED point to a CGI script?
	 * -------------------------------------------
	 */

	char *suffix = strrchr(script_path, '.');
	if (!suffix)
		panic(64, "%s: has no filename ending.", script_path);
	if (STRNE(suffix, SCRIPT_SUFFIX))
		panic(64, "%s: does not end with \"%s\".", script_path, SCRIPT_SUFFIX);


	/*
	 * Call CGI handler
	 * ----------------
	 */

	char *const argv[] = { CGI_HANDLER, NULL };
	execve(CGI_HANDLER, argv, environ);

	panic(71, "execve %s: %s.", CGI_HANDLER, strerror(errno));
}
