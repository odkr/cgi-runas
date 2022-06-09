# su-php

A wrapper around PHP that runs PHP scripts under the UID and GID of their owner.


## Requirements

*su-php* requires an operating system that:

1. complies with
   [POSIX](https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/),
2. supports the
   [setgroups](https://man7.org/linux/man-pages/man2/setgroups.2.html)
   system call, and
3. has a
   [proc](https://tldp.org/LDP/Linux-Filesystem-Hierarchy/html/proc.html)
   filesystem mounted on `/proc`.

Most Linux distributions should meet those requirements.
*su-php* has been tested on Debian GNU/Linux.

You also need a webserver and [PHP](https://php.net/), of course.
And you need to run PHP via the CGI in order for *su-php* to work.


## Security

**WARNING:**
*su-php* is in need of real-world testing and an audit.
**You do *not* want to use it.**

*su-php* performs the following checks to determine whether it should run a PHP script:

1. Are `WWW_USER` and `WWW_GROUP` known to the system?
2. Is the executable owned by UID 0 and `WWW_GROUP` and
   neither group-writable, nor world-writable, nor world-executable?
3. Is the directory the executable is located in,
   and each of its parent directories,
   owned by root and neither group- nor world-writable?
4. Is the executable run as `WWW_USER` and `WWW_GROUP`?
5. Does the value of `PATH_TRANSLATED` represent an absolute path?
6. Is the UID of the script's owner not 0 and greater than `MIN_UID`?
7. Is the GID of the script's owner not 0 and greater than `MIN_GID`?
8. Do the UID and GID of the script reference a user and a group known to the system?
9. Is the GID of the script's owner also the ID of the owner's primary group?
10. Can the executable drop all supplemantary groups,
    set its effective GID to the GID of the script's owner, and
    its effective UID to the UID of the script's owner?
11. After doing that, does resetting its effective UID to 0 fail?
12. Does the value of `PATH_TRANSLATED` end in '.php'?
13. Is the script located in `BASE_DIR`?
14. Is the script located in the home directory of its owner?
15. Is the directory the script is located in,
    and each of its parent directories up to the user's home directory,
	owned by the user the script should be run as and
	neither group- nor world-writable?
15. Is the parent directory of the user's home directory,
    and each of its parent directories,
    owned by root and neither group- nor world-writable?
17. Can environment variables not listed in `ENV_VARS` be unset?
18. Can the environment variable `PATH` be set to the configuration value `PATH`?

Unless all of the above conditions are met, *su-php* aborts.


## Installation 

You use *su-php* at your own risk!
That risk is considerable!

----

Download the repository and unpack it.
<!--[latest release](https://github.com/odkr/su-php/releases/latest)
and unpack it:

```sh
(
	url="https://github.com/odkr/su-php/releases/download/v0.0.0/su-php-0.0.0.tgz"
	curl --silent --show-error --location "$url"
	[ "$?" -eq 127 ] && wget --output-document=- "$url"
) | tar -xz
```
-->
----

*su-php* is configured at compile-time. Don't worry, it compiles in < 1s.

Adapt `config.h`:

```sh
cd su-php-0.0.0
"${VISUAL-${EDITOR-vi}}" config.h
```

If you are using Debian GNU/Linux and Apache, the defaults should be fine.

----

Once you are done, compile *su-php* by:

```sh
make
```

There should now be an executable `su-php` in the repository's top-level directory:

```sh
$ ls su-php
su-php
```


----


Change `su-php`'s owner and permissions by:

```sh
chown root:www-data su-php
chmod u=rws,g=x,o= su-php
```

If your web server does not run under the group 'www-data', you need to adapt the command above accordingly.


----

Move `su-php` into your directory for CGI binaries (e.g., `/usr/lib/cgi-bin`).

```sh
mv su-php /usr/lib/cgi-bin
```

If your directory for CGI binaries is not `/usr/lib/cgi-bin`, you need to adapt the command above accordingly.

----

Tell your webserver to run PHP scripts via *su-php*.

If you are using [Apache](https://www.apache.org) v2, look for

> Action application/x-httpd-php /cgi-bin/php

and replace it with:

> Action application/x-httpd-php /cgi-bin/su-php

You need to run PHP via CGI for *su-php* to work.


## Documentation

See the [source code](su-php.c) and the [configuration file](config.h) for more details.
It's short and straightforward, I promise.


## Contact

If there's something wrong with *su-php*, please
[open an issue](https://github.com/odkr/su-php/issues).


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

GitHub: <https://github.com/odkr/su-php>
