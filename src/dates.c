/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1992-2009 Edwin Groothuis <edwin@FreeBSD.org>.
 * All rights reserved.
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
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "calendar.h"
#include "basics.h"
#include "gregorian.h"
#include "utils.h"

struct cal_year {
	int year;	/* 19xx, 20xx, 21xx */
	struct cal_month *months;
	struct cal_year *nextyear;
};

struct cal_month {
	int month;			/* 01 .. 12 */
	int firstdayjulian;		/* 000 .. 366 */
	struct cal_year *year;		/* points back */
	struct cal_day *days;
	struct cal_month *nextmonth;
};

struct cal_day {
	int dayofmonth;			/* 01 .. 31 */
	int julianday;			/* 000 .. 366 */
	int dayofweek;			/* 0 .. 6 */
	struct cal_day *nextday;
	struct cal_month *month;	/* points back */
	struct cal_year *year;		/* points back */
	struct event *events;
};

static struct cal_year *hyear = NULL;

/* 1-based month, 0-based days, cumulative */
int	cumdaytab[][14] = {
	{0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364},
	{0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
};
/* 1-based month, individual */
static int *monthdays;
int	monthdaytab[][14] = {
	{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30},
	{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30},
};

static void createdate(int y, int m, int d);


int
cal_day_get_month(struct cal_day *dayp)
{
	return dayp->month->month;
}

int
cal_day_get_day(struct cal_day *dayp)
{
	return dayp->dayofmonth;
}


static void
createdate(int y, int m, int d)
{
	struct cal_year *py, *pyp;
	struct cal_month *pm, *pmp;
	struct cal_day *pd, *pdp;
	int *cumday;

	pyp = NULL;
	py = hyear;
	while (py != NULL) {
		if (py->year == y)
			break;
		pyp = py;
		py = py->nextyear;
	}

	if (py == NULL) {
		py = xcalloc(1, sizeof(*py));
		py->year = y;
		if (pyp != NULL)
			pyp->nextyear = py;
	}
	if (hyear == NULL)
		hyear = py;

	pmp = NULL;
	pm = py->months;
	while (pm != NULL) {
		if (pm->month == m)
			break;
		pmp = pm;
		pm = pm->nextmonth;
	}

	if (pm == NULL) {
		pm = xcalloc(1, sizeof(*pm));
		pm->year = py;
		pm->month = m;
		cumday = cumdaytab[isleap(y)];
		pm->firstdayjulian = cumday[m] + 2;
		if (pmp != NULL)
			pmp->nextmonth = pm;
	}
	if (py->months == NULL)
		py->months = pm;

	pdp = NULL;
	pd = pm->days;
	while (pd != NULL) {
		pdp = pd;
		pd = pd->nextday;
	}

	if (pd == NULL) {	/* Always true */
		pd = xcalloc(1, sizeof(*pd));
		pd->month = pm;
		pd->year = py;
		pd->dayofmonth = d;
		pd->julianday = pm->firstdayjulian + d - 1;
		if (pdp != NULL)
			pdp->nextday = pd;
	}
	if (pm->days == NULL)
		pm->days = pd;
}

void
generatedates(int rd1, int rd2)
{
	struct g_date gdate;
	int y1, m1, d1;
	int y2, m2, d2;
	int y, m, d;

	gregorian_from_fixed(rd1, &gdate);
	y1 = gdate.year;
	m1 = gdate.month;
	d1 = gdate.day;

	gregorian_from_fixed(rd2, &gdate);
	y2 = gdate.year;
	m2 = gdate.month;
	d2 = gdate.day;

	if (y1 == y2) {
		if (m1 == m2) {
			/* Same year, same month. Easy! */
			for (d = d1; d <= d2; d++)
				createdate(y1, m1, d);
			return;
		}
		/*
		 * Same year, different month.
		 * - Take the leftover days from m1
		 * - Take all days from <m1 .. m2>
		 * - Take the first days from m2
		 */
		monthdays = monthdaytab[isleap(y1)];
		for (d = d1; d <= monthdays[m1]; d++)
			createdate(y1, m1, d);
		for (m = m1 + 1; m < m2; m++)
			for (d = 1; d <= monthdays[m]; d++)
				createdate(y1, m, d);
		for (d = 1; d <= d2; d++)
			createdate(y1, m2, d);
		return;
	}
	/*
	 * Different year, different month.
	 * - Take the leftover days from y1-m1
	 * - Take all days from y1-<m1 .. 12]
	 * - Take all days from <y1 .. y2>
	 * - Take all days from y2-[1 .. m2>
	 * - Take the first days of y2-m2
	 */
	monthdays = monthdaytab[isleap(y1)];
	for (d = d1; d <= monthdays[m1]; d++)
		createdate(y1, m1, d);
	for (m = m1 + 1; m <= 12; m++)
		for (d = 1; d <= monthdays[m]; d++)
			createdate(y1, m, d);
	for (y = y1 + 1; y < y2; y++) {
		monthdays = monthdaytab[isleap(y)];
		for (m = 1; m <= 12; m++)
			for (d = 1; d <= monthdays[m]; d++)
				createdate(y, m, d);
	}
	monthdays = monthdaytab[isleap(y2)];
	for (m = 1; m < m2; m++)
		for (d = 1; d <= monthdays[m]; d++)
			createdate(y2, m, d);
	for (d = 1; d <= d2; d++)
		createdate(y2, m2, d);
}

void
dumpdates(void)
{
	struct cal_year *yp;
	struct cal_month *mp;
	struct cal_day *dp;
	struct g_date date;
	int rd, dow;

	for (yp = hyear; yp != NULL; yp = yp->nextyear) {
		date.year = yp->year;
		date.month = 1;
		date.day = 1;
		rd = fixed_from_gregorian(&date);
		dow = (int)dayofweek_from_fixed(rd);
		fprintf(stderr, "%-5d (rd:%d, dow:%d)\n", yp->year, rd, dow);
		for (mp = yp->months; mp != NULL; mp = mp->nextmonth) {
			date.month = mp->month;
			date.day = 1;
			rd = fixed_from_gregorian(&date);
			dow = (int)dayofweek_from_fixed(rd);
			fprintf(stderr, "-- %-5d (rd:%d, dow:%d)\n",
				mp->month, rd, dow);
			for (dp = mp->days; dp != NULL; dp = dp->nextday) {
				date.day = dp->dayofmonth;
				rd = fixed_from_gregorian(&date);
				dow = (int)dayofweek_from_fixed(rd);
				fprintf(stderr, "  -- %-5d (rd:%d, dow:%d)\n",
					dp->dayofmonth, rd, dow);
			}
		}
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

	if (firstday < (Options.today - Options.days_before) ||
	    lastday > (Options.today + Options.days_after))
		return -1;  /* out-of-range */

	return (int)dayofweek_from_fixed(firstday);
}

bool
walkthrough_dates(struct event **e)
{
	static struct cal_year *y = NULL;
	static struct cal_month *m = NULL;
	static struct cal_day *d = NULL;

	if (y == NULL) {
		y = hyear;
		m = y->months;
		d = m->days;
		*e = d->events;
		return true;
	}
	if (d->nextday != NULL) {
		d = d->nextday;
		*e = d->events;
		return true;
	}
	if (m->nextmonth != NULL) {
		m = m->nextmonth;
		d = m->days;
		*e = d->events;
		return true;
	}
	if (y->nextyear != NULL) {
		y = y->nextyear;
		m = y->months;
		d = m->days;
		*e = d->events;
		return true;
	}

	return false;
}

struct cal_day *
find_yd(int yy, int dd)
{
	struct cal_year *yp;
	struct cal_month *mp;
	struct cal_day *dp;

	for (yp = hyear; yp != NULL; yp = yp->nextyear) {
		if (yp->year == yy)
			break;
	}
	if (yp == NULL)
		return NULL;

	for (mp = yp->months; mp != NULL; mp = mp->nextmonth) {
		for (dp = mp->days; dp != NULL; dp = dp->nextday) {
			if (dp->julianday == dd)
				return dp;
		}
	}

	return NULL;
}

struct cal_day *
find_ymd(int yy, int mm, int dd)
{
	struct cal_year *yp;
	struct cal_month *mp;
	struct cal_day *dp;

	for (yp = hyear; yp != NULL; yp = yp->nextyear) {
		if (yp->year == yy)
			break;
	}
	if (yp == NULL)
		return NULL;

	for (mp = yp->months; mp != NULL; mp = mp->nextmonth) {
		if (mp->month == mm)
			break;
	}
	if (mp == NULL)
		return NULL;

	for (dp = mp->days; dp != NULL; dp = dp->nextday) {
		if (dp->dayofmonth == dd)
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
