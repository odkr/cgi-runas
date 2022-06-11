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
#include <time.h>
#include <unistd.h>


/*
 * CONFIGURATION
 * =============
 */

#include "config.h"

#if !defined(CGI_HANDLER)
	#error CGI_HANDLER: not defined.
#endif

#if !defined(DATE_FORMAT)
	#define DATE_FORMAT "%b %e %T"
#endif

#ifdef SCRIPT_MIN_UID
	#if SCRIPT_MIN_UID < 1
		#error SCRIPT_MIN_UID: must be greater than 0.
	#endif
	#ifdef UID_MAX
		#if SCRIPT_MIN_UID > UID_MAX
			#error SCRIPT_MIN_UID: must not be greater than UID_MAX.
		#endif
	#endif

	#ifdef SCRIPT_MAX_UID
		#if SCRIPT_MIN_GID >= SCRIPT_MAX_UID
			#error SCRIPT_MIN_GID: must be smaller than SCRIPT_MAX_UID.
		#endif
	#endif
#else
	#error SCRIPT_MIN_UID: not defined.
#endif

#ifdef SCRIPT_MIN_GID
	#if SCRIPT_MIN_GID < 1
		#error SCRIPT_MIN_GID: must be greater than 0.
	#endif
	#ifdef GID_MAX
		#if SCRIPT_MIN_GID > GID_MAX
			#error SCRIPT_MIN_GID: must not be greater than GID_MAX.
		#endif
	#endif

	#ifdef SCRIPT_MAX_GID
		#if SCRIPT_MIN_GID >= SCRIPT_MAX_GID
			#error SCRIPT_MIN_GID: must be smaller than SCRIPT_MAX_GID.
		#endif
	#endif
#else
	#error SCRIPT_MIN_GID: not defined.
#endif

#ifdef SCRIPT_MAX_UID
	#if SCRIPT_MAX_UID < 1
		#error SCRIPT_MAX_UID: must be greater than 0.
	#endif
	#ifdef UID_MAX
		#if SCRIPT_MAX_UID > UID_MAX
			#error SCRIPT_MAX_UID: must not be greater than UID_MAX.
		#endif
	#endif
#else
	#error SCRIPT_MAX_UID: not defined.
#endif

#ifdef SCRIPT_MAX_GID
	#if SCRIPT_MAX_GID < 1
		#error SCRIPT_MAX_GID: must be greater than 0.
	#endif
	#ifdef GID_MAX
		#if SCRIPT_MAX_GID > GID_MAX
			#error SCRIPT_MAX_GID: must not be greater than GID_MAX.
		#endif
	#endif
#else
	#error SCRIPT_MAX_GID: not defined.
#endif

#if !defined(SCRIPT_BASE_DIR)
	#error SCRIPT_BASE_DIR: not defined.
#endif

#if !defined(SCRIPT_SUFFIX)
	#error SCRIPT_SUFFIX: not defined.
#endif

#if !defined(SECURE_PATH)
	#error SECURE_PATH: not defined.
#endif

#if !defined(WWW_USER)
	#error WWW_USER: not defined.
#endif

#if !defined(WWW_GROUP)
	#error WWW_USER: not defined.
#endif


/*
 * CONSTANTS
 * =========
 */

// Exit statuses conform to the BSD convention.
// See <https://www.freebsd.org/cgi/man.cgi?query=sysexits>.

/*
 * Constant: EX_NOINPUT
 *
 * Status to exit with if a file does not exist.
 */
#define EX_NOINPUT 66

/*
 * Constant: EX_NOUSER
 *
 * Status to exit with if a user does not exist.
 */
#define EX_NOUSER 67

/*
 * Constant: EX_UNAVAILABLE
 *
 * Status to exit with if no other status matches.
 */
#define EX_UNAVAILABLE 69

/*
 * Constant: EX_SOFTWARE
 *
 * Status to exit with if the programmer made an error.
 */
#define EX_SOFTWARE 70

/*
 * Constant: EX_OSERR
 *
 * Status to exit with an operating system error occurs.
 */
#define EX_OSERR 71

/*
 * Constant: EX_NOPERM
 *
 * Status to exit with an operating system error occurs.
 */
#define EX_NOPERM 77

/*
 * Constant: EX_CONFIG
 *
 * Status to exit with an the user misconfigured something.
 */
#define EX_CONFIG 78

