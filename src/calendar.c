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
#include "basics.h"
#include "gregorian.h"
#include "parsedata.h"

struct cal_options Options = {
	.calendarFile = NULL,
	.days_before = 0,
	.days_after = 0,
	.UTCOffset = 0.0,
	.EastLongitude = 0.0,
	.debug = false,
};

static void	usage(const char *progname) __dead2;
static int	get_fixed_of_today(void);
static double	get_utc_offset(void);

int
main(int argc, char *argv[])
{
	bool	doall = false;
	int	ret = 0;
	int	Friday = 5;  /* days before weekend */
	int	ch;
	enum dayofweek dow;
	struct g_date gdate1, gdate2;
	struct passwd *pw;
	const char *debug_type = NULL;

	Options.today = get_fixed_of_today();
	Options.UTCOffset = get_utc_offset();
	Options.EastLongitude = Options.UTCOffset * 15;

	while ((ch = getopt(argc, argv, "-A:aB:D:dF:f:hl:t:U:W:?")) != -1) {
		switch (ch) {
		case '-':		/* backward compatible */
		case 'a':
			if (getuid() != 0)
				errx(1, "must be root to run with '-a'");
			doall = true;
			break;

		case 'W': /* don't need to specially deal with Fridays */
			Friday = -1;
			/* FALLTHROUGH */
		case 'A': /* days after current date */
			Options.days_after = atoi(optarg);
			if (Options.days_after < 0)
				errx(1, "number of days must be positive");
			break;

		case 'B': /* days before current date */
			Options.days_before = atoi(optarg);
			if (Options.days_before < 0)
				errx(1, "number of days must be positive");
			break;

		case 'D': /* debug output of sun and moon info */
			debug_type = optarg;
			break;

		case 'd': /* debug output of current date */
			Options.debug = true;
			break;

		case 'F': /* Change the time: When does weekend start? */
			Friday = atoi(optarg);
			break;

		case 'f': /* other calendar file */
			Options.calendarFile = optarg;
			if (strcmp(optarg, "-") == 0)
				Options.calendarFile = "/dev/stdin";
			break;

		case 'l': /* Change longitudal position */
			Options.EastLongitude = strtod(optarg, NULL);
			Options.UTCOffset = Options.EastLongitude / 15;
			break;

		case 't': /* specify date */
			if (!parse_date(optarg, &Options.today))
				errx(1, "invalid date: |%s|", optarg);
			break;

		case 'U': /* Change UTC offset */
			Options.UTCOffset = strtod(optarg, NULL);
			Options.EastLongitude = Options.UTCOffset * 15;
			break;

		case 'h':
		case '?':
		default:
			usage(argv[0]);
		}
	}

	if (argc > optind)
		usage(argv[0]);

	if (doall && Options.calendarFile != NULL)
		errx(1, "flags -a and -f cannot be used together");

	/* Friday displays Monday's events */
	dow = dayofweek_from_fixed(Options.today);
	if (Options.days_after == 0 && Friday != -1)
		Options.days_after = ((int)dow == Friday) ? 3 : 1;

	gregorian_from_fixed(Options.today - Options.days_before, &gdate1);
	Options.year1 = gdate1.year;
	gregorian_from_fixed(Options.today + Options.days_after, &gdate2);
	Options.year2 = gdate2.year;

	generatedates(&gdate1, &gdate2);

	setlocale(LC_ALL, "");
	setnnames();

	/*
	 * FROM now on, we are working in UTC.
	 * This will only affect moon and sun related events anyway.
	 */
	if (setenv("TZ", "UTC", 1) != 0)
		err(1, "setenv");
	tzset();

	if (Options.debug)
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


static int
get_fixed_of_today(void)
{
	time_t now;
	struct tm tm;
	struct g_date gdate;

	now = time(NULL);
	tzset();
	localtime_r(&now, &tm);

	gdate.year = tm.tm_year + 1900;
	gdate.month = tm.tm_mon + 1;
	gdate.day = tm.tm_mday;

	return fixed_from_gregorian(&gdate);
}

static double
get_utc_offset(void)
{
	time_t now;
	struct tm tm;

	now = time(NULL);
	tzset();
	localtime_r(&now, &tm);

	return (tm.tm_gmtoff / 3600.0);
}

static void __dead2
usage(const char *progname)
{
	fprintf(stderr,
		"usage:\n"
		"%s [-A days] [-a] [-B days] [-D sun|moon] [-d]\n"
		"\t[-F friday] [-f calendarfile] [-l longitude]\n"
		"\t[-t [[[cc]yy]mm]dd] [-U utcoffset] [-W days]\n",
		progname);
	exit(1);
}
