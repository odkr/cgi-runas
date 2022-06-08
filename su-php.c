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

#define SELF "/proc/self/exe"


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
	// Prelude.
	errno = 0;

	// Get the path of the executable.
	char *PROG_PATH = malloc(PATH_MAX + 1);
	ssize_t b = readlink(SELF, PROG_PATH, PATH_MAX);
	if      (b < -1)
		panic(71, "failed to resolve %s.", SELF);
	else if (b == -1)
		panic(71, "readlink %s: %s.", SELF, strerror(errno));
	else if (b == 0)
		panic(71, "link %s resolves to nothing.", SELF);
	else if (b == PATH_MAX)
		panic(69, "link %s resolves to overly long path.", SELF);
	PROG_NAME = basename(PROG_PATH);

	// Check if the executable is secure.
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

	// Check if the programme is run by the web server.
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

	// Get the script's path.
	char *trans = NULL;
	trans = getenv("PATH_TRANSLATED");
	if (trans == NULL)
		panic(64, "PATH_TRANSLATED is not set.");
	if (strcmp(trans, "") == 0)
		panic(64, "PATH_TRANSLATED is empty.");
	if (trans[0] != '/')
		panic(64, "%s is not an absolute path.", trans);
	
	char *path = NULL;
	path = realpath(trans, NULL);
	if (!path)
		panic(69, "failed to canonicalise %s: %s.", trans, strerror(errno));
	free(trans); trans = NULL;

	// Check if the script's UID and GID are sound.
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
	
	// Drop privileges.
	const gid_t groups[] = {};
	if (setgroups(0, groups) != 0)
		panic(69, "failed to drop groups: %s.", strerror(errno));
	if (setgid(gid) != 0)
		panic(69, "failed to set group ID: %s.", strerror(errno));
	if (setuid(uid) != 0)
		panic(69, "failed to set user ID: %s.", strerror(errno));
	if (setuid(0) != -1)
		panic(69, "could regain privileges, aborting.");

	// Check if we are dealing with a PHP script.
	char *suffix = NULL;
	suffix = strrchr(path, '.');
	if (!suffix)
		panic(64, "%s has no filename ending.", path);
	if (strcmp(suffix, ".php") != 0)
		panic(64, "%s is not a PHP script.", path);

	// Check if the PHP script's path is sound.
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

	// Check if the PHP script can be modified by other users.
	assert_secure_path(path, uid, gid);
	free(path); path = NULL;

	// Clean up the environment.
	int i = 0;
	int j = 0;
	for (i = 0; environ[i]; i++) {
		char *pair = environ[i];
		int safe = 0;
		for (j = 0; SAFE_ENV_VARS[j]; j++) {
			char *pattern = SAFE_ENV_VARS[j];
			int len = strlen(pattern);
			char *prefix = malloc(len + 1);
			strncpy(prefix, pair, len);
			if (strcmp(prefix, pattern) == 0) {
				safe = 1;
				break;
			}
			free(prefix); prefix = NULL;
		}
		if (safe != 1) {
			char *name = strtok(pair, "=");
			if (!name)
				panic(69, "environment key-value pair: %s: failed to parse.", pair);
			if(unsetenv(name) != 0)
				panic(69, "failed to unset %s: %s.", name, strerror(errno));
		}
	}

	if (setenv("PATH", PATH, 1) != 0)
		panic(69, "failed to set PATH: %s.", strerror(errno));



	// Call the actual CGI handler.
	char *const a[] = { PHP_CGI, NULL };
	execve(PHP_CGI, a, environ);
}