/*
 * Constant: CR_PATH_MAX
 *
 * A safe fallback value for the maximum length of a path,
 * in case `PATH_MAX` and `pathconf(<path>, _PC_PATH_MAX)`
 * are indeterminate. See <path_max> for details.
 */
#define CR_PATH_MAX 256

/*
 * Constant: CR_SECURE_PATH_MAX
 *
 * The maximum length for `SECURE_PATH`.
 */
#define CR_SECURE_PATH_MAX 1024

/*
 * Constant: CR_SELF_EXE
 *
 * `/proc/self/exe`. Needed to locate the executable.
 */
#define CR_SELF_EXE "/proc/self/exe"

/*
 * Constant: CR_TS_MAX
 *
 * Maximum length of the timestamp for error message.
 */
#define CR_TS_MAX 128


/*
 * MACROS
 * ======
 */

/*
 * Macro: EPRINTF
 *
 * Print a formatted string to STDERR.
 *
 * Arguments:
 *
 *    See `printf`.
 */
#define EPRINTF(...) fprintf(stderr, __VA_ARGS__)

/*
 * Macro: EPUTS
 *
 * Print to STDERR.
 *
 * Arguments:
 *
 *    See `puts`.
 */
#define EPUTS(a) fputs(a, stderr)

/*
 * Macro: ERR_NOINPUT
 *
 * Raise a no-such-file error.
 *
 * Arguments:
 *
 *    The same as <panic>, but without the status.
 */
#define ERR_NOINPUT(...) panic(EX_NOINPUT, __VA_ARGS__)

/*
 * Macro: ERR_NOUSER
 *
 * Raise a no-such-user error.
 *
 * Arguments:
 *
 *    The same as <panic>, but without the status.
 */
#define ERR_NOUSER(...) panic(EX_NOUSER, __VA_ARGS__)

/*
 * Macro: ERR_UNAVAILABLE
 *
 * Raise a generic error.
 *
 * Arguments:
 *
 *    The same as <panic>, but without the status.
 */
#define ERR_UNAVAILABLE(...) panic(EX_UNAVAILABLE, __VA_ARGS__)

/*
 * Macro: ERR_SOFTWARE
 *
 * Raise a software error.
 *
 * Arguments:
 *
 *    The same as <panic>, but without the status.
 */
#define ERR_SOFTWARE(...) panic(EX_SOFTWARE, __VA_ARGS__)

/*
 * Macro: ERR_OSERR
 *
 * Raise an operating system error.
 *
 * Arguments:
 *
 *   The same as <panic>, but without the status.
 */
#define ERR_OSERR(...) panic(EX_OSERR, __VA_ARGS__)

/*
 * Macro: ERR_NOPERM
 *
 * Raise a permission error.
 *
 * Arguments:
 *
 *    The same as <panic>, but without the status.
 */
#define ERR_NOPERM(...) panic(EX_NOPERM, __VA_ARGS__)

/*
 * Macro: ERR_CONFIG
 *
 * Raise a configuration error.
 *
 * Arguments:
 *
 *    The same as <panic>, but without the status.
 */
#define ERR_CONFIG(...) panic(EX_CONFIG, __VA_ARGS__)

/*
 * Macro: ASSERT
 *
 * Raise an unavailability error if a condition fails.
 *
 * Arguments:
 *
 *    cond - A condition.
 *    ...  - See <ERR_UNAVAILABLE>.
 */
#define ASSERT(cond, ...) if (!(cond)) ERR_UNAVAILABLE(__VA_ARGS__)


/*
 * Macro: ASS_CONF_NEMPTY
 *
 * Raise a configuration error if a constant is empty the empty string.
 *
 * Arguments:
 *
 *    cf - A configuration macro.
 */
#define ASS_CONF_NEMPTY(cf) if (STREQ(cf, "")) ERR_CONFIG(#cf ": is empty.")

/*
 * Macro: ASS_USER_EXISTS
 *
 * Raise an OS error if a user does not exist.
 *
 * Arguments:
 *
 *    pwd  - A `struct passwd` pointer.
 *    user - A username.
 *
 * Side-effects:
 *
 *    `pwd` is overwritten with the user's record.
 */
#define ASS_USER_EXISTS(pwd, user) \
	if (!(pwd = getpwnam(user))) \
		ERR_NOUSER("%s: no such user.", user)

