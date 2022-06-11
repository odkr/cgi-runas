[![Language grade: C](https://img.shields.io/lgtm/grade/cpp/github/odkr/cgi-runas.svg)](https://lgtm.com/projects/g/odkr/cgi-runas)
[![Build](https://ci.appveyor.com/api/projects/status/3besl1g6c66llwax/branch/main?svg=true)](https://ci.appveyor.com/project/odkr/cgi-runas/branch/main)

# cgi-runas

Run CGI scripts under the UID and GID of their owner.

**cgi-runas** checks if the script file pointed to by the environment variable
`PATH_TRANSLATED` is secure, sets the process' effective UID and GID to
the UID and the GID of the script's owner, cleans up the environment, and then
executes the actual CGI handler.

If you set up 
[PHP as CGI handler](https://www.php.net/manual/en/install.unix.commandline.php),
you can use **cgi-runas** to run PHP scripts under the UID and GID of their owner.

See the [manual](MANUAL.rst) for details.

## Requirements

**cgi-runas** requires an operating system that complies with
[POSIX.1-2018](https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/).

Ideally, your system supports the
[setgroups](https://man7.org/linux/man-pages/man2/setgroups.2.html)
and the
[clearenv](https://man7.org/linux/man-pages/man3/clearenv.3.html)
system calls and has a
[proc](https://tldp.org/LDP/Linux-Filesystem-Hierarchy/html/proc.html)
filesystem mounted on */proc*.
Linux-based operating systems should meet those requirements.


## Installation 

You use **cgi-runas** at your own risk!
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
less cgi-runas.c
```

----

**cgi-runas** is configured at compile-time. Don't worry, it compiles in < 1s.

Adapt [config.h](config.h):

```sh
"${VISUAL-${EDITOR-vi}}" config.h
```

If you are using Debian GNU/Linux and Apache and want to use **cgi-runas**
to run PHP scripts, the defaults should be fine.

----

Once you are done, compile **cgi-runas** by:

```sh
make
```

If your operating system does not support **clearenv**, **setgroups**,
or the proc filesystem, you can disable them using these flags:

| Flag         | Description                                |
| ------------ | ------------------------------------------ |
| NO_CLEARENV  | Clear the environment by `environ = NULL`. |
| NO_PROCFS    | Don't use */proc*.                         |
| NO_SETGROUPS | Don't call **setgroups**.                  |

For example:

```sh
make CFLAGS="-DNO_PROCFS -DNO_CLEARENV"
```

There should now be an executable **cgi-runas** in
the repository's top-level directory:

```sh
$ ls cgi-runas
cgi-runas
```


----


Change **cgi-runas**'s owner and permissions by:

```sh
chown root:www-data cgi-runas
chmod u=rws,g=x,o= cgi-runas
```

If your web server does not run under the group 'www-data',
you need to adapt the command above accordingly.


----

Move `cgi-runas` into your directory for CGI binaries
(e.g., */usr/lib/cgi-bin*).

You may want to rename it so that its name indicates
which CGI handler it is running.

```sh
mv cgi-runas /usr/lib/cgi-bin/php-runas
```

If your directory for CGI binaries is not */usr/lib/cgi-bin*,
you need to adapt the command above accordingly.

----

Tell your webserver to run scripts in **SCRIPT_BASE_DIR** via **php-runas**.

If you are using [Apache](https://www.apache.org) v2, you can do so by:

```apacheconf
<Directory "/home">
        Action application/x-httpd-php /cgi-bin/php-runas
</Directory>
```

If your **SCRIPT_BASE_DIR** is not */home*, you need to adapt the command
above accordingly. Note that **php-runas** is the name we have given
**cgi-runas** above.


## Documentation

See the [manual](MANUAL.rst), the [source code](cgi-runas.c), and
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
