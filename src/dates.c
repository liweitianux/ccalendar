/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 The DragonFly Project.  All rights reserved.
 * Copyright (c) 1992-2009 Edwin Groothuis <edwin@FreeBSD.org>.
 * All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Aaron LI <aly@aaronly.me>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: head/usr.bin/calendar/dates.c 326276 2017-11-27 15:37:16Z pfg $
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "calendar.h"
#include "basics.h"
#include "dates.h"
#include "gregorian.h"
#include "utils.h"


struct event {
	bool		 variable;  /* Whether a variable event ? */
	char		*date;  /* human readable */
	char		*contents[CAL_MAX_LINES];  /* lines of contents */
	char		*extra;
	struct event	*next;
};

struct cal_day {
	int		 rd;
	struct event	*events;
};
static struct cal_day *cal_days = NULL;


void
generate_dates(void)
{
	struct cal_day *dp;
	struct date date;
	int daycount, dow;

	daycount = Options.day_end - Options.day_begin + 1;
	cal_days = xcalloc((size_t)daycount, sizeof(struct cal_day));
	for (int i = 0; i < daycount; i++) {
		dp = &cal_days[i];
		dp->rd = Options.day_begin + i;

		if (Options.debug) {
			gregorian_from_fixed(dp->rd, &date);
			dow = (int)dayofweek_from_fixed(dp->rd);
			fprintf(stderr,
				"%s: [%d] rd:%d, date:%d-%02d-%02d, dow:%d\n",
				__func__, i, dp->rd, date.year, date.month,
				date.day, dow);
		}
	}
}

int
first_dayofweek_of_year(int yy)
{
	if (yy < Options.year1 || yy > Options.year2)
		return -1;  /* out-of-range */

	struct date date = { yy, 1, 1 };
	int rd = fixed_from_gregorian(&date);
	return (int)dayofweek_from_fixed(rd);
}

int
first_dayofweek_of_month(int yy, int mm)
{
	int firstday, lastday;

	struct date date = { yy, mm, 1 };
	firstday = fixed_from_gregorian(&date);
	date.month++;
	lastday = fixed_from_gregorian(&date) - 1;

	if (firstday > Options.day_end || lastday < Options.day_begin)
		return -1;  /* out-of-range */

	return (int)dayofweek_from_fixed(firstday);
}

struct cal_day *
find_yd(int yy, int dd)
{
	struct date gdate = { yy, 1, 1 };
	int rd = fixed_from_gregorian(&gdate) + dd - 1;
	if (rd < Options.day_begin || rd > Options.day_end)
		return NULL;

	return &cal_days[rd - Options.day_begin];
}

struct cal_day *
find_ymd(int yy, int mm, int dd)
{
	struct date gdate = { yy, mm, dd };
	int rd = fixed_from_gregorian(&gdate);
	if (rd < Options.day_begin || rd > Options.day_end)
		return NULL;

	return &cal_days[rd - Options.day_begin];
}


struct event *
event_add(struct cal_day *dp, bool day_first, bool variable,
	  char *contents[], char *extra)
{
	static char dbuf[32];
	struct date gdate = { 0 };
	struct tm tm = { 0 };
	struct event *e;

	gregorian_from_fixed(dp->rd, &gdate);
	tm.tm_year = gdate.year - 1900;
	tm.tm_mon = gdate.month - 1;
	tm.tm_mday = gdate.day;
	strftime(dbuf, sizeof(dbuf), (day_first ? "%e %b" : "%b %e"), &tm);

	e = xcalloc(1, sizeof(*e));
	e->variable = variable;
	e->date = xstrdup(dbuf);
	for (int i = 0; i < CAL_MAX_LINES; i++)
		e->contents[i] = contents[i];
	if (extra != NULL && extra[0] != '\0')
		e->extra = extra;

	e->next = dp->events;
	dp->events = e;

	return (e);
}

void
event_print_all(FILE *fp)
{
	struct event *e;
	struct cal_day *dp;
	int daycount = Options.day_end - Options.day_begin + 1;
	int last;

	for (int i = 0; i < daycount; i++) {
		dp = &cal_days[i];
		for (e = dp->events; e != NULL; e = e->next) {
			last = 0;
			while (last < CAL_MAX_LINES && e->contents[last])
			     last++;

			fprintf(fp, "%s%c", e->date, e->variable ? '*' : ' ');
			for (int j = 0; j < last; j++) {
				fprintf(fp, "\t%s%s", e->contents[j],
					(j == last - 1) ? "" : "\n");
			}
			if (e->extra)
				fprintf(fp, " (%s)", e->extra);
			fputc('\n', fp);
			fflush(fp);
		}
	}
}
