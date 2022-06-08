/* PROGRAMME: su-php
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

// Needed to find the executable.
#define PROC_SELF_EXE "/proc/self/exe"


/*
 * GLOBALS
 * =======
 */

/* Global: environ
 *
 * The environemnt.
 */
extern char **environ;

/* Global: PROG_PATH
 *
 * The path to the programme's executable.
 * Set by `main`.
 */
char *PROG_PATH = NULL;

/* Global: PROG_NAME
 *
 * The filename of the programme's executable.
 * Set by `main`.
 */ 
char *PROG_NAME = NULL;


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
 *    PROG_NAME - The filename of the executable.
 *                If not `NULL`, `PROG_NAME`, a colon, and a space
 *                are printed before the message.
 */
void
panic (const int status, const char *message, ...)
{
	if (PROG_NAME) fprintf(stderr, "%s: ", PROG_NAME);
	va_list argp;
	va_start(argp, message);
	vfprintf(stderr, message, argp);
	va_end(argp);
	fprintf(stderr, "\n");
	exit(status);
}

/* Function: assert_secure_location
 *
 * Abort the programme with status 69 and an error message
 * if a file, or one of its parent directories, can be modified by
 * somebody other than root and a given user.
 *
 * Arguments:
 *     path - The file's path.
 *     uid  - The user's UID.
 *     gid  - The GID of that user's primary group.
 */
void
assert_secure_location (char *path, uid_t uid, gid_t gid)
{
	struct stat fs;
	char *ptr = malloc(strlen(path) + 1);
	char *p = NULL;
	strcpy(ptr, path);
	p = ptr;
	do {
		if (stat(p, &fs) != 0)
			panic(69, "%s: %s.", p, strerror(errno));
		if (fs.st_uid != 0 && fs.st_uid != uid)
			panic(69, "%s's UID is insecure.", p);
		if (fs.st_gid != 0 && fs.st_gid != gid)
			panic(69, "%s's GID is insecure.", p);
		if (fs.st_mode & S_IWOTH)
			panic(69, "%s is world-writable.", p);
		p = dirname(p);
	} while ((strcmp(p, "/") != 0 && strcmp(p, ".") != 0));
	free(ptr);
}

/* Function: starts_with
 *
 * Check if a string starts with a substring.
 *
 * Arguments:
 *     str - A string.
 *     sub - A substring.
 *
 * Returns:
 *     1 - The string starts with the given substring.
 *     0 - Otherwise.
 */
