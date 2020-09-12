Chinese Calendar
================
[![Build Status](https://travis-ci.com/liweitianux/ccalendar.svg)](https://travis-ci.com/liweitianux/ccalendar)

A Chinese calendar based on the UNIX calendar(1) utility for BSD, Linux,
and macOS systems.

The Chinese calendar calculation, as well as the Sun and Moon calculations, is
implemented with **astronomical algorithms** by referring to
[Calendrical Calculations: The Ultimate Edition (4th Edition)](http://www.cs.tau.ac.il/~nachum/calendar-book/fourth-edition/)
by Edward M. Reingold and Nachum Dershowitz.

This utility was derived from the
[calendar(1)](https://gitweb.dragonflybsd.org/dragonfly.git/tree/HEAD:/usr.bin/calendar)
in
[DragonFly BSD](https://www.dragonflybsd.org/)
on 2019-11-16.
It has since been rewritten to be more clean, understandable, and extensible.
Currently, the **Gregorian**, **Julian**, and **Chinese** calendars are supported.
Some updates in FreeBSD, OpenBSD, NetBSD, and
[Debian Linux](https://salsa.debian.org/meskes/bsdmainutils)
have also been integrated.

See also the
[calendar(1)](https://www.dragonflybsd.org/cgi/web-man?command=calendar&section=1)
man page.

Welcome to try this utility, report issues, and contribute fixes and improvements :D


Installaion
-----------
Requirements:
* GCC
* GNU Make

Build and installation:
1. `make [PREFIX=/usr/local]`
2. `sudo make install [PREFIX=/usr/local]`


References
----------
[RD18]
Edward M. Reingold and Nachum Dershowitz,
*Calendrical Calculations: The Ultimate Edition*
(4th Edition).
Cambridge University Press, 2018.
ISBN: 9781107057623


License
-------
The 3-Clause BSD License

Copyright (c) 2019-2020 The DragonFly Project.
