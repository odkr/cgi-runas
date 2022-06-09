# cgi-runas

A wrapper around CGI handlers that runs scripts under the UID and GID of their owner.


## Requirements

*cgi-runas* requires an operating system that:

1. complies with
   [POSIX](https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/),
2. supports the
   [setgroups](https://man7.org/linux/man-pages/man2/setgroups.2.html)
   system call, and
3. has a
   [proc](https://tldp.org/LDP/Linux-Filesystem-Hierarchy/html/proc.html)
   filesystem mounted on `/proc`.

Linux-based systems should meet those requirements.
*cgi-runas* has been tested on Debian GNU/Linux.

You also need a webserver, of course.


## Rationale

*cgi-runas* is intended to run PHP scripts under the UID and GID of their owner.
Therefore, the remaining documentation assumes PHP as CGI handler.

You need to install
[PHP as CGI handler](https://www.php.net/manual/en/install.unix.commandline.php)
for this to work.


## Operation

*cgi-runas* checks if the script file pointed to by the environment variable
`PATH_TRANSLATED` is secure, changes its effective UID and its effective GID to
the UID and the GID of the script's owner, cleans up the environment and then
executes the actual CGI handler.


## Security

**WARNING:**
*cgi-runas* is in need of real-world testing and an audit.
**You do *not* want to use it.**

*cgi-runas* performs the following checks to determine whether it should run a script:

1. Are `WWW_USER` and `WWW_GROUP` known to the system?
2. Is `cgi-runas` owned by UID 0 and `WWW_GROUP` and
   neither group-writable, nor world-writable, nor world-executable?
3. Is the directory `cgi-runas` is located in,
   and each of its parent directories,
   owned by root and neither group- nor world-writable?
4. Is `cgi-runas` run as `WWW_USER` and `WWW_GROUP`?
5. Does the value of `PATH_TRANSLATED` represent a canonical path?
6. Is the UID of the script's owner not 0, greater than `MIN_UID`,
   and known to the system?
7. Is the GID of the script's owner not 0, greater than `MIN_GID`,
   and known to the system?
8. Is the GID of the script's owner also the ID of the owner's primary group?
9. Can the executable drop all supplemantary groups,
   set its effective GID to the GID of the script's owner, and
   its effective UID to the UID of the script's owner?
10. After doing that, does resetting its effective UID to 0 fail?
11. Is the script located in `BASE_DIR`?
12. Is the script located in the home directory of its owner?
13. Is the directory the script is located in,
    and each of its parent directories up to the user's home directory,
	owned by the user the script should be run as and
	neither group- nor world-writable?
14. Is the parent directory of the user's home directory,
    and each of its parent directories,
    owned by root and neither group- nor world-writable?
15. Does the value of `PATH_TRANSLATED` end with `SCRIPT_SUFFIX`?

Unless all of the above conditions are met, *cgi-runas* aborts.

These checks emulate those of Apache's
[suExec](https://httpd.apache.org/docs/2.4/suexec.html).
Some checks go beyond what suExec does, some checks suExec performs were dropped.

*cgi-runas* also cleans up the process' environment:

* it only keeps environment variables listed in `ENV_VARS` and
* sets the environment variable `PATH` to the [configured](config.h) `PATH`.

The default for `ENV_VARS` has been adopted from Apache's suExec.

You will also want to have a look at the [PHP project's recommendations for securing
CGI-based setups](https://www.php.net/manual/en/security.cgi-bin.php).

## Installation 

You use *cgi-runas* at your own risk!
That risk is considerable!

----

Download the repository and unpack it.
<!--[latest release](https://github.com/odkr/cgi-runas/releases/latest)
and unpack it:

```sh
(
	url="https://github.com/odkr/cgi-runas/releases/download/v0.0.0/cgi-runas-0.0.0.tgz"
	curl --silent --show-error --location "$url"
	[ "$?" -eq 127 ] && wget --output-document=- "$url"
) | tar -xz
```
-->

----

Please take the time to read and evaluate the source code.

```sh
cd cgi-runas-0.0.0
less cgi-runas.c
```

----

*cgi-runas* is configured at compile-time. Don't worry, it compiles in < 1s.

Adapt [config.h](config.h):

```sh
"${VISUAL-${EDITOR-vi}}" config.h
```

If you are using Debian GNU/Linux and Apache and want to use *cgi-runas* for PHP,
the defaults should be fine.

----

Once you are done, compile *cgi-runas* by:

```sh
make
```

There should now be an executable `cgi-runas` in the repository's top-level directory:

```sh
$ ls cgi-runas
cgi-runas
```


----


Change `cgi-runas`'s owner and permissions by:

```sh
chown root:www-data cgi-runas
chmod u=rws,g=x,o= cgi-runas
```

If your web server does not run under the group 'www-data', you need to adapt the command above accordingly.


----

Move `cgi-runas` into your directory for CGI binaries (e.g., `/usr/lib/cgi-bin`).
You may want to rename it so that its name indicates which CGI handler it is running.

```sh
mv cgi-runas /usr/lib/cgi-bin/run-php-as
```

If your directory for CGI binaries is not `/usr/lib/cgi-bin`, you need to adapt the command above accordingly.

----

Tell your webserver to run scripts in `BASE_DIR` via *cgi-runas*.

If you are using [Apache](https://www.apache.org) v2, you can do so by:

```apacheconf
<Directory "/home">
        Action application/x-httpd-php /cgi-bin/run-php-as
</Directory>
```

If your `BASE_DIR` is not `/home`, you need to adapt the command above accordingly.
Note that `run-php-as` is the name that we have given `cgi-runas` above.


## Documentation

See the [source code](cgi-runas.c) and
the [configuration file](config.h) for more details.


## Contact

If there's something wrong with *cgi-runas*, please
[open an issue](https://github.com/odkr/cgi-runas/issues).


## License

Copyright 2022 Odin Kroeger

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


## Further Information

GitHub: <https://github.com/odkr/cgi-runas>
