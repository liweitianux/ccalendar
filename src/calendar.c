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
#include "chinese.h"
#include "gregorian.h"
#include "moon.h"
#include "parsedata.h"
#include "sun.h"


struct cal_options Options = {
	.calendarFile = NULL,
	.time = 0.5,  /* noon */
	.allmode = false,
	.debug = false,
};

static void	usage(const char *progname) __dead2;
static double	get_time_of_now(void);
static int	get_fixed_of_today(void);
static int	get_utc_offset(void);


int
main(int argc, char *argv[])
{
	bool	L_flag = false;
	int	ret = 0;
	int	days_before = 0;
	int	days_after = 0;
	int	Friday = 5;  /* days before weekend */
	int	ch;
	int	utc_offset;
	enum dayofweek dow;
	struct passwd *pw;
	struct location loc = { 0 };
	const char *show_info = NULL;

	Options.location = &loc;
	Options.time = get_time_of_now();
	Options.today = get_fixed_of_today();
	loc.zone = get_utc_offset() / (3600.0 * 24.0);

	while ((ch = getopt(argc, argv, "-A:aB:dF:f:hL:l:s:t:U:W:")) != -1) {
		switch (ch) {
		case '-':		/* backward compatible */
		case 'a':
			if (getuid() != 0)
				errx(1, "must be root to run with '-a'");
			Options.allmode = true;
			break;

		case 'W': /* don't need to specially deal with Fridays */
			Friday = -1;
			/* FALLTHROUGH */
		case 'A': /* days after current date */
			days_after = atoi(optarg);
			if (days_after < 0)
				errx(1, "number of days must be positive");
			break;

		case 'B': /* days before current date */
			days_before = atoi(optarg);
			if (days_before < 0)
				errx(1, "number of days must be positive");
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

		case 'L': /* location */
			if (!parse_location(optarg, &loc.latitude,
					    &loc.longitude, &loc.elevation)) {
				errx(1, "invalid location: |%s|", optarg);
			}
			L_flag = true;
			break;

		case 's': /* show info of specified category */
			show_info = optarg;
			break;

		case 't': /* specify date */
			if (!parse_date(optarg, &Options.today))
				errx(1, "invalid date: |%s|", optarg);
			break;

		case 'U': /* specify timezone */
			if (!parse_timezone(optarg, &utc_offset))
				errx(1, "invalid timezone: |%s|", optarg);
			loc.zone = utc_offset / (3600.0 * 24.0);
			break;

		case 'h':
		default:
			usage(argv[0]);
		}
	}

	if (argc > optind)
		usage(argv[0]);

	if (Options.allmode && Options.calendarFile != NULL)
		errx(1, "flags -a and -f cannot be used together");

	if (!L_flag)
		loc.longitude = loc.zone * 360.0;

	/* Friday displays Monday's events */
	dow = dayofweek_from_fixed(Options.today);
	if (days_after == 0 && Friday != -1)
		days_after = ((int)dow == Friday) ? 3 : 1;

	Options.day_begin = Options.today - days_before;
	Options.day_end = Options.today + days_after;
	Options.year1 = gregorian_year_from_fixed(Options.day_begin);
	Options.year2 = gregorian_year_from_fixed(Options.day_end);
	generatedates();

	setlocale(LC_ALL, "");
	setnnames();

	if (setenv("TZ", "UTC", 1) != 0)
		err(1, "setenv");
	tzset();
	/* We're in UTC from now on */

	if (Options.debug)
		dumpdates();

	if (show_info != NULL) {
		if (strcmp(show_info, "chinese") == 0) {
			show_chinese_calendar(Options.today);
		} else if (strcmp(show_info, "moon") == 0) {
			show_moon_info(Options.today + Options.time,
				       Options.location);
		} else if (strcmp(show_info, "sun") == 0) {
			show_sun_info(Options.today + Options.time,
				      Options.location);
		} else {
			errx(1, "unknown -s value: |%s|", show_info);
		}

		exit(0);
	}

	if (Options.allmode) {
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

				ret = cal();
				_exit(ret);
			}
		}
	} else {
		ret = cal();
	}

	return (ret);
}


static double
get_time_of_now(void)
{
	time_t now;
	struct tm tm;

	now = time(NULL);
	tzset();
	localtime_r(&now, &tm);

	return (tm.tm_hour + tm.tm_min/60.0 + tm.tm_sec/3600.0) / 24.0;
}

static int
get_fixed_of_today(void)
{
	time_t now;
	struct tm tm;
	struct date gdate;

	now = time(NULL);
	tzset();
	localtime_r(&now, &tm);

	gdate.year = tm.tm_year + 1900;
	gdate.month = tm.tm_mon + 1;
	gdate.day = tm.tm_mday;

	return fixed_from_gregorian(&gdate);
}

static int
get_utc_offset(void)
{
	time_t now;
	struct tm tm;

	now = time(NULL);
	tzset();
	localtime_r(&now, &tm);

	return tm.tm_gmtoff;
}

static void __dead2
usage(const char *progname)
{
	fprintf(stderr,
		"usage:\n"
		"%s [-A days] [-a] [-B days] [-d] [-F friday]\n"
		"\t[-f calendarfile] [-L latitude,longitude[,elevation]]\n"
		"\t[-s chinese|moon|sun] [-t [[[cc]yy]mm]dd] [-U ±hh[[:]mm]] [-W days]\n",
		progname);
	exit(1);
}