int
starts_with (char *str, char *sub)
{
	int ret = -1;
	int len = strlen(sub);
	char *tmp = malloc(len + 1);
	strncpy(tmp, str, len);
	tmp[len] = '\0';
	ret = strcmp(tmp, sub);
	free(tmp);
	if (ret == 0) return 1;
	return 0;
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

	// PATH_MAX does not do what it should be doing, but what is the alternative?
	// See <https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html>.
	// The '+ 1' should be superfluous, but better be save than sorry.

	PROG_PATH = malloc(PATH_MAX + 1);
	ssize_t bytes = readlink(PROC_SELF_EXE, PROG_PATH, PATH_MAX);
	if      (bytes < 0)
		panic(71, "readlink %s: %s.", PROC_SELF_EXE, strerror(errno));
	else if (bytes == 0)
		panic(71, "link %s resolves to nothing.", PROC_SELF_EXE);
	else if (bytes >= PATH_MAX)
		panic(69, "link %s resolves to overly long path.", PROC_SELF_EXE);
	PROG_NAME = basename(PROG_PATH);


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

	if (stat(PROG_PATH, &fs) != 0)
		panic(69, "%s: %s.", PROG_PATH, strerror(errno));

	if (fs.st_uid != 0)
		panic(69, "%s's UID is not 0.", PROG_PATH);
	if (fs.st_gid != grp->gr_gid)
		panic(69, "%s's GID is not %d.", PROG_PATH, grp->gr_gid);
	if (fs.st_mode & S_IWGRP)
		panic(69, "%s is group-writable.", PROG_PATH);
	if (fs.st_mode & S_IWOTH)
		panic(69, "%s is world-writable.", PROG_PATH);
	if (fs.st_mode & S_IXOTH)
		panic(69, "%s is world-executable.", PROG_PATH);

	assert_secure_location(PROG_PATH, 0, grp->gr_gid);


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
		panic(77, "can only be called by user %s.", WWW_USER);

	gid_t prog_gid;
	prog_gid = getgid();
	grp = getgrgid(prog_gid);
	if (!grp)
		panic(71, "GID %d: no such group.", prog_gid);
	if (strcmp(grp->gr_name, WWW_GROUP) != 0)
		panic(77, "can only be called by group %s.", WWW_GROUP);


	/*
	 * Get script's path
	 * -----------------
	 */

	// This section depends on /proc.
	// There is no other, reliable, way to find a process' executable.

	char *trans = NULL;
	trans = getenv("PATH_TRANSLATED");
	if (trans == NULL)
		panic(64, "PATH_TRANSLATED is not set.");
	if (strcmp(trans, "") == 0)
		panic(64, "PATH_TRANSLATED is empty.");
	if (trans[0] != '/')
		panic(64, "%s is not an absolute path.", trans);
	
	char *restrict path = NULL;
	path = realpath(trans, NULL);
	if (!path)
		panic(69, "failed to canonicalise %s: %s.", trans, strerror(errno));
	if (strlen(path) >= PATH_MAX)
		panic(69, "PATH_TRANSLATED is too long.");


	/*
	 * Check script's UID and GID
	 * --------------------------
	 */

	if (stat(path, &fs) != 0)
		panic(69, "%s: %s.", path, strerror(errno));

	if (fs.st_uid == 0)
		panic(69, "%s's UID is 0.", path);
	if (fs.st_gid == 0)
		panic(69, "%s's GID is 0.", path);
	if (fs.st_uid < MIN_UID)
		panic(69, "%s's UID is privileged.", path);
	if (fs.st_gid < MIN_GID)
		panic(69, "%s's GID is privileged.", path);

	pwd = getpwuid(fs.st_uid);
	if (!pwd)
		panic(67, "UID %d: no such user.", fs.st_uid);
	grp = getgrgid(fs.st_gid);
	if (!grp)
		panic(67, "GID %d: no such group.", fs.st_uid);

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
	 * Does PATH_TRANSLATED point to a PHP script?
	 * -------------------------------------------
	 */

	char *suffix = NULL;
	suffix = strrchr(path, '.');
	if (!suffix)
		panic(64, "%s has no filename ending.", path);
	if (strcmp(suffix, ".php") != 0)
		panic(64, "%s is not a PHP script.", path);


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
	if (!starts_with(path, base_dir))
		panic(69, "%s is not in %s.", path, BASE_DIR);
	free(base_dir); base_dir = NULL;

	assert_secure_location(path, uid, gid);
	free(path); path = NULL;


	/*
	 * Clean up the environment
	 * ------------------------
	 */

	int i, j;
	for (i = 0; environ[i]; i++) {
		char *pair = environ[i];
		int safe = 0;
		for (j = 0; ENV_VARS[j]; j++) {
			if (starts_with(pair, ENV_VARS[j])) {
				safe = 1;
				break;
			}
		}
		if (safe != 1) {
			char *name = strtok(pair, "=");
			if (!name)
				panic(69, "%s: failed to parse.", pair);
			if(unsetenv(name) != 0)
				panic(69, "failed to unset %s: %s.", name, strerror(errno));
		}
	}

	if (setenv("PATH", PATH, 1) != 0)
		panic(69, "failed to set PATH: %s.", strerror(errno));


	/* 
	 * Call PHP
	 * --------
	 */

	char *const argv[] = { PHP_CGI, NULL };
	execve(PHP_CGI, argv, environ);
}
