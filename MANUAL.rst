NAME
====

**cgi-runas** - Run CGI scripts as their owner.


SYNOPSIS
========

**cgi-runas**


DESCRIPTION
===========

**cgi-runas** checks if the script file pointed to by the environment variable
**PATH_TRANSLATED** is secure, changes the process' effective UID and GID to
the UID and the GID of the script's owner, cleans up the environment, and
then executes the actual CGI handler.


CONFIGURATION
=============

**cgi-runas** is configured by adapting the header file *config.h*
before compilation.

**CGI_HANDLER**
	Path to a programme.
	Must be a canonical.
	Should run the file pointed to by the environment variable **PATH_TRANSLATED**.
	Given neither operands nor options.

**DATE_FORMAT**
	How to format timestamps in error messages.
	See strftime(3) for details.

**SCRIPT_MIN_UID**
	A user ID (UID).
	Script files are not executed if their UID is smaller than this UID.
	Low UIDs are typically used for system accounts.

**SCRIPT_MIN_GID**
	A group ID (GID).
	Script files are not executed if their GID is smaller than this GID.
	Low GIDs are typically used for system groups.

**SCRIPT_MAX_UID**
	A user ID (UID).
	Script files are not executed if their UID is greater than this UID.
	High UIDs may be used for the 'nobody' user and daemons (e.g., libvirtd).
	
**SCRIPT_MAX_GID**
	A group ID (GID).
	Script files are not executed if their GID is greater than this GID.
	High GIDs may be used for the 'nogroup' group and daemons (e.g., libvirtd).

**SCRIPT_BASE_DIR**
	A directory.
	Only scripts within that directory are run.

**SCRIPT_SUFFIX**
	A filename suffix, including the leading dot.
	Scripts are only run if their filename ends with this suffix.

**SECURE_PATH**
	A colon-separated list of directories to search for programmes.
	Overwrites the **PATH** environment variable.
	Should be set to a list of secure directories.

**WWW_USER**
	A username.
	Only processes running as this user may call **cgi-runas**.
	Should be set to the user your webserver runs as.

**WWW_GROUP**
	A groupname.
	Only processes running as this group may call **cgi-runas**.
	Should be set to the group your webserver runs as.

Just in case your C is rusty: ``#define`` statements are *not* terminated
with a semicolon; strings must be enclosed in double quotes ("..."), *not*
single quotes; and numbers must *not* be enclosed in quotes at all.


ENVIRONMENT
===========


**DOCUMENT_ROOT**
	The root directory of a website.
	Only scripts in this directory are executed.

**PATH**
	A search path.
	Overwritten with a safe value before **CGI_HANDLER** is called.

**PATH_TRANSLATED**
	Path of the script to run.
	Must be a canonical.


SECURITY
========

Untested and unaudited
----------------------

**cgi-runas** has *neither* been thoroughly tested, *nor* audited.
It has seen no real-world usage. You should assess the code yourself
and weigh the benefits of using **cgi-runas** against the risks.

Philosophy
----------

**cgi-runas** aims to comply with POSIX.1-2018 and extant best practices,
to improve upon prior art, above all, Apache's suExec, and to be secure
even in the face of user errors.

It implements most of suExec's safeguards, the exception being calling
**ufork** on systems that support it, but protects more thoroughly
against user errors and checks whether privileges can be re-gained.

Environment
-----------

Only safe environment variables are kept and **PATH** is set to the
**SECURE_PATH** given in *config.h* before calling **CGI_HANDLER**.

Checks
------

The following checks are performed before calling **CGI_HANDLER**.

Note that some of these checks are subject to race conditions; that is,
the system may change between the time they are perfomed and
the time **CGI_HANDLER** is called.

Configuration checks:

1. Is **CGI_HANDLER** set?
2. Does that file exist?
3. Is its path canonical?
4. Is its parent directory, the parent directory of that directory, and so on,
   owned by the superuser and the supergroup and *not* world-writable?
