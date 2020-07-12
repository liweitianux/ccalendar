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

#include "calendar.h"
#include "basics.h"
#include "gregorian.h"
#include "utils.h"


/* 1-based month, 0-based days, cumulative */
int	cumdaytab[][14] = {
	{0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364},
	{0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
};
/* 1-based month, individual */
int	monthdaytab[][14] = {
	{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30},
	{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30},
};


struct cal_day {
	int		 rd;
	struct cal_day	*next;
	struct event	*events;
};
static struct cal_day *cal_days = NULL;


int
cal_day_get_month(struct cal_day *dayp)
{
	struct g_date gdate;

	gregorian_from_fixed(dayp->rd, &gdate);
	return gdate.month;
}

int
cal_day_get_day(struct cal_day *dayp)
{
	struct g_date gdate;

	gregorian_from_fixed(dayp->rd, &gdate);
	return gdate.day;
}


void
generatedates(int rd1, int rd2)
{
	struct cal_day *dp;
	struct cal_day **dpp = &cal_days;

	for (int rd = rd1; rd <= rd2; rd++) {
		dp = xcalloc(1, sizeof(*dp));
		dp->rd = rd;
		*dpp = dp;
		dpp = &dp->next;
	}
}

void
dumpdates(void)
{
	struct cal_day *dp;
	int i, dow;

	for (i = 0, dp = cal_days; dp != NULL; i++, dp = dp->next) {
		dow = (int)dayofweek_from_fixed(dp->rd);
		fprintf(stderr, "%s(): [%d] rd:%d, dow:%d\n",
			__func__, i, dp->rd, dow);
	}
}

int
first_dayofweek_of_year(int yy)
{
	if (yy < Options.year1 || yy > Options.year2)
		return -1;  /* out-of-range */

	struct g_date date = { yy, 1, 1 };
	int rd = fixed_from_gregorian(&date);
	return (int)dayofweek_from_fixed(rd);
}

int
first_dayofweek_of_month(int yy, int mm)
{
	int firstday, lastday;

	struct g_date date = { yy, mm, 1 };
	firstday = fixed_from_gregorian(&date);
	date.month++;
	lastday = fixed_from_gregorian(&date) - 1;

	if (firstday > (Options.today + Options.days_after) ||
	    lastday < (Options.today - Options.days_before))
		return -1;  /* out-of-range */

	return (int)dayofweek_from_fixed(firstday);
}

bool
walkthrough_dates(struct event **e)
{
	static struct cal_day *dp = NULL;
	static bool inited = false;

	if (dp == NULL && !inited) {
		dp = cal_days;
		inited = true;
	}

	if (dp != NULL) {
		*e = dp->events;
		dp = dp->next;
		return true;
	}

	return false;
}

struct cal_day *
find_yd(int yy, int dd)
{
	struct g_date gdate = { yy, 1, 1 };
	struct cal_day *dp;
	int rd;

	rd = fixed_from_gregorian(&gdate) + dd - 1;
	for (dp = cal_days; dp != NULL; dp = dp->next) {
		if (dp->rd == rd)
			return dp;
	}

	return NULL;
}

struct cal_day *
find_ymd(int yy, int mm, int dd)
{
	struct g_date gdate = { yy, mm, dd };
	struct cal_day *dp;
	int rd;

	rd = fixed_from_gregorian(&gdate);
	for (dp = cal_days; dp != NULL; dp = dp->next) {
		if (dp->rd == rd)
			return dp;
	}

	return NULL;
}

void
addtodate(struct event *e, int year, int month, int day)
{
	struct cal_day *d;

	d = find_ymd(year, month, day);
	assert(d != NULL);

	e->next = d->events;
	d->events = e;
}
