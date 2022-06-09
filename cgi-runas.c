/*
 * Programme: cgi-runas
 *
 * Run PHP scripts under the UID and GID of their owner.
 *
 * See <https://https://github.com/odkr/su-php> for details.
 *
 * Copyright 2022 Odin Kroeger
 *
 * This program is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
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

// How deeply path may be nexted.
#define MAX_PATH_DEPTH 128

// Needed to find the executable.
#define EXE "/proc/self/exe"


/*
 * GLOBALS
 * =======
 */

/*
 * Global: environ
 *
 * The environemnt.
 */
extern char **environ;

/* 
 * Global: prog_path
 *
 * The path to the programme's executable.
 * Set by `main`.
 */
char *prog_path = NULL;

/* 
 * Global: prog_name
 *
 * The filename of the programme's executable.
 * Set by `main`.
 */ 
char *prog_name = NULL;


/*
 * DATA TYPES
 * ==========
 */

/* 
 * Type: struct list
 *
 * A simple linked list.
 *
 * See also:
 *
 *    - <push>.
 */
struct list {
	void *item;
	struct list *prev;
};


/*
 * FUNCTIONS
 * =========
 */


/* Function: panic
 *
 * Prints an error message to STDERR and exits the programme.
 *
 * Arguments:
 * 
 *    status  - Status to exit with.
 *    message - Message to print.
 *    ...     - Arguments for the message (think `printf`).
 *
 * Globals:
 *
 *    prog_name - The filename of the executable.
 *                If not `NULL`, `prog_name`, a colon, and a space
 *                are printed before the message.
 */
void
panic (const int status, const char *message, ...)
{
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
 * Append an element to a list.
 *
 * Arguments:
 *
 *    head - A pointer to a linked list.
 *    item - A pointer to an item.
 *
 *
 * Returns:
 *
 *    1 - On success.
 *    0 - On failure.
 *        If <errno> is set, <malloc> failed.
 *        Otherwise, `head` evaluates as false.
 *
 * Caveats:
 *
 *    The list is modified inplace and the pointer to the list is moved forward!
 *
 *
 * Example:
 *
 *    === C ===
 *    struct list *foo = NULL;
 *    char *bar = "bar";
 *    push(&foo, bar);
 *
 *    struct list *idx = foo;
 *    while (idx) {
 *        printf("%s\n", idx->item);
 *        idx = idx->prev;
 *    }
 *    =========
 */
int
push (struct list **head, void *item)
{
	if (!head) return 0;
	struct list *next = malloc(sizeof(struct list));
	if (!next) return 0;
	next->prev = *head;
	next->item = item;
	*head = next;
	return 1;
}

/*
 * Function: parent_dirs
 *
 * The parent directory of a file, the parent directory of that directory, ...,
 * and so on up to the root directory ("/"), a relative directory root ("."),
 * or a given directory.
 * 
 *
 * Argument:
 *
 *    start - A file.
 *    stop  - Directory at which to stop.
 *            Set to `NULL` to traverse up to "/" or ".".
 *
 * Returns:
 *
 *    A list of directories or `NULL` on failure.
 *    If `errno` is not set on failure, then the list's head was NULL.
 *
 *
 * Caveats:
 *
 *    - Memory allocated to directories must be freed manually.
 *    - The paths given should be canonical.
 *
 * Example:
 *
 *    === C ===
 *    struct list *dirs = *parent_dirs("/some/dir", NULL);
 *    if (!dirs) {
 *        if (errno) panic(71, strerror(errno));
 *        else panic(78, "list head is NULL, this is a bug.")
 *    }
 *    struct list *idx = dirs;
 *    while (idx) {
 *        printf("%s\n", idx->item);
 *        idx = idx->prev;
 *    }
 *    =========
 */
struct list
*parent_dirs (char *start, char *stop)
{
	struct list *dirs = NULL;
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
		(!stop || strcmp(dir, stop) != 0) &&
		strcmp(dir, "/") != 0 && 
		strcmp(dir, ".") != 0
	);
	
	return dirs;
}


/*
 * MAIN
 * ====
 */

