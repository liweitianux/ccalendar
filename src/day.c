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
 * $FreeBSD: head/usr.bin/calendar/day.c 326025 2017-11-20 19:49:47Z pfg $
 */

#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "calendar.h"
#include "parsedata.h"

int	year1, year2;


void
settimes(time_t now, int before, int after, int friday,
	 struct tm *tp1, struct tm *tp2)
{
	struct tm tp;
	time_t t;

	localtime_r(&now, &tp);

	/* Friday displays Monday's events */
	if (after == 0 && before == 0 && friday != -1)
		after = (tp.tm_wday == friday) ? 3 : 1;

	t = now - SECSPERDAY * before;
	localtime_r(&t, tp1);
	year1 = 1900 + tp1->tm_year;
	t = now + SECSPERDAY * after;
	localtime_r(&t, tp2);
	year2 = 1900 + tp2->tm_year;
}

/*
 * Parse '[[[cc]yy]mm]dd' date string into UNIX timestamp (since 1970)
 */
bool
Mktime(const char *date, time_t *t_out)
{
	time_t t;
	size_t len;
	struct tm tm;
	int val;

	len = strlen(date);
	if (len < 2)
		return false;

	time(&t);
	localtime_r(&t, &tm);
	tm.tm_sec = tm.tm_min = 0;
	/* Avoid getting caught by a timezone shift; set time to noon */
	tm.tm_hour = 12;
	tm.tm_wday = tm.tm_isdst = 0;

	if (parse_int_ranged(date+len-2, 2, 1, 31, &tm.tm_mday) == NULL)
		return false;

	if (len >= 4) {
		if (parse_int_ranged(date+len-4, 2, 1, 12, &val) == NULL)
			return false;
		tm.tm_mon = val - 1;
	}

	if (len >= 6) {
		if (parse_int_ranged(date, len-4, 0, 9999, &val) == NULL)
			return false;
		if (val < 69)  /* Y2K */
			tm.tm_year = val + 100;
		else if (val < 100)
			tm.tm_year = val;
		else
			tm.tm_year = val - 1900;
	}

	*t_out = mktime(&tm);
	logdebug("%s(): %ld, %s\n", __func__, (long)*t_out, asctime(&tm));

	return true;
}
