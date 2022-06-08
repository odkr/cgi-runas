// PHP scripts outside of this directory are rejected.
const char BASE_DIR[] = "/home";

// PHP scripts owned by users with a UID lower than this one are rejected.
const uid_t MIN_UID = 1000;

// PHP scripts owned by groups with a GID lower than this one are rejected.
const gid_t MIN_GID = 1000;

// What to set the `PATH` environment variable to.
const char PATH[] = "/usr/bin:/bin";

// The absolute path of the CGI handler to run PHP scripts with.
const char PHP_CGI[] = "/usr/lib/cgi-bin/php";

// A list of safe environment variables.
// Must be terminated with a `NULL`.
// Copied from Apache's suExec.
const char *const ENV_VARS[] =
{
	// If you do not terminate variable names with a "=",
	// they match the begining of the variable name.
	"HTTP_",
	"SSL_",

	// If you terminate variable names with a "=",
	// they match the whole variable name.
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

// The user the web server runs as.
const char WWW_USER[] = "www-data";

// The group the web server runs as.
const char WWW_GROUP[] = "www-data";