5. Is **CGI_HANDLER** itself a regular file?
6. Is it owned by the superuser and the supergroup?
7. Is it *not* world-writable?
8. Are its set-UID and set-GID bits unset?
9. And *is* it world-executable?
10. Are **SCRIPT_MIN_UID** and **SCRIPT_MAX_UID** set to a UID
    greater than 0 and smaller than or equal to the system's **MAX_UID**?
11. Is **SCRIPT_MIN_UID** smaller than **SCRIPT_MAX_UID**?
12. Are **SCRIPT_MIN_GID** and **SCRIPT_MAX_GID** set to a UID
    greater than 0 and smaller than or equal to the system's **MAX_GID**?
13. Is **SCRIPT_MIN_GID** smaller than **SCRIPT_MAX_GID**?
14. Is **SCRIPT_BASE_DIR** set?
15. Does that file exist?
16. Is its path canonical?
17. Is its parent directory, the parent directory of that directory, and so on,
    owned by the superuser and the supergroup and *not* world-writable?
18. Is **SCRIPT_BASE_DIR** itself a directory?
19. Is it owned by the superuser and the supergroup?
20. Is it *not* world-writable?
21. And *is* it world-executable?
22. Is **SCRIPT_SUFFIX** set?
23. Is **SECURE_PATH** path set?
24. Is it suspiciously long?
25. Is **WWW_USER** set?
26. Is the given username valid?
27. Does that user exist?
28. Is **WWW_GROUP** set?
29. Is the given groupname valid?
30. Does that group exist?

Self-checks:

1. Is the parent directory of **cgi-runas**,
   the parent directory of that directory, and so on,
   owned by the superuser and the supergroup and
   *not* world-writable?
2. Is **cgi-runas** itself owned by the superuser and **WWW_GROUP** and
   neither world-writable nor world-executable?

These checks won't be performed if an attacker succeeds in tricking you
to run another programme instead of **cgi-runas**, of course;
their purpose is to make sure that you secure your setup.

Permission checks:

Is **cgi-runas** run by **WWW_USER** and **WWW_GROUP**?

Script checks:

1. Is **PATH_TRANSLATED** set?
2. Does the script it points to exist?
3. Is its path canonical?
4. Is **DOCUMENT_ROOT** set?
5. Does the script it points to exist?
6. Is its path canonical?
7. Is that file in **SCRIPT_BASE_DIR**?
8. Is that file in the home directory of its owner?
9. Is that file in **DOCUMENT_ROOT**?
10. Is the script's parent directory,
    the parent directory of that directory, and so on,
    up to the home directory of the script's owner,
    owned by the script's owner and their primary group
    and *not* world-writable?
11. Is the parent directory of the script owner's home directory,
    the parent directory of that directory, and so on,
    owned by the superuser and the supergroup
    and *not* world-writable?
12. Is the script itself *not* world-writable?
13. Are its set-UID and set-GID bits unset?
14. Does its filename have a filename ending?
15. Does that ending equal **SCRIPT_SUFFIX**?

User and group checks:

1. Is the script file's UID greater than 0?
2. Is it a UID from **SCRIPT_MIN_UID** to **SCRIPT_MAX_UID**?
3. Does a user with that UID exist?
4. Is its name valid?
5. Is the script file's GID greater than 0?
6. Is it a GID from **SCRIPT_MIN_GID** to **SCRIPT_MAX_GID**?
7. Does a group with that GID exist?
8. Is its name valid?
9. Is it the primary group of the script file's owner?

Transition checks:

1. Was dropping the caller's supplementary groups successful?
2. Was setting the GID to that of the script file successful?
3. Was setting the UID to that of the script file successful?
4. Did trying to reset the UID to that of the superuser fail?

CGI
---

You should also consider the `security issues that running PHP as a CGI
handler <https://www.php.net/manual/en/security.cgi-bin.php>`_ implies.


DIAGNOSTICS
===========

**cgi-runas** prints errors, and only errors, to STDERR.
You need to set up the webserver so that it logs them.


EXIT STATUSES
=============

64
	Usage error.

67
	User or group not found.

69
	Any other error.

70
	Bug.

71
	Operating system error.

77
	Permission denied.

78
	Configuration error.


AUTHOR
======

Odin Kroeger
