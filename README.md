# su-php

A wrapper that runs PHP scripts under the UID and GID of their owner.


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

*su-php* has only been tested on Debian GNU/Linux.

You also need [PHP](https://php.net/), of course.
And you need to run it via the CGI for *su-php* to work.


## Security

**WARNING:**
*su-php* is in need of real-world testing and a second pair of eyes to audit it.
**You do *not* want to use it.**

*su-php* performs the following checks to determine whether it should run a PHP script:

1. Are `WWW_USER` and `WWW_GROUP` known to the system?
2. Is the executable owned by UID 0 and `WWW_GROUP`?
3. Is the executable *not* group-writable, world-writable, or world-executable?
4. Is the directory the executable is located in, and each of its ancestors,
   owned by root or the webserver and *not* world-writable?
5. Is the executable run as `WWW_USER` and `WWW_GROUP`?
6. Is the environment variable `PATH_TRANSLATED` set to a non-empty value?
7. Is the UID of the PHP script's owner not 0 and greater than `MIN_UID`?
8. Is the GID of the PHP script's owner not 0 and greater than `MIN_GID`?
9. Do the UID and GID of the PHP reference a user and a group known to the system?
10. Can the executable drop all suplemantary groups,
    set its effective GID to the GID of the PHP script's owner, and
	its effective UID to the UID of the PHP script's owner?
11. After doing that, does resetting its effective UID to 0 fail?
12. Does the value of `PATH_TRANSLATED` end in '.php'?
13. Is the PHP script located inside `BASE_DIR`?
14. Is the directory the PHP script is located in, and each of its ancestors,
    owned by root or the user the script should be run as and *not* world-writable?
15. Can environment variables not listed in `SAFE_ENV_VARS` be unset?
16. Can the environment variable `PATH` be set to the configuration value `PATH`?

Unless all of the above conditions are met, *su-php* aborts.

*su-php* does *not* clean up the environent just yet.


## Installation 

You use *su-php* at your own risk!
And that risk is considerable!

----

Download the
[latest release](https://github.com/odkr/su-php/releases/latest)
and unpack it:

```sh
(
	url="https://github.com/odkr/su-php/releases/download/v0.0.0/su-php-0.0.0.tgz"
	curl --silent --show-error --location "$url"
	[ "$?" -eq 127 ] && wget --output-document=- "$url"
) | tar -xz
```

----

*su-php* is configured at compile-time.

You need to adapt `config.h` before compiling:

```sh
cd su-php-0.0.0
"${VISUAL-${EDITOR-vi}}" config.h
```


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


Change *su-php*'s owner and permissions by:

```sh
chown root:www-data su-php
chmod u=rws,g=x,o= su-php
```

If your web server does not run under the group 'www-data', you need to adopt the command above accordingly.


----

Move `su-php` into your directory for CGI binaries (e.g., `/usr/lib/cgi-bin`).

```sh
mv su-php /usr/lib/cgi-bin
```

If your directory for CGI binaries is not `/usr/lib/cgi-bin`, you need to adopt the command above accordingly.

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