/*
 * Macro: ASS_GROUP_EXISTS
 *
 * Raise a no-such-user error if a group does not exist.
 *
 * Arguments:
 *
 *    grp   - A `struct group` pointer.
 *    group - A groupname.
 *
 * Side-effects:
 *
 *    `grp` is overwritten with the group's record.
 */
#define ASS_GROUP_EXISTS(grp, group) \
	if (!(grp = getgrnam(group))) \
		ERR_NOUSER("%s: no such group.", group)

/*
 * Macro: ASS_UID_EXISTS
 *
 * Raise a no-such-user error if a UID does not exist.
 *
 * Arguments:
 *
 *    pwd - A `struct passwd` pointer.
 *    uid - A UID.
 *
 * Side-effects:
 *
 *    `pwd` is overwritten with the user's record.
 */
#define ASS_UID_EXISTS(pwd, uid) \
		if (!(pwd = getpwuid(uid))) \
			ERR_NOUSER("UID %d: no such user.", uid)

/*
 * Macro: ASS_GID_EXISTS
 *
 * Raise a no-such-user error if a GID does not exist.
 *
 * Arguments:
 *
 *    grp - A `struct group` pointer.
 *    gid - A GID.
 *
 * Side-effects:
 *
 *    `grp` is overwritten with the group's record.
 */
#define ASS_GID_EXISTS(grp, gid) \
		if (!(grp = getgrgid(gid))) \
			ERR_NOUSER("GID %d: no such group.", gid)

/*
 * Macro: ASS_STAT
 *
 * Raise a no-input error if a stat fails.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - A `struct stat` pointer.
 *
 * Side-effects:
 *
 *    `fs` is overwritten with the file's metadata record.
 */
#define ASS_STAT(fname, fs) \
	if (stat(fname, fs) != 0) \
		ERR_NOINPUT("stat %s: %s", fname, strerror(errno))

/*
 * Macro: ASS_UID
 *
 * Raise a permission error if a file has the wrong UID.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - The `struct stat` record of that file.
 *    uid    - A UID.
 */
#define ASS_UID(fname, fs, uid) \
	if (fs.st_uid != uid) \
		ERR_NOPERM("%s: not owned by UID %d.", fname, uid)

/*
 * Macro: ASS_GID
 *
 * Raise a permission error if a file has the wrong GID.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - The `struct stat` record of that file.
 *    gid    - A GID.
 */
#define ASS_GID(fname, fs, gid) \
if (fs.st_gid != gid) \
	ERR_NOPERM("%s: not owned by GID %d.", fname, gid)

/*
 * Macro: ASS_ISDIR
 *
 * Raise an unavailability error if a file is not a directory.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - The `struct stat` record of that file.
 */
#define ASS_ISDIR(fname, fs) \
	ASSERT(S_ISDIR(fs.st_mode), "%s: not a directory.", fname)

/*
 * Macro: ASS_ISREG
 *
 * Raise an unavailability error if a file is not a regular file.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - The `struct stat` record of that file.
 */
#define ASS_ISREG(fname, fs) \
	ASSERT(S_ISREG(fs.st_mode), "%s: not a regular file.", fname)

/*
 * Macro: ASS_IXOTH
 *
 * Raise an unavailability error if a file is not world-executable.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - The `struct stat` record of that file.
 */
#define ASS_IXOTH(fname, fs) \
	if (fs.st_mode ^ S_IXOTH) \
		ERR_NOPERM("%s: is not world-executable.", fname)

/*
 * Macro: ASS_NWOTH
 *
 * Raise a permission error if a file is world-writable.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - The `struct stat` record of that file.
 */
#define ASS_NWOTH(fname, fs) \
	if (fs.st_mode & S_IWOTH) \
		ERR_NOPERM("%s: is world-writable.", fname)

/*
 * Macro: ASS_NXOTH
 *
 * Raise a permission error if a file is world-executable.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - The `struct stat` record of that file.
 */
 #define ASS_NXOTH(fname, fs) \
	if (fs.st_mode & S_IXOTH) \
		ERR_NOPERM("%s: is world-executable.", fname)

/*
 * Macro: ASS_NSUID
 *
 * Raise a permission error if a file's set-UID bit is set.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - The `struct stat` record of that file.
 */
#define ASS_NSUID(fname, fs) \
	if (fs.st_mode & S_ISUID) \
		ERR_NOPERM("%s: set-UID bit is set.", fname)