int
main ()
{
	/*
	 * Prelude
	 * -------
	 */
	errno = 0;


	/*
	 * Self-discovery
	 * --------------
	 */

	// This section depends on /proc.
	// There is no other, reliable, way to find a process' executable.

	// PATH_MAX does not do what it should be doing, but what is the alternative?
	// See <https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html>.
	// The '+ 1' should be superfluous, but better be save than sorry.

	prog_path = malloc(PATH_MAX + 1);
	ssize_t bytes = readlink(EXE, prog_path, PATH_MAX);
	if (bytes < 0)
		panic(71, "readlink %s: %s.", EXE, strerror(errno));
	else if (bytes == 0)
		panic(71, "link %s: resolves to nothing.", EXE);
	else if (bytes >= PATH_MAX)
		panic(69, "link %s: resolves to overly long path.", EXE);
	prog_name = basename(prog_path);
	

	/*
	 * Check location security
	 * -----------------------
	 */

	// If it were insecure, these checks wouldn't be run to begin with, of course.
	// Their purpose is to force the user to secure their setup.

	struct stat fs;
	struct group *grp;
	struct passwd *pwd;

	pwd = getpwnam(WWW_USER);
	if (!pwd)
		panic(67, "%s: no such user.", WWW_USER);

	grp = getgrnam(WWW_GROUP);
	if (!grp)
		panic(67, "%s: no such group.", WWW_GROUP);

	if (stat(prog_path, &fs) != 0)
		panic(69, "%s: %s.", prog_path, strerror(errno));

	if (fs.st_uid != 0)
		panic(69, "%s: UID is not 0.", prog_path);
	if (fs.st_gid != grp->gr_gid)
		panic(69, "%s: GID is not %d.", prog_path, grp->gr_gid);
	if (fs.st_mode & S_IWGRP)
		panic(69, "%s: is group-writable.", prog_path);
	if (fs.st_mode & S_IWOTH)
		panic(69, "%s: is world-writable.", prog_path);
	if (fs.st_mode & S_IXOTH)
		panic(69, "%s: is world-executable.", prog_path);
	
	struct list *dirs = parent_dirs(prog_path, NULL);
	if (!dirs) {
		if (errno) panic(71, strerror(errno));
		else panic(78, "list head is NULL, this is a bug.");
	}
	
	struct list *idx = dirs;
	struct list *prv = NULL;
	while (idx) {
		char *dir = idx->item;
		prv = idx->prev;
		if (stat(dir, &fs) != 0)
			panic(69, "%s: %s.", dir, strerror(errno));
		if (fs.st_uid != 0)
			panic(69, "%s: not owned by UID 0.", dir);
		if (fs.st_mode & S_IWGRP)
			panic(69, "%s: is group-writable.", dir);
		if (fs.st_mode & S_IWOTH)
			panic(69, "%s: is world-writable.", dir);
		free(dir);
		free(idx);
		idx = prv;
	}
	dirs = NULL;

	// char *ptr = NULL;
	// if(!(*ptr = strdup(prog_path)))
	//		panic(71, strerror(errno));
	// char *p = *ptr;
	// do {
	// 	if (stat(p, &fs) != 0)
	// 		panic(69, "%s: %s.", p, strerror(errno));
	// 	if (fs.st_uid != 0)
	// 		panic(69, "%s: not owned by UID 0.", p);
	// 	if (fs.st_mode & S_IWGRP)
	// 		panic(69, "%s is group-writable.", p);
	// 	if (fs.st_mode & S_IWOTH)
	// 		panic(69, "%s is world-writable.", p);
	// 	p = dirname(p);
	// } while ((strcmp(p, "/") != 0 && strcmp(p, ".") != 0));
	// free(ptr); ptr = NULL;


	/*
	 * Check if run by webserver
	 * -------------------------
	 */

	uid_t prog_uid;
	prog_uid = getuid();
	pwd = getpwuid(prog_uid);
	if (!pwd)
		panic(71, "UID %d: no such user.", prog_uid);
	if (strcmp(pwd->pw_name, WWW_USER) != 0)
		panic(77, "must be called by user %s.", WWW_USER);

	gid_t prog_gid;
	prog_gid = getgid();
	grp = getgrgid(prog_gid);
	if (!grp)
		panic(71, "GID %d: no such group.", prog_gid);
	if (strcmp(grp->gr_name, WWW_GROUP) != 0)
		panic(77, "must be called by group %s.", WWW_GROUP);


	/*
	 * Get script's path
	 * -----------------
	 */

	char *trans = NULL;
	trans = getenv("PATH_TRANSLATED");
	if (trans == NULL)
		panic(64, "PATH_TRANSLATED is not set.");
	if (strcmp(trans, "") == 0)
		panic(64, "PATH_TRANSLATED is empty.");

	char *restrict path = NULL;
	path = realpath(trans, NULL);
	if (!path)
		panic(69, "failed to canonicalise %s: %s.", trans, strerror(errno));
	if (strlen(path) >= PATH_MAX)
		panic(69, "PATH_TRANSLATED is too long.");
	if (strcmp(trans, path) != 0)
		panic(69, "%s: not a canonical path.", trans);


	/*
	 * Check script's UID and GID
	 * --------------------------
	 */

	if (stat(path, &fs) != 0)
		panic(69, "%s: %s.", path, strerror(errno));

	if (fs.st_uid == 0)
		panic(69, "%s: UID is 0.", path);
	if (fs.st_gid == 0)
		panic(69, "%s: GID is 0.", path);
	if (fs.st_uid < MIN_UID)
		panic(69, "%s: UID is privileged.", path);
	if (fs.st_gid < MIN_GID)
		panic(69, "%s: GID is privileged.", path);

	pwd = getpwuid(fs.st_uid);
	if (!pwd)
		panic(67, "UID %d: no such user.", fs.st_uid);
	grp = getgrgid(fs.st_gid);
	if (!grp)
		panic(67, "GID %d: no such group.", fs.st_uid);
	if (fs.st_gid != pwd->pw_gid)
		panic(67, "GID %d: not %s's GID.", pwd->pw_name);

	uid_t uid = fs.st_uid;
	gid_t gid = fs.st_gid;


	/*
	 * Drop privileges
	 * ---------------
	 */

	// This section uses setgroups(), which is non-POSIX.
	// I drop privileges earlier than suExec, because
	// the fewer privileges, the better. Why wait?

	const gid_t groups[] = {};
	if (setgroups(0, groups) != 0)
		panic(69, "failed to drop groups: %s.", strerror(errno));
	if (setgid(gid) != 0)
		panic(69, "failed to set group ID: %s.", strerror(errno));
	if (setuid(uid) != 0)
		panic(69, "failed to set user ID: %s.", strerror(errno));
	if (setuid(0) != -1)
		panic(69, "could regain privileges, aborting.");


	/*
	 * Does PATH_TRANSLATED point to a secure file?
	 * --------------------------------------------
	 */

	char *restrict base_dir = realpath(BASE_DIR, NULL);
	if (!base_dir)
		panic(69, "failed to canonicalise %s: %s.", BASE_DIR, strerror(errno));
	if (strcmp(BASE_DIR, base_dir) != 0)
		panic(69, "%s: not a canonical path.", BASE_DIR);
	if (strlen(base_dir) >= PATH_MAX)
		panic(69, "path of base directory is too long.");
	strcat(base_dir, "/");
	if (strncmp(path, BASE_DIR, strlen(BASE_DIR)) != 0)
		panic(69, "%s: not in %s.", path, BASE_DIR);
	free(base_dir); base_dir = NULL;

	char *restrict home_dir = realpath(pwd->pw_dir, NULL);
	if (!home_dir)
		panic(69, "failed to canonicalise %s: %s.", pwd->pw_dir, strerror(errno));
	if (strcmp(pwd->pw_dir, home_dir) != 0)
		panic(69, "%s: not a canonical path.", pwd->pw_dir);
	if (strlen(home_dir) >= PATH_MAX)
		panic(69, "path of %s's home directory is too long.");
	strcat(home_dir, "/");
	
	if (strncmp(path, home_dir, strlen(home_dir)) != 0)
		panic(69, "%s: not in %s.", path, home_dir);

	*dirs = *parent_dirs(path, home_dir);
	if (!dirs) {
		if (errno) panic(71, strerror(errno));
		else panic(78, "list head is NULL, this is a bug.");
	}
	
	*idx = *dirs;
	prv = NULL;
	while (idx) {
		char *dir = idx->item;
		prv = idx->prev;
		if (stat(dir, &fs) != 0)
			panic(69, "%s: %s.", dir, strerror(errno));
		if (fs.st_uid != uid)
			panic(69, "%s: not owned by UID %d.", dir, uid);
		if (fs.st_gid != gid)
			panic(69, "%s: not owned by GID %d.", dir, gid);
		if (fs.st_mode & S_IWGRP)
			panic(69, "%s: is group-writable.", dir);
		if (fs.st_mode & S_IWOTH)
			panic(69, "%s: is world-writable.", dir);
		free(dir);
		free(idx);
		idx = prv;
	}
	dirs = NULL;

	*dirs = *parent_dirs(home_dir, NULL);
	if (!dirs) {
		if (errno) panic(71, strerror(errno));
		else panic(78, "list head is NULL, this is a bug.");
	}

	*idx = *dirs;
	prv = NULL;
	while (idx) {
		char *dir = idx->item;
		prv = idx->prev;
		if (stat(dir, &fs) != 0)
			panic(69, "%s: %s.", dir, strerror(errno));
		if (fs.st_uid != 0)
			panic(69, "%s: not owned by UID 0.", dir);
		if (fs.st_gid != 0)
			panic(69, "%s: not owned by GID 0.", dir);
		if (fs.st_mode & S_IWGRP)
			panic(69, "%s: is group-writable.", dir);
		if (fs.st_mode & S_IWOTH)
			panic(69, "%s: is world-writable.", dir);
		free(dir);
		free(idx);
		idx = prv;
	}
	dirs = NULL;

	//
	// p = path;
	// do {
	// 	if (stat(p, &fs) != 0)
	// 		panic(69, "%s: %s.", p, strerror(errno));
	// 	if (fs.st_uid != uid)
	// 		panic(69, "%s: not owned by UID %d.", p, uid);
	// 	if (fs.st_gid != gid)
	// 		panic(69, "%s: not owned by GID %d.", p, gid);
	// 	if (fs.st_mode & S_IWGRP)
	// 		panic(69, "%s is group-writable.", p);
	// 	if (fs.st_mode & S_IWOTH)
	// 		panic(69, "%s is world-writable.", p);
	// 	if (strcmp(p, home_dir) != 0) break;
	// 	p = dirname(p);
	// } while (strcmp(p, "/") != 0 && strcmp(p, ".") != 0);
	// free(path); path = NULL;
	//
	// p = dirname(home_dir);
	// do {
	// 	if (stat(p, &fs) != 0)
	// 		panic(69, "%s: %s.", p, strerror(errno));
	// 	if (fs.st_uid != 0)
	// 		panic(69, "%s: not owned by UID 0.", p);
	// 	if (fs.st_gid != 0)
	// 		panic(69, "%s: not owned by GID 0.", p);
	// 	if (fs.st_mode & S_IWGRP)
	// 		panic(69, "%s is group-writable.", p);
	// 	if (fs.st_mode & S_IWOTH)
	// 		panic(69, "%s is world-writable.", p);
	// 	p = dirname(p);
	// } while ((strcmp(p, "/") != 0 && strcmp(p, ".") != 0));
	// free(home_dir); home_dir = NULL;


	/*
	 * Does PATH_TRANSLATED point to a PHP script?
	 * -------------------------------------------
	 */

	char *suffix = NULL;
	suffix = strrchr(path, '.');
	if (!suffix)
		panic(64, "%s: has no filename ending.", path);
	if (strcmp(suffix, SCRIPT_SUFFIX) != 0)
		panic(64, "%s: does not end with \"%s\".", path, SCRIPT_SUFFIX);


	/*
	 * Clean up the environment
	 * ------------------------
	 */

	while (*environ) {
		const char *const *pattern = ENV_VARS;
		int safe = 0;
		while (*pattern) {
			if (strncmp(*environ, *pattern, strlen(*pattern)) == 0) {
				safe = 1;
				break;
			}
			pattern++;
		}
		if (safe != 1) {
			// strtok moves the pointer, but that doesn't matter here.
			char *name = strtok(*environ, "=");
			if (!name)
				name = *environ;
			if (unsetenv(name) != 0)
				panic(69, "failed to unset %s: %s.", name, strerror(errno));
		}
		*environ++;
	}

	if (setenv("PATH", PATH, 1) != 0)
		panic(69, "failed to set PATH: %s.", strerror(errno));


	/*
	 * Call CGI handler
	 * ----------------
	 */
	
	char *const argv[] = { CGI_HANDLER, NULL };
	execve(CGI_HANDLER, argv, environ);
}
