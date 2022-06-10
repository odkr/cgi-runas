// Absolute path to a programme.
// Should run the file pointed to by the environment variable PATH_TRANSLATED.
// No operands or options are passed.
#define CGI_HANDLER "/usr/lib/cgi-bin/php"

// How to format timestamps in error messages.
// See strftime(3) for details.
#define DATE_FORMAT "%b %e %T"

// A user ID (UID).
// Script files are not executed if their UID is smaller than this UID.
// Low UIDs are typically used for system accounts.
#define SCRIPT_MIN_UID 1000

// A group ID (GID).
// Script files are not executed if their GID is smaller than this GID.
// Low GIDs are typically used for system accounts.
#define SCRIPT_MIN_GID 1000

// A user ID (UID).
// Script files are not executed if their UID is greater than this UID.
// High UIDs may be used for the 'nobody' user and daemons (e.g., libvirtd).
#define SCRIPT_MAX_UID 50000

// A group ID (GID).
// Script files are not executed if their GID is greater than this GID.
// High GIDs may be used for the 'nogroup' group and daemons (e.g., libvirtd).
#define SCRIPT_MAX_GID 50000

// A directory.
// Only scripts within that directory are run.
#define SCRIPT_BASE_DIR "/home"

// A filename suffix, including the leading dot.
// Scripts are only run if their filename ends with this suffix.
#define SCRIPT_SUFFIX ".php"

// A colon-separated list of directories.
// Overwrites the PATH environment variable.
// Should be set to a list of secure directories.
#define SECURE_PATH "/usr/bin:/bin"

// A username.
// Only processes running as this user may call cgi-runas.
// Should be set to the user your webserver runs as.
#define WWW_USER "www-data"

// A groupname.
// Only processes running as this group may call cgi-runas.
// Should be set to the group your webserver runs as.
#define WWW_GROUP "www-data"