/*
 * Macro: ASS_NSGID
 *
 * Raise a permission error if a file's set-GID bit is set.
 *
 * Arguments:
 *
 *    fname  - A filename.
 *    fs     - The `struct stat` record of that file.
 */
#define ASS_NSGID(fname, fs) \
if (fs.st_mode & S_ISGID) \
	ERR_NOPERM("%s: set-GID bit is set.", fname)

/*
 * Macro: ASS_CANON
 *
 * Raise an unavailability error if a path is not canonical.
 *
 * Arguments:
 *
 *    canon - A `char` pointer to hold the canonical path.
 *    path  - A path.
 *
 * Side-effects:
 *
 *    `canon` is overwritten with path's canonical path.
 */
#define ASS_CANON(canon, path) \
	canon = realpath_f(path); \
	ASSERT(STREQ(path, canon), "%s: not canonical.", path)

/*
 * Macro: ASS_PORTNAME
 *
 * Raise an unavailability error if a name is not portable.
 *
 * Arguments:
 *
 *    name - A filename, username, or groupname.
 *
 * See also:
 *
 *    <is_portable_name>
 */
#define ASS_PORTNAME(name) \
	ASSERT(is_portable_name(name) == 0, "%s: invalid name.", name)

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
 * It has been adapted from Apache's suEXEC.
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
 * It has been adapted from Apache's suEXEC.
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
 * The message is prefixed with a timestamp if STDERR is not a TTY.
 *
 * Arguments:
 * 
 *    status  - Status to exit with.
 *    message - Message to print.
 *    ...     - Arguments for the message (think `printf`).
 *
 * Constants:
 *
 *    <DATE_FORMAT> - How to format the timestamp.
 *
 * Globals:
 *
 *    <prog_name> - The filename of the executable.
 *                  If not `NULL`, `prog_name`, a colon, and a space
 *                  are printed before the message.
 */
