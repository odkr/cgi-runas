/*
 *
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

#include <config.h>

#define PROC_SELF_EXE "/proc/self/exe"


// GLOBALS
// =======

// The environment.
extern char **environ;

// The path to this script.
// Set by `main`.
char *PROG_PATH = NULL;

// The name of this programme.
// Set by `main`.
char *PROG_NAME = NULL;


// FUNCTIONS
// =========


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

void
assert_secure_path (char *path, uid_t uid, gid_t gid)
{
	struct stat fs;
	char *ptr = malloc(strlen(path));
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
	free(ptr); ptr = NULL; p = NULL;
}


// MAIN
// ====

int
main (int argc, char *const argv[])
{
	/* Prelude.
	 * -------- */
	errno = 0;


	/* Get path and name of executable.
	 * -------------------------------- */

	// PATH_MAX does not do what it should be doing, but what is the alternative?
	// See <https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html>.
	// The '+ 1' should be superflous, but better be save than sorry.

	PROG_PATH = malloc(PATH_MAX + 1);
	ssize_t bytes = readlink(PROC_SELF_EXE, PROG_PATH, PATH_MAX);
	if      (bytes < 0)
		panic(71, "readlink %s: %s.", PROC_SELF_EXE, strerror(errno));
	else if (bytes == 0)
		panic(71, "link %s resolves to nothing.", PROC_SELF_EXE);
	else if (bytes == PATH_MAX)
		panic(69, "link %s resolves to overly long path.", PROC_SELF_EXE);
	PROG_NAME = basename(PROG_PATH);


	/* Check if executable is secure.
	 * ------------------------------ */

	// If it were insecure, this checks wouldn't be run to begin with, of course.
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

	assert_secure_path(PROG_PATH, 0, grp->gr_gid);


	/* Check if programme is run by webserver.
	 * --------------------------------------- */

	uid_t prog_uid;
	prog_uid = getuid();
	pwd = getpwuid(prog_uid);
	if (!pwd)
		panic(71, "UID %s: no such user.", prog_uid);
	if (strcmp(pwd->pw_name, WWW_USER) != 0)
		panic(77, "can only be called by user %s.", WWW_USER);
	gid_t prog_gid;
	prog_gid = getgid();
	grp = getgrgid(prog_gid);
	if (!grp)
		panic(71, "GID %s: no such group.", prog_gid);
	if (strcmp(grp->gr_name, WWW_GROUP) != 0)
		panic(77, "can only be called by group %s.", WWW_GROUP);


	/* Get script's path.
	 * ------------------ */

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


	/* Check if script's UID and GID are sound.
	 * ---------------------------------------- */

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
	

	/* Drop privileges.
	 * ---------------- */
	
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


	/* Check if PATH_TRANSLATED points to a PHP script.
	 * ------------------------------------------------ */

	char *suffix = NULL;
	suffix = strrchr(path, '.');
	if (!suffix)
		panic(64, "%s has no filename ending.", path);
	if (strcmp(suffix, ".php") != 0)
		panic(64, "%s is not a PHP script.", path);


	/* Check if PATH_TRANSLATED points to a secure file.
	 * ------------------------------------------------- */

	int base_len = strlen(BASE_DIR) + 1;
	char *base_dir = malloc(base_len);
	strcpy(base_dir, BASE_DIR);
	strcat(base_dir, "/");
	char *prefix = malloc(base_len);
	strncpy(prefix, path, base_len);
	if (strcmp(base_dir, prefix) != 0)
		panic(69, "%s is not in %s.", path, BASE_DIR);
	free(prefix); prefix = NULL;
	free(base_dir); base_dir = NULL;

	assert_secure_path(path, uid, gid);
	free(path); path = NULL;


	/* Clean up the environment.
	 * ------------------------- */

	int i, j;
	for (i = 0; environ[i]; i++) {
		char *pair = environ[i];
		int safe = 0;
		for (j = 0; SAFE_ENV_VARS[j]; j++) {
			char *pattern = SAFE_ENV_VARS[j];
			int len = strlen(pattern);
			// prefix is defined above.
			prefix = malloc(len + 1);
			strncpy(prefix, pair, len);
			prefix[len] = '\0';
			if (strcmp(prefix, pattern) == 0) {
				safe = 1;
				break;
			}
			free(prefix); prefix = NULL;
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


	/* Call the actual CGI handler.
	 * ---------------------------- */

	char *const a[] = { PHP_CGI, NULL };
	execve(PHP_CGI, a, environ);
}
