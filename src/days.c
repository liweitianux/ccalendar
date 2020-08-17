/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2020 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Aaron LI <aly@aaronly.me>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <err.h>
#include <stddef.h>

#include "calendar.h"
#include "basics.h"
#include "chinese.h"
#include "dates.h"
#include "days.h"
#include "ecclesiastical.h"
#include "gregorian.h"
#include "nnames.h"
#include "parsedata.h"
#include "utils.h"

static int	days_in_month(int month, int year);
static int	dayofweek_of_month(int dow, int index, int month, int year);
static int	find_days_yearly(int day_flag, int offset,
				 struct cal_day **dayp, char **edp);

static int	find_days_easter(int, struct cal_day **, char **);
static int	find_days_paskha(int, struct cal_day **, char **);
static int	find_days_cny(int, struct cal_day **, char **);

#define SPECIALDAY_INIT0 \
	{ NULL, 0, NULL, 0, 0, NULL }
#define SPECIALDAY_INIT(name, flag, func) \
	{ name, sizeof(name)-1, NULL, 0, (flag), func }
struct specialday specialdays[] = {
	SPECIALDAY_INIT("Easter", F_EASTER, &find_days_easter),
	SPECIALDAY_INIT("Paskha", F_PASKHA, &find_days_paskha),
	SPECIALDAY_INIT("ChineseNewYear", F_CNY, &find_days_cny),
	SPECIALDAY_INIT("NewMoon", F_NEWMOON, NULL),
	SPECIALDAY_INIT("FullMoon", F_FULLMOON, NULL),
	SPECIALDAY_INIT("MarEquinox", F_MAREQUINOX, NULL),
	SPECIALDAY_INIT("SepEquinox", F_SEPEQUINOX, NULL),
	SPECIALDAY_INIT("JunSolstice", F_JUNSOLSTICE, NULL),
	SPECIALDAY_INIT("DecSolstice", F_DECSOLSTICE, NULL),
	SPECIALDAY_INIT0,
};


static int
find_days_easter(int offset, struct cal_day **dayp, char **edp)
{
	return find_days_yearly(F_EASTER, offset, dayp, edp);
}

static int
find_days_paskha(int offset, struct cal_day **dayp, char **edp)
{
	return find_days_yearly(F_PASKHA, offset, dayp, edp);
}

static int
find_days_cny(int offset, struct cal_day **dayp, char **edp)
{
	return find_days_yearly(F_CNY, offset, dayp, edp);
}

/*
 * Find days of the yearly special day ($day_flag).
 */
static int
find_days_yearly(int day_flag, int offset, struct cal_day **dayp,
		 char **edp __unused)
{
	struct cal_day *dp;
	struct date date;
	int day0, yday;
	int count = 0;

	for (int y = Options.year1; y <= Options.year2; y++) {
		date_set(&date, y - 1, 12, 31);
		day0 = fixed_from_gregorian(&date);

		switch (day_flag) {
		case F_EASTER:
			yday = easter(y) - day0;
			break;
		case F_PASKHA:
			yday = orthodox_easter(y) - day0;
			break;
		case F_CNY:
			yday = chinese_new_year(y) - day0;
			break;
		default:
			errx(1, "%s: unknown special day: 0x%x",
			     __func__, day_flag);
		}

		if ((dp = find_yd(y, yday, offset)) != NULL) {
			if (count >= CAL_MAX_REPEAT) {
				warnx("%s: too many repeats", __func__);
				return count;
			}
			dayp[count++] = dp;
		}
	}

	return count;
}

/**************************************************************************/

/*
 * Find days of the specified year ($y), month ($m) and day ($d).
 * If year $y < 0, then year is ignored.
 */
int
find_days_ymd(int year, int month, int day,
	      struct cal_day **dayp, char **edp __unused)
{
	struct cal_day *dp;
	int count = 0;