void panic (const int status, const char *message, ...) {
	if (!isatty(fileno(stderr))) {
		time_t now_sec = time(NULL);
		if (now_sec == -1) {
			EPRINTF("<time: %s>", strerror(errno));
		} else {
			struct tm *now_rec = localtime(&now_sec);
			if (!now_rec) {
				EPRINTF("<localtime: %s>", strerror(errno));
			} else {
				char ts[CR_TS_MAX];
				if (strftime(ts, CR_TS_MAX,
					     DATE_FORMAT, now_rec) == 0) {
					EPUTS("<strftime: returned 0.>");
				} else {
					EPUTS(ts);
				}
			}
		}
		EPUTS(": ");
	}

	if (prog_name) EPRINTF("%s: ", prog_name);
	
	va_list argp;
	va_start(argp, message);
	vfprintf(stderr, message, argp);
	va_end(argp);
	EPRINTF("\n");
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
 *    The list is modified inplace and
 *    the pointer to the list is moved to the next item!
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
int push (list_t **list, void *item) {
	if (!list) return 0;
	list_t *next = malloc(sizeof(list_t));
	if (!next) return 0;
	next->prev = *list;
	next->data = item;
	*list = next;
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
 *        if (errno) ERR_OSERR(strerror(errno));
 *        else ERR_SOFTWARE("encountered null pointer to list.")
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
 * Function: getenv_f
 *
 * Read an environment variable,
 * but abort the programme if an error occors or
 * the variable is unset or empty.
 *
 * Argument:
 *
 *    var - An environment variable.
 *
 * Returns:
 *
 *    A pointer to the content of the environment variable.
 */
char *getenv_f (char *var) {
	char *value = getenv(var);
	ASSERT(value, "%s: not set.", *var);
	ASSERT(STRNE(value, ""), "%s: is empty.", *var);
	return value;
}

/*
 * Function: setenv_f
 *
 * Set an environment variable,
 * but abort the programme if an error occors.
 *
 * Argument:
 *
 *    name  - A variable name.
 *    value - A value.
 *    ow    - Overwrite existing values?
 *            Use 1 for yes and 0 for no.
 */
void setenv_f (char *name, char *value, int ow) {
	ASSERT(setenv(name, value, ow) == 0,
	       "setenv %s: %s.", name, strerror(errno));
}

/*
 * Function: path_max
 *
 * Get maximum length of a path on the filesystem
 * that a file is located on.
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
 *    POSIX allows for `PATH_MAX` and `pathconf(<path>, _PC_PATH_MAX)`
 *    to be indeterminate. `CR_PATH_MAX` defines a fallback.
 *
 *    Moreover, there is confusion about what `pathconf(<path>,
 *    _PC_PATH_MAX)` should return. POSIX.1-2008 implies that it should
 *    return the "actual value of [PATH_MAX] at runtime". This is the
 *    behaviour implemented in glibc. But the Linux manual says that
 *    `pathconf(<path>, _PC_PATH_MAX)` returns "the maximum length of a
 *    relative pathname when [the given] path [...] is the current
 *    working directory". That being so, `path_max` may be inaccurate
 *    if the current working directory is not the root directory.
 *
 *    Also, most operating systems only *partly* enforce `PATH_MAX`,
 *    that is, though some system calls may error or fail silently if
 *    they encouter a path that longer than `PATH_MAX` or
 *    `pathconf(<path>, _PC_PATH_MAX)`, others won't, and the system
 *    may still allow the user to create such paths.
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
	if (S_ISDIR(fs.st_mode))
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
	ASSERT(max != -1, "stat %s: %s.", path, strerror(errno));

	len = strlen(path);
	if (len == 0) ERR_SOFTWARE("got empty string as path.");
	ASSERT(len <= max, "%s: path too long.", path);

	char *restrict real = realpath(path, NULL);
	ASSERT(real, "realpath %s: %s.", path, strerror(errno));

	max = path_max(real);
	ASSERT(max != -1, "stat %s: %s.", real, strerror(errno));
	
	len = strlen(real);
	ASSERT(len > 0, "%s: canonical path is empty.", path);
	ASSERT(len <= max, "%s: canonical path too long.", path);

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
		if (errno) ERR_OSERR(strerror(errno));
		ERR_SOFTWARE("encountered null pointer to list.");
	}

	list_t *item = dirs;
	while (item) {
		struct stat dir_fs;
		char *dir = item->data;
		list_t *prev = item->prev;

		ASS_STAT(dir, &dir_fs);
		ASS_UID(dir, dir_fs, uid);
		ASS_GID(dir, dir_fs, gid);
		ASS_NWOTH(dir, dir_fs);

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
	ASSERT(STRSTARTW(sub, super) || sep == '/' || sep == '\0',
	                "%s: not in %s.", sub, super);
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
 *    Deviating from POSIX.1-2018, `is_portable_name` requires the
 *    the first character of a name to be a letter or an underscore ("_").
 *
 * Returns:
 *
 *    0  - If the string is a portable name.
 *    -1 - Otherwise.
 *
 * See also:
 *
 *    - <https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_437>
 */
int is_portable_name (char *str) {
	int len = strlen(str);
	if (len == 0)
		return -1;

	int c = (char) str[0];
	if (
		// A-Z.
		!(65 <= c && c <= 90)	&&
		// a-z.
		!(97 <= c && c <= 122)	&&
		// "_"
		 95 != c
	) return -1;	

	int i;
	for (i = 1; i < strlen(str); i++) {
		c = (char) str[i];
		if (
			// 0-9.
			!(48 <= c && c <= 57)	&&
			// A-Z.
			!(65 <= c && c <= 90)	&&
			// a-z.
			!(97 <= c && c <= 122)	&&
			// "-"
			 45 != c		&&
			// "."
			 46 != c		&&
			// "_"
			 95 != c
		) return -1;
	}
	return 0;
}


/*
 * MAIN
 * ====
 */

int main (int argc, const char *argv[]) {
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
	// > (E.g. malloc() may get the name of a file for writing debugging
	// > info. Bad news if MALLOC_DEBUG_FILE is set to /etc/passwd.)

	char **env_p = environ;
	
	#ifdef NO_CLEARENV
		char *empty = NULL;
		environ = &empty;		
	#else
		clearenv();
	#endif


	/*
	 * Self-discovery
	 * --------------
	 */

	if (!prog_path) {
		const char *const paths[] = {CR_SELF_EXE, argv[0], NULL};
		const char *const *path = paths;
		while (*path) {
			errno = 0;
			if (*path)
				prog_path = realpath(*path, NULL);
			if (prog_path)
				break;
			*path++;
		}
		ASSERT(prog_path, "failed to find myself.");

		int len = strlen(prog_path);
		ASSERT(len > 0,
		                "%s: canonical path is empty.", prog_path);
		ASSERT(len <= CR_PATH_MAX,
		                "%s: canonical path too long.", prog_path);

		struct stat fs;
		ASS_STAT(prog_path, &fs);
	}

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
				
				setenv_f(*env_p, val, 0);
				goto next;
			}
			*safe++;
		}
		next:
		*env_p++;
	}

	setenv_f("PATH", SECURE_PATH, 1);


	/*
	 * Change working directory
	 * ------------------------
	 */

	// Needed for `path_max` to be accurate.
	ASSERT(chdir("/") == 0, "chdir /: %s", strerror(errno));


	/*
	 * Check configuration
	 * -------------------
	 */

	// CGI_HANDLER.
	ASS_CONF_NEMPTY(CGI_HANDLER);

	char *restrict cgi_handler = NULL;;
	ASS_CANON(cgi_handler, CGI_HANDLER);
	free(cgi_handler); cgi_handler = NULL;
	
	is_excl_owner_f(0, 0, CGI_HANDLER, NULL);

	struct stat cgi_handler_fs;
	ASS_STAT(CGI_HANDLER, &cgi_handler_fs);
	ASS_ISREG(CGI_HANDLER, cgi_handler_fs);
	ASS_UID(CGI_HANDLER, cgi_handler_fs, 0);
	ASS_GID(CGI_HANDLER, cgi_handler_fs, 0);
	ASS_NWOTH(CGI_HANDLER, cgi_handler_fs);
	ASS_NSUID(CGI_HANDLER, cgi_handler_fs);
	ASS_NSGID(CGI_HANDLER, cgi_handler_fs);
	ASS_IXOTH(CGI_HANDLER, cgi_handler_fs);

	// DATE_FORMAT.
	ASS_CONF_NEMPTY(DATE_FORMAT);

	// SCRIPT_BASE_DIR.
	ASS_CONF_NEMPTY(SCRIPT_BASE_DIR);

	char *restrict script_base_dir = NULL;		
	ASS_CANON(script_base_dir, SCRIPT_BASE_DIR);
	free(script_base_dir); script_base_dir = NULL;

	is_excl_owner_f(0, 0, SCRIPT_BASE_DIR, NULL);

	struct stat script_base_dir_fs;

	ASS_STAT(SCRIPT_BASE_DIR, &script_base_dir_fs);
	ASS_ISDIR(SCRIPT_BASE_DIR, script_base_dir_fs);
	ASS_UID(SCRIPT_BASE_DIR, script_base_dir_fs, 0);
	ASS_GID(SCRIPT_BASE_DIR, script_base_dir_fs, 0);
	ASS_NWOTH(SCRIPT_BASE_DIR, script_base_dir_fs);

	// SCRIPT_SUFFIX.
	ASS_CONF_NEMPTY(SCRIPT_SUFFIX);

	// SECURE_PATH.
	if (strlen(SECURE_PATH) > CR_SECURE_PATH_MAX)
		ERR_CONFIG("SECURE_PATH: is too long.");

	// WWW_USER.
	ASS_CONF_NEMPTY(WWW_USER);
	ASS_PORTNAME(WWW_USER);
	ASS_USER_EXISTS(pwd, WWW_USER);

	// WWW_GROUP.
	ASS_CONF_NEMPTY(WWW_GROUP);
	ASS_PORTNAME(WWW_GROUP);
	ASS_GROUP_EXISTS(grp, WWW_GROUP);

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
	ASS_STAT(prog_path, &prog_fs);
	ASS_ISREG(prog_path, prog_fs);
	ASS_UID(prog_path, prog_fs, 0);
	ASS_GID(prog_path, prog_fs, 0);
	ASS_NWOTH(prog_path, prog_fs);
	ASS_NXOTH(prog_path, prog_fs);


	/*
	 * Get script's path
	 * -----------------
	 */

	char *path_translated_e = NULL;
	path_translated_e = getenv_f("PATH_TRANSLATED");
	
	char *restrict script_path = NULL;
	ASS_CANON(script_path, path_translated_e);


	/*
	 * Check script's UID and GID
	 * --------------------------
	 */

	struct stat script_fs;
	ASS_STAT(script_path, &script_fs);
	ASS_ISREG(script_path, script_fs);
	ASSERT(script_fs.st_uid != 0,
	                "%s: UID is 0.", script_path);
	ASSERT(script_fs.st_gid != 0,
	                "%s: GID is 0.", script_path);
	ASSERT(script_fs.st_uid >= SCRIPT_MIN_UID &&
	                script_fs.st_uid <= SCRIPT_MAX_UID,
	                "%s: UID is privileged.", script_path);
	ASSERT(script_fs.st_gid >= SCRIPT_MIN_GID &&
	                script_fs.st_gid <= SCRIPT_MAX_GID,
	                "%s: GID is privileged.", script_path);

	ASS_UID_EXISTS(pwd, script_fs.st_uid);
	ASS_PORTNAME(pwd->pw_name);

	ASS_GID_EXISTS(grp, script_fs.st_gid);
	ASS_PORTNAME(grp->gr_name);

	ASSERT(script_fs.st_gid == pwd->pw_gid,
	       "%s: GID %d: not %s's primary group.",
	       script_path, script_fs.st_gid, pwd->pw_name);

	// `pwd` and `grp` are not cleared because
	// we might need them for `initgroups`
	// when dropping privileges. 


	/*
	 * Drop privileges
	 * ---------------
	 */

	// This section uses `setgroups` or `initgroups`,
	// neither of which is part of POSIX.1-2018.

	#ifdef NO_SETGROUPS
		if (initgroups(pwd->pw_name, script_fs.st_gid) != 0)
			ERR_OSERR("initgroups %s %d: %s",
			          pwd->pw_name, script_fs.st_gid,
	 		          strerror(errno));
	#else
		const gid_t groups[] = {};
		if (setgroups(0, groups) != 0)
			ERR_OSERR("setgroups 0: %s.", strerror(errno));
	#endif

	if (setgid(script_fs.st_gid) != 0)
		ERR_OSERR("setgid %d: %s.", script_fs.st_gid, strerror(errno));
	if (setuid(script_fs.st_uid) != 0)
		ERR_OSERR("setuid %s: %s.", script_fs.st_uid, strerror(errno));
	if (setuid(0) != -1)
		ERR_OSERR("setuid 0: %s.", strerror(errno));

	// Deferred from the previous section.
	pwd = NULL;
	grp = NULL;


	/*
	 * Check if run by webserver
	 * -------------------------
	 */

	uid_t prog_uid = getuid();
	if (prog_uid != www_uid)
		ERR_NOPERM("UID %d: not permitted.", prog_uid);

	gid_t prog_gid = getgid();
	if (prog_gid != www_gid)
		ERR_NOPERM("GID %d: not permitted.", prog_gid);


	/*
	 * Does PATH_TRANSLATED point to a safe file?
	 * ------------------------------------------
	 */

	is_subdir_f(script_path, SCRIPT_BASE_DIR);

	char *restrict home_dir = NULL;
	ASS_CANON(home_dir, pwd->pw_dir);
	free(home_dir); home_dir = NULL;

	is_subdir_f(script_path, pwd->pw_dir);

	char *document_root_e = NULL;
	document_root_e = getenv_f("DOCUMENT_ROOT");

	char *restrict document_root = NULL;
	ASS_CANON(document_root, document_root_e);
	is_subdir_f(script_path, document_root);
	free(document_root); document_root = NULL;

	is_excl_owner_f(script_fs.st_uid, script_fs.st_gid,
	                script_path, pwd->pw_dir);
	is_excl_owner_f(0, 0, pwd->pw_dir, NULL);

	ASS_NWOTH(script_path, script_fs);
	ASS_NSUID(script_path, script_fs);
	ASS_NSGID(script_path, script_fs);


	/*
	 * Does PATH_TRANSLATED point to a CGI script?
	 * -------------------------------------------
	 */

	char *suffix = strrchr(script_path, '.');
	ASSERT(suffix, "%s: has no filename ending.", script_path);
	ASSERT(STREQ(suffix, SCRIPT_SUFFIX),
	       "%s: does not end with \"%s\".",
	       script_path, SCRIPT_SUFFIX);


	/*
	 * Call CGI handler
	 * ----------------
	 */

	char *const args[] = { CGI_HANDLER, NULL };
	execve(CGI_HANDLER, args, environ);

	ERR_OSERR("execve %s: %s.", CGI_HANDLER, strerror(errno));
}
