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

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "calendar.h"
#include "utils.h"

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
		if (py->year == y + 1900)
			break;
		pyp = py;
		py = py->nextyear;
	}

	if (py == NULL) {
		struct tm td = { 0 };
		time_t t;

		py = xcalloc(1, sizeof(*py));
		py->year = y + 1900;
		py->easter = easter(y);
		py->paskha = paskha(y);

		td.tm_year = y;
		td.tm_mon = 0;
		td.tm_mday = 1;
		t = mktime(&td);
		localtime_r(&t, &td);
		py->firstdayofweek = td.tm_wday;

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
		pm->firstdayofweek =
		    (py->firstdayofweek + pm->firstdayjulian - 1) % 7;
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
		pd->dayofweek = (pm->firstdayofweek + d - 1) % 7;
		if (pdp != NULL)
			pdp->nextday = pd;
	}
	if (pm->days == NULL)
		pm->days = pd;
}

void
generatedates(struct tm *tp1, struct tm *tp2)
{
	int y1, m1, d1;
	int y2, m2, d2;
	int y, m, d;

	y1 = tp1->tm_year;
	m1 = tp1->tm_mon + 1;
	d1 = tp1->tm_mday;
	y2 = tp2->tm_year;
	m2 = tp2->tm_mon + 1;
	d2 = tp2->tm_mday;

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

	for (yp = hyear; yp != NULL; yp = yp->nextyear) {
		fprintf(stderr, "%-5d (wday:%d)\n",
			yp->year, yp->firstdayofweek);
		for (mp = yp->months; mp != NULL; mp = mp->nextmonth) {
			fprintf(stderr, "-- %-5d (julian:%d, dow:%d)\n",
				mp->month, mp->firstdayjulian,
				mp->firstdayofweek);
			for (dp = mp->days; dp != NULL; dp = dp->nextday) {
				fprintf(stderr, "  -- %-5d (julian:%d, dow:%d)\n",
					dp->dayofmonth, dp->julianday,
					dp->dayofweek);
			}
		}
	}
}

int
first_dayofweek_of_year(int yy)
{
	struct cal_year *yp;

	for (yp = hyear; yp != NULL; yp = yp->nextyear) {
		if (yp->year == yy)
			return yp->firstdayofweek;
	}

	return -1;
}

int
first_dayofweek_of_month(int yy, int mm)
{
	struct cal_year *yp;
	struct cal_month *mp;

	for (yp = hyear; yp != NULL; yp = yp->nextyear) {
		if (yp->year == yy)
			break;
	}
	if (yp == NULL)
		return -1;

	for (mp = yp->months; mp != NULL; mp = mp->nextmonth) {
		if (mp->month == mm)
			return mp->firstdayofweek;
	}

	return -1;
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
	if (d == NULL) {
		errx(1, "%s: find_ymd(%d, %d, %d) failed",
				__func__, year, month, day);
	}

	e->next = d->events;
	d->events = e;
}