	for (int y = Options.year1; y <= Options.year2; y++) {
		if (year >= 0 && year != y)
			continue;
		if ((dp = find_ymd(y, month, day)) != NULL) {
			if (count >= CAL_MAX_REPEAT) {
				warnx("%s: too many repeats", __func__);
				return count;
			}
			dayp[count++] = dp;
		}
	}

	return count;
}

/*
 * Find days of the specified day of month ($dom) of all months.
 */
int
find_days_dom(int dom, struct cal_day **dayp, char **edp __unused)
{
	struct cal_day *dp;
	int count = 0;

	for (int y = Options.year1; y <= Options.year2; y++) {
		for (int m = 1; m <= NMONTHS; m++) {
			if ((dp = find_ymd(y, m, dom)) != NULL) {
				if (count >= CAL_MAX_REPEAT) {
					warnx("%s: too many repeats",
					      __func__);
					return count;
				}
				dayp[count++] = dp;
			}
		}
	}

	return count;
}

/*
 * Find days of all days of the specified month ($month).
 */
int
find_days_month(int month, struct cal_day **dayp, char **edp __unused)
{
	struct cal_day *dp;
	int mdays;
	int count = 0;

	for (int y = Options.year1; y <= Options.year2; y++) {
		mdays = days_in_month(month, y);
		for (int d = 1; d <= mdays; d++) {
			if ((dp = find_ymd(y, month, d)) != NULL) {
				if (count >= CAL_MAX_REPEAT) {
					warnx("%s: too many repeats",
					      __func__);
					return count;
				}
				dayp[count++] = dp;
			}
		}
	}

	return count;
}

/*
 * If $index == 0, find days of every day-of-week ($dow) of the specified month
 * ($month).  Otherwise, find days of the indexed day-of-week of the month.
 * If month $month < 0, then find days in every month.
 */
int
find_days_mdow(int month, int dow, int index,
	       struct cal_day **dayp, char **edp __unused)
{
	struct cal_day *dp;
	int d, dow_m1, mdays;
	int count = 0;

	for (int y = Options.year1; y <= Options.year2; y++) {
		for (int m = 1; m <= NMONTHS; m++) {
			if (month >= 0 && month != m)
				continue;
			if (index == 0) {
				/* Every day-of-week of month */
				mdays = days_in_month(m, y);
				dow_m1 = first_dayofweek_of_month(y, m);
				d = mod1(dow - dow_m1 + 8, 7);
				while (d <= mdays) {
					dp = find_ymd(y, m, d);
					if (dp != NULL) {
						if (count >= CAL_MAX_REPEAT) {
							warnx("%s: too many repeats",
							      __func__);
							return count;
						}
						dayp[count++] = dp;
					}
					d += 7;
				}
			} else {
				/* One indexed day-of-week of month */
				d = dayofweek_of_month(dow, index, m, y);
				if ((dp = find_ymd(y, m, d)) != NULL) {
					if (count >= CAL_MAX_REPEAT) {
						warnx("%s: too many repeats",
						      __func__);
						return count;
					}
					dayp[count++] = dp;
				}
			}
		}
	}

	return count;
}


/*
 * Return the days in the given month ($month) of year ($year)
 */
static int
days_in_month(int month, int year)
{
	static int mdays[][12] = {
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	};

	assert(month >= 0 && month <= 12);

	if (gregorian_leap_year(year))
		return mdays[1][month-1];
	else
		return mdays[0][month-1];
}

/*
 * Calculate the date of an indexed day-of-week in the month, e.g.,
 * 'Thu-3', 'Wed+2'.
 * The 'index' is the ordinal number of the day-of-week in the month.
 */
static int
dayofweek_of_month(int dow, int index, int month, int year)
{
	assert(index != 0);

	struct date date = { year, month, 1 };
	int day1 = fixed_from_gregorian(&date);

	if (index < 0) {	/* count back from the end of month */
		date.month++;
		date.day = 0;	/* the last day of previous month */
	}

	return nth_kday(index, dow, &date) - day1 + 1;
}
