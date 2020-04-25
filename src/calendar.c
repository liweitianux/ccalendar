/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)calendar.c  8.3 (Berkeley) 3/25/94
 * $FreeBSD: head/usr.bin/calendar/calendar.c 326025 2017-11-20 19:49:47Z pfg $
 */

#include <sys/types.h>

#include <err.h>
#include <grp.h>  /* required on Linux for initgroups() */
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "calendar.h"
#include "parsedata.h"

bool		debug = false;
double		UTCOffset = 0.0;
double		EastLongitude = 0.0;
struct tm	tm_now = { 0 };  /* time/date of calendar events to remind */

static void	usage(void) __dead2;

int
main(int argc, char *argv[])
{
	bool	doall = false;
	int	ret = 0;
	int	f_dayAfter = 0;		/* days after current date */
	int	f_dayBefore = 0;	/* days before current date */
	int	Friday = 5;		/* day before weekend */
	int	ch;
	time_t	f_time;
	struct tm tp1, tp2;
	struct passwd *pw;
	const char *debug_type = NULL;

	setlocale(LC_ALL, "");
	tzset();
	f_time = time(NULL);
	localtime_r(&f_time, &tm_now);
	UTCOffset = tm_now.tm_gmtoff / FSECSPERHOUR;
	EastLongitude = UTCOffset * 15;

	while ((ch = getopt(argc, argv, "-A:aB:D:dF:f:hl:t:U:W:?")) != -1) {
		switch (ch) {
		case '-':		/* backward compatible */
		case 'a':
			if (getuid())
				errx(1, "must be root to run with '-a'");
			doall = true;
			break;

		case 'W': /* we don't need no steenking Fridays */
			Friday = -1;
			/* FALLTHROUGH */
		case 'A': /* days after current date */
			f_dayAfter = atoi(optarg);
			if (f_dayAfter < 0)
				errx(1, "number of days must be positive");
			break;

		case 'B': /* days before current date */
			f_dayBefore = atoi(optarg);
			if (f_dayBefore < 0)
				errx(1, "number of days must be positive");
			break;

		case 'D': /* debug output of sun and moon info */
			debug_type = optarg;
			break;

		case 'd': /* debug output of current date */
			debug = true;
			break;

		case 'F': /* Change the time: When does weekend start? */
			Friday = atoi(optarg);
			break;

		case 'f': /* other calendar file */
			calendarFile = optarg;
			break;

		case 'l': /* Change longitudal position */
			EastLongitude = strtod(optarg, NULL);
			UTCOffset = EastLongitude / 15;
			break;

		case 't': /* specify date */
			f_time = Mktime(optarg);
			if (f_time <= 0)
				errx(1, "invalid date: |%s|", optarg);
			localtime_r(&f_time, &tm_now);
			break;

		case 'U': /* Change UTC offset */
			UTCOffset = strtod(optarg, NULL);
			EastLongitude = UTCOffset * 15;
			break;

		case 'h':
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	setnnames();
	settimes(f_time, f_dayBefore, f_dayAfter, Friday, &tp1, &tp2);
	generatedates(&tp1, &tp2);

	/*
	 * FROM now on, we are working in UTC.
	 * This will only affect moon and sun related events anyway.
	 */
	if (setenv("TZ", "UTC", 1) != 0)
		err(1, "setenv");
	tzset();

	if (debug)
		dumpdates();

	if (debug_type != NULL) {
		dodebug(debug_type);
		exit(0);
	}

	if (doall) {
		while ((pw = getpwent()) != NULL) {
			if (chdir(pw->pw_dir) == -1)
				continue;

			pid_t pid = fork();
			if (pid < 0)
				err(1, "fork");
			if (pid == 0) {
				if (setgid(pw->pw_gid) == -1)
					err(1, "setgid(%d)", (int)pw->pw_gid);
				if (initgroups(pw->pw_name, pw->pw_gid) == -1)
					err(1, "initgroups(%s)", pw->pw_name);
				if (setuid(pw->pw_uid) == -1)
					err(1, "setuid(%d)", (int)pw->pw_uid);

				ret = cal(doall);
				exit(ret);
			}
		}
	} else {
		ret = cal(doall);
	}

	return (ret);
}


static void __dead2
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: calendar [-A days] [-a] [-B days] [-D sun|moon] [-d]",
	    "                [-F friday] [-f calendarfile] [-l longitude]",
	    "                [-t [[[cc]yy]mm]dd] [-U utcoffset] [-W days]"
	    );
	exit(1);
}
