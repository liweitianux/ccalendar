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
 * $FreeBSD: head/usr.bin/calendar/parsedata.c 326276 2017-11-27 15:37:16Z pfg $
 */

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "calendar.h"
#include "basics.h"
#include "chinese.h"
#include "ecclesiastical.h"
#include "gregorian.h"
#include "moon.h"
#include "parsedata.h"
#include "sun.h"
#include "utils.h"

struct dateinfo {
	int	flags;
	int	iyear;
	int	imonth;
	int	idayofmonth;
	int	idayofweek;
	int	imodifieroffset;
	int	imodifierindex;
	char	year[32];
	char	month[32];
	char	dayofmonth[32];
	char	dayofweek[32];
	char	modifieroffset[32];
	char	modifierindex[32];
	char	specialday[128];
};

struct yearinfo {
	struct yearinfo *next;
	int	*monthdays;  /* number of days in each month */
	int	year;
	int	ieaster;  /* day of year of Easter */
	int	ipaskha;  /* day of year of Paskha */
	int	firstcnyday;  /* day of year of ChineseNewYear */
	double	ffullmoon[MAXMOONS];
	double	fnewmoon[MAXMOONS];
	double	equinoxdays[2];
	double	solsticedays[2];
};
static struct yearinfo *yearinfo_list = NULL;

/* 1-based month, individual */
static int monthdaytab[][14] = {
	{ 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30 },
	{ 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30 },
};

static struct yearinfo *calc_yearinfo(int year);
static bool	 checkdayofweek(const char *s, size_t *len, int *offset,
				const char **dow);
static bool	 checkmonth(const char *s, size_t *len, int *offset,
			    const char **month);
static int	 dayofweek_of_month(int dow, int offset, int month, int year);
static bool	 determinestyle(const char *date, struct dateinfo *di);
static char	*floattotime(double f);
static const char *getdayofweekname(int i);
static const char *getmonthname(int i);
static bool	 isonlydigits(const char *s, bool nostar);
static bool	 parse_angle(const char *s, double *result);
static const char *parse_int_ranged(const char *s, size_t len, int min,
				    int max, int *result);
static bool	 parse_index(const char *s, int *index);
static void	 remember(int *index, struct cal_day **cd, struct cal_day *dp,
			  char **ed, const char *extra);
static void	 show_datestyle(struct dateinfo *di);
static char	*showflags(int flags);

/*
 * Expected styles:
 *
 * Date			::=	Month . ' ' . DayOfMonth |
 *				Month . ' ' . DayOfWeek . ModifierIndex |
 *				Month . '/' . DayOfMonth |
 *				Month . '/' . DayOfWeek . ModifierIndex |
 *				DayOfMonth . ' ' . Month |
 *				DayOfMonth . '/' . Month |
 *				DayOfWeek . ModifierIndex . ' ' . Month |
 *				DayOfWeek . ModifierIndex . '/' . Month |
 *				DayOfWeek . ModifierIndex |
 *				SpecialDay . ModifierOffset
 *
 * Month		::=	MonthName | MonthNumber | '*'
 * MonthNumber		::=	'0' ... '9' | '00' ... '09' | '10' ... '12'
 * MonthName		::=	MonthNameShort | MonthNameLong
 * MonthNameLong	::=	'January' ... 'December'
 * MonthNameShort	::=	'Jan' ... 'Dec' | 'Jan.' ... 'Dec.'
 *
 * DayOfWeek		::=	DayOfWeekShort | DayOfWeekLong
 * DayOfWeekShort	::=	'Mon' .. 'Sun'
 * DayOfWeekLong	::=	'Monday' .. 'Sunday'
 * DayOfMonth		::=	'0' ... '9' | '00' ... '09' | '10' ... '29' |
 *				'30' ... '31' | '*'
 *
 * ModifierOffset	::=	'' | '+' . ModifierNumber | '-' . ModifierNumber
 * ModifierNumber	::=	'0' ... '9' | '00' ... '99' | '000' ... '299' |
 *				'300' ... '359' | '360' ... '365'
 * ModifierIndex	::=	'First' | 'Second' | 'Third' | 'Fourth' |
 *				'Fifth' | 'Last'
 *
 * SpecialDay		::=	'Easter' | 'Paskha' | 'ChineseNewYear'
 *
 */
static bool
determinestyle(const char *date, struct dateinfo *di)
{
	char *date2 = xstrdup(date);
	char *p, *p1, *p2;
	const char *dow, *pmonth;
	size_t len;
	int offset;
	bool ret = false;

	if (Options.debug)
		fprintf(stderr, "-------\ndate: |%s|\n", date);

#define CHECKSPECIAL(s1, s2, type)						\
	if (string_startswith(s1, s2)) {					\
		di->flags |= (type | F_SPECIALDAY | F_VARIABLE);		\
		if (strlen(s1) == strlen(s2)) {					\
			strcpy(di->specialday, s1);				\
			ret = true;						\
			goto out;						\
		}								\
		strncpy(di->specialday, s1, strlen(s2));			\
		di->specialday[strlen(s2)] = '\0';				\
		strcpy(di->modifieroffset, s1 + strlen(s2));			\
		di->imodifieroffset = (int)strtol(di->modifieroffset, NULL, 10); \
		di->flags |= F_MODIFIEROFFSET;					\
		ret = true;							\
		goto out;							\
	}

	if ((p = strchr(date2, ' ')) == NULL &&
	    (p = strchr(date2, '/')) == NULL) {
		CHECKSPECIAL(date2, STRING_CNY, F_CNY);
		CHECKSPECIAL(date2, ncny, F_CNY);
		CHECKSPECIAL(date2, STRING_NEWMOON, F_NEWMOON);
		CHECKSPECIAL(date2, nnewmoon, F_NEWMOON);
		CHECKSPECIAL(date2, STRING_FULLMOON, F_FULLMOON);
		CHECKSPECIAL(date2, nfullmoon, F_FULLMOON);
		CHECKSPECIAL(date2, STRING_PASKHA, F_PASKHA);
		CHECKSPECIAL(date2, npaskha, F_PASKHA);
		CHECKSPECIAL(date2, STRING_EASTER, F_EASTER);
		CHECKSPECIAL(date2, neaster, F_EASTER);
		CHECKSPECIAL(date2, STRING_MAREQUINOX, F_MAREQUINOX);
		CHECKSPECIAL(date2, nmarequinox, F_SEPEQUINOX);
		CHECKSPECIAL(date2, STRING_SEPEQUINOX, F_SEPEQUINOX);
		CHECKSPECIAL(date2, nsepequinox, F_SEPEQUINOX);
		CHECKSPECIAL(date2, STRING_JUNSOLSTICE, F_JUNSOLSTICE);
		CHECKSPECIAL(date2, njunsolstice, F_JUNSOLSTICE);
		CHECKSPECIAL(date2, STRING_DECSOLSTICE, F_DECSOLSTICE);
		CHECKSPECIAL(date2, ndecsolstice, F_DECSOLSTICE);

		if (checkdayofweek(date2, &len, &offset, &dow)) {
			di->flags |= (F_DAYOFWEEK | F_VARIABLE);
			di->idayofweek = offset;
			if (strlen(date2) == len) {
				strcpy(di->dayofweek, date2);
				ret = true;
				goto out;
			}
			strncpy(di->dayofweek, date2, len);
			di->dayofweek[len] = '\0';
			strcpy(di->modifierindex, date2 + len);
			if (parse_index(di->modifierindex, &di->imodifierindex)) {
				di->flags |= F_MODIFIERINDEX;
				ret = true;
			}
			goto out;
		}
		if (isonlydigits(date2, true)) {
			/* Assume month number only */
			di->flags |= F_MONTH;
			di->imonth = (int)strtol(date2, NULL, 10);
			strcpy(di->month, getmonthname(di->imonth));
			ret = true;
			goto out;
		}

		goto out;
	}

	*p = '\0';
	p1 = date2;
	p2 = p + 1;
	/* Now p1 and p2 point to the first and second fields, respectively */

	if ((p = strchr(p2, '/')) != NULL) {
		/* Got a year in the date string. */
		di->flags |= F_YEAR;
		strcpy(di->year, p1);
		di->iyear = (int)strtol(di->year, NULL, 10);
		*p = '\0';
		p1 = p2;
		p2 = p + 1;
	}

	/* Check if there is a month string */
	if (checkmonth(p1, &len, &offset, &pmonth) ||
	    (checkmonth(p2, &len, &offset, &pmonth) && (p2 = p1))) {
		/* Now p2 is the non-month part */

		di->flags |= F_MONTH;
		di->imonth = offset;
		strcpy(di->month, getmonthname(offset));

		if (isonlydigits(p2, true)) {
			strcpy(di->dayofmonth, p2);
			di->idayofmonth = (int)strtol(p2, NULL, 10);
			di->flags |= F_DAYOFMONTH;
			ret = true;
			goto out;
		}
		if (strcmp(p2, "*") == 0) {
			di->flags |= F_ALLDAY;
			ret = true;
			goto out;
		}
		if (checkdayofweek(p2, &len, &offset, &dow)) {
			di->flags |= (F_DAYOFWEEK | F_VARIABLE);
			di->idayofweek = offset;
			strcpy(di->dayofweek, getdayofweekname(offset));
			if (strlen(p2) == len) {
				ret = true;
				goto out;
			}
			strcpy(di->modifierindex, p2 + len);
			if (parse_index(di->modifierindex, &di->imodifierindex)) {
				di->flags |= F_MODIFIERINDEX;
				ret = true;
			}
			goto out;
		}

		goto out;
	}

	/* Check if there is an every-month specifier */
	if ((strcmp(p1, "*") == 0 && isonlydigits(p2, true)) ||
	    (strcmp(p2, "*") == 0 && isonlydigits(p1, true) && (p2 = p1))) {
		di->flags |= (F_ALLMONTH | F_DAYOFMONTH);
		di->idayofmonth = (int)strtol(p2, NULL, 10);
		sprintf(di->dayofmonth, "%d", di->idayofmonth);
		ret = true;
		goto out;
	}

	/* Month as a number, then a weekday */
	if (isonlydigits(p1, true) &&
	    checkdayofweek(p2, &len, &offset, &dow)) {
		di->flags |= (F_MONTH | F_DAYOFWEEK | F_VARIABLE);

		di->imonth = (int)strtol(p1, NULL, 10);
		strcpy(di->month, getmonthname(di->imonth));

		di->idayofweek = offset;
		strcpy(di->dayofweek, getdayofweekname(offset));
		if (strlen(p2) == len) {
			ret = true;
			goto out;
		}
		strcpy(di->modifierindex, p2 + len);
		if (parse_index(di->modifierindex, &di->imodifierindex)) {
			di->flags |= F_MODIFIERINDEX;
			ret = true;
		}
		goto out;
	}

	/* Both the month and date are specified as numbers */
	if (isonlydigits(p1, true) && isonlydigits(p2, false)) {
		di->flags |= (F_MONTH | F_DAYOFMONTH);
		if (strchr(p2, '*') != NULL)
			di->flags |= F_VARIABLE;

		int m = (int)strtol(p1, NULL, 10);
		int d = (int)strtol(p2, NULL, 10);

		if (m > 12 && d > 12) {
			warnx("Invalid date: |%s|", date);
			goto out;
		}

		if (m > 12)
			swap(&m, &d);
		di->imonth = m;
		di->idayofmonth = d;
		strcpy(di->month, getmonthname(m));
		sprintf(di->dayofmonth, "%d", d);
		ret = true;
		goto out;
	}

	warnx("Unrecognized date: |%s|", date);

out:
	if (Options.debug)
		show_datestyle(di);
	free(date2);
	return ret;
}

static void
show_datestyle(struct dateinfo *di)
{
	fprintf(stderr, "flags: 0x%x - %s\n", di->flags, showflags(di->flags));
	if (di->modifieroffset[0] != '\0')
		fprintf(stderr, "modifieroffset: |%s| (%d)\n",
			di->modifieroffset, di->imodifieroffset);
	if (di->modifierindex[0] != '\0')
		fprintf(stderr, "modifierindex: |%s| (%d)\n",
			di->modifierindex, di->imodifierindex);
	if (di->year[0] != '\0')
		fprintf(stderr, "year: |%s| (%d)\n", di->year, di->iyear);
	if (di->month[0] != '\0')
		fprintf(stderr, "month: |%s| (%d)\n", di->month, di->imonth);
	if (di->dayofmonth[0] != '\0')
		fprintf(stderr, "dayofmonth: |%s| (%d)\n",
			di->dayofmonth, di->idayofmonth);
	if (di->dayofweek[0] != '\0')
		fprintf(stderr, "dayofweek: |%s| (%d)\n",
			di->dayofweek, di->idayofweek);
	if (di->specialday[0] != '\0')
		fprintf(stderr, "specialday: |%s|\n", di->specialday);
}

static char *
showflags(int flags)
{
	static char s[128];
	s[0] = '\0';

	if ((flags & F_YEAR) != 0)
		strcat(s, "year ");
	if ((flags & F_MONTH) != 0)
		strcat(s, "month ");
	if ((flags & F_DAYOFWEEK) != 0)
		strcat(s, "dayofweek ");
	if ((flags & F_DAYOFMONTH) != 0)
		strcat(s, "dayofmonth ");
	if ((flags & F_MODIFIERINDEX) != 0)
		strcat(s, "modifierindex ");
	if ((flags & F_MODIFIEROFFSET) != 0)
		strcat(s, "modifieroffset ");
	if ((flags & F_SPECIALDAY) != 0)
		strcat(s, "specialday ");
	if ((flags & F_ALLMONTH) != 0)
		strcat(s, "allmonth ");
	if ((flags & F_ALLDAY) != 0)
		strcat(s, "allday ");
	if ((flags & F_VARIABLE) != 0)
		strcat(s, "variable ");
	if ((flags & F_CNY) != 0)
		strcat(s, "chinesenewyear ");
	if ((flags & F_PASKHA) != 0)
		strcat(s, "paskha ");
	if ((flags & F_EASTER) != 0)
		strcat(s, "easter ");
	if ((flags & F_FULLMOON) != 0)
		strcat(s, "fullmoon ");
	if ((flags & F_NEWMOON) != 0)
		strcat(s, "newmoon ");
	if ((flags & F_MAREQUINOX) != 0)
		strcat(s, "marequinox ");
	if ((flags & F_SEPEQUINOX) != 0)
		strcat(s, "sepequinox ");
	if ((flags & F_JUNSOLSTICE) != 0)
		strcat(s, "junsolstice ");
	if ((flags & F_DECSOLSTICE) != 0)
		strcat(s, "decsolstice ");

	return s;
}

/*
 * Calculate the date of an indexed day-of-week in the month, e.g.,
 * 'Thu-3', 'Wed+2'.
 * 'index' is the ordinal number of the day-of-week in the month.
 */
static int
dayofweek_of_month(int dow, int index, int month, int year)
{
	struct yearinfo *yinfo;
	int dow1;
	int d;

	assert(index != 0);

	dow1 = first_dayofweek_of_month(year, month);
	if (dow1 == -1)  /* not in the date range */
		return -1;

	for (yinfo = yearinfo_list; yinfo; yinfo = yinfo->next) {
		if (yinfo->year == year)
			break;
	}
	assert(yinfo != NULL);

	/*
	 * Date of zeroth or first of our weekday in month, depending on the
	 * relationship with the first of the month.  The range is [-6, 6].
	 */
	d = (dow - dow1 + 1) % 7;
	/*
	 * Which way are we counting?  Index 0 is invalid, abs(index) > 5 is
	 * meaningless, but that's OK.  Index 5 may or may not be meaningless,
	 * so there's no point in complaining for complaining's sake.
	 */
	if (index < 0) {			/* back from end of month */
						/* FIXME */
		int dow2 = d;
		while (dow2 <= yinfo->monthdays[month])
			dow2 += 7;
		d = index * 7 + dow2;
	} else {
		if (d > 0)
			d += index * 7 - 7;
		else
			d += index * 7;
	}
	return (d);
}

int
parsedaymonth(const char *date, int *flags, struct cal_day **dayp,
	      char **edp, const char *line)
{
	struct dateinfo di = { 0 };
	struct cal_day *dp;
	struct yearinfo *yinfo;
	char *ed;
	int remindex, d, m, dow;

	di.flags = F_NONE;

	if (determinestyle(date, &di) == false) {
		warnx("Cannot determine style for date: |%s| in line |%s|",
				date, line);
		return (0);
	}

	*flags = di.flags;
	remindex = 0;

	for (int year = Options.year1; year <= Options.year2; year++) {
		if ((di.flags & F_YEAR) != 0 && di.iyear != year)
			continue;

		int lflags = di.flags & ~F_YEAR;

		/* Get important dates for this year */
		for (yinfo = yearinfo_list; yinfo; yinfo = yinfo->next) {
			if (yinfo->year == year)
				break;
		}
		if (yinfo == NULL) {
			yinfo = calc_yearinfo(year);
			yinfo->next = yearinfo_list;
			yearinfo_list = yinfo;
		}

		/* Specified month and day (fixed or variable) */
		if ((lflags & ~F_VARIABLE) == (F_MONTH | F_DAYOFMONTH)) {
			dp = find_ymd(year, di.imonth, di.idayofmonth);
			if (dp != NULL)
				remember(&remindex, dayp, dp, edp, NULL);
			continue;
		}

		/* Same day every month */
		if (lflags == (F_ALLMONTH | F_DAYOFMONTH)) {
			for (m = 1; m <= NMONTHS; m++) {
				dp = find_ymd(year, m, di.idayofmonth);
				if (dp != NULL)
					remember(&remindex, dayp, dp, edp, NULL);
			}
			continue;
		}

		/* Every day of a month */
		if (lflags == (F_ALLDAY | F_MONTH)) {
			for (d = 1; d <= yinfo->monthdays[di.imonth]; d++) {
				dp = find_ymd(year, di.imonth, d);
				if (dp != NULL)
					remember(&remindex, dayp, dp, edp, NULL);
			}
			continue;
		}

		/* One day of every month */
		if (lflags == (F_ALLMONTH | F_DAYOFWEEK)) {
			for (m = 1; m <= NMONTHS; m++) {
				dp = find_ymd(year, m, di.idayofmonth);
				if (dp != NULL)
					remember(&remindex, dayp, dp, edp, NULL);
			}
			continue;
		}

		/* Every day-of-week of the year, e.g., 'Thu' */
		if (lflags == (F_DAYOFWEEK | F_VARIABLE)) {
			dow = first_dayofweek_of_year(year);
			if (dow == -1)  /* not in the date range */
				continue;
			d = (di.idayofweek - dow + 8) % 7;
			while (d <= 366) {
				dp = find_yd(year, d);
				if (dp != NULL)
					remember(&remindex, dayp, dp, edp, NULL);
				d += 7;
			}
			continue;
		}

		/*
		 * One indexed day-of-week of every month of the year,
		 * e.g., 'Thu-3'
		 */
		if (lflags == (F_DAYOFWEEK | F_MODIFIERINDEX | F_VARIABLE)) {
			for (m = 1; m <= NMONTHS; m++) {
				d = dayofweek_of_month(di.idayofweek,
						di.imodifierindex, m, year);
				if (d == -1)  /* not in the date range */
					continue;
				dp = find_ymd(year, m, d);
				if (dp != NULL)
					remember(&remindex, dayp, dp, edp, NULL);
			}
			continue;
		}

		/*
		 * One indexed day-of-week of a month, e.g., 'Jan/Thu-3'
		 */
		if (lflags ==
		    (F_MONTH | F_DAYOFWEEK | F_MODIFIERINDEX | F_VARIABLE)) {
			d = dayofweek_of_month(di.idayofweek,
					di.imodifierindex, di.imonth, year);
			if (d == -1)  /* not in the date range */
				continue;
			dp = find_ymd(year, di.imonth, d);
			if (dp != NULL)
				remember(&remindex, dayp, dp, edp, NULL);
			continue;
		}

		/* Every day-of-week of the month, e.g., 'Jan/Thu' */
		if (lflags == (F_DAYOFWEEK | F_MONTH | F_VARIABLE)) {
			dow = first_dayofweek_of_month(year, di.imonth);
			if (dow == -1)  /* not in the date range */
				continue;
			d = (di.idayofweek - dow + 8) % 7;
			while (d <= yinfo->monthdays[di.imonth]) {
				dp = find_ymd(year, di.imonth, d);
				if (dp != NULL)
					remember(&remindex, dayp, dp, edp, NULL);
				d += 7;
			}
			continue;
		}

		/* Easter */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_EASTER)) {
			dp = find_yd(year, yinfo->ieaster + di.imodifieroffset);
			if (dp != NULL)
				remember(&remindex, dayp, dp, edp, NULL);
			continue;
		}

		/* Paskha */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_PASKHA)) {
			dp = find_yd(year, yinfo->ipaskha + di.imodifieroffset);
			if (dp != NULL)
				remember(&remindex, dayp, dp, edp, NULL);
			continue;
		}

		/* Chinese New Year */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_CNY)) {
			dp = find_yd(year, yinfo->firstcnyday + di.imodifieroffset);
			if (dp != NULL)
				remember(&remindex, dayp, dp, edp, NULL);
			continue;
		}

		/* FullMoon */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_FULLMOON)) {
			for (int i = 0; yinfo->ffullmoon[i] > 0; i++) {
				dp = find_yd(year,
					(int)floor(yinfo->ffullmoon[i]) +
					di.imodifieroffset);
				if (dp != NULL) {
					ed = floattotime(yinfo->ffullmoon[i]);
					remember(&remindex, dayp, dp, edp, ed);
				}
			}
			continue;
		}

		/* NewMoon */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_NEWMOON)) {
			for (int i = 0; yinfo->ffullmoon[i] > 0; i++) {
				dp = find_yd(year,
					(int)floor(yinfo->fnewmoon[i]) +
					di.imodifieroffset);
				if (dp != NULL) {
					ed = floattotime(yinfo->fnewmoon[i]);
					remember(&remindex, dayp, dp, edp, ed);
				}
			}
			continue;
		}

		/* (Mar|Sep)Equinox */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_MAREQUINOX)) {
			dp = find_yd(year,
				(int)yinfo->equinoxdays[0] + di.imodifieroffset);
			if (dp != NULL) {
				ed = floattotime(yinfo->equinoxdays[0]);
				remember(&remindex, dayp, dp, edp, ed);
			}
			continue;
		}
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_SEPEQUINOX)) {
			dp = find_yd(year,
				(int)yinfo->equinoxdays[1] + di.imodifieroffset);
			if (dp != NULL) {
				ed = floattotime(yinfo->equinoxdays[1]);
				remember(&remindex, dayp, dp, edp, ed);
			}
			continue;
		}

		/* (Jun|Dec)Solstice */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_JUNSOLSTICE)) {
			dp = find_yd(year,
				(int)yinfo->solsticedays[0] + di.imodifieroffset);
			if (dp != NULL) {
				ed = floattotime(yinfo->solsticedays[0]);
				remember(&remindex, dayp, dp, edp, ed);
			}
			continue;
		}
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_DECSOLSTICE)) {
			dp = find_yd(year,
				(int)yinfo->solsticedays[1] + di.imodifieroffset);
			if (dp != NULL) {
				ed = floattotime(yinfo->solsticedays[1]);
				remember(&remindex, dayp, dp, edp, ed);
			}
			continue;
		}

		warnx("%s(): unprocessed date: |%s| in line |%s|",
				__func__, date, line);
		if (Options.debug) {
			show_datestyle(&di);
		}
	}

	return remindex;
}

static struct yearinfo *
calc_yearinfo(int year)
{
	struct yearinfo *yinfo;
	struct date date = { year - 1, 12, 31 };
	double t, t_begin, t_end;
	int i, day0, day_approx;

	day0 = fixed_from_gregorian(&date);

	yinfo = xcalloc(1, sizeof(*yinfo));
	yinfo->year = year;
	yinfo->monthdays = monthdaytab[isleap(year)];

	yinfo->ieaster = easter(year) - day0;
	yinfo->ipaskha = orthodox_easter(year) - day0;
	yinfo->firstcnyday = chinese_new_year(year) - day0;

	/*
	 * Lunar events
	 */
	date.year = year;
	date.month = 1;
	date.day = 1;
	t_begin = fixed_from_gregorian(&date) - Options.location->zone;
	date.year++;
	t_end = fixed_from_gregorian(&date) - Options.location->zone;
	/* New moon events */
	for (t = t_begin, i = 0; (t = new_moon_atafter(t)) < t_end; i++) {
		t += Options.location->zone;  /* to standard time */
		yinfo->fnewmoon[i] = t - day0;
	}
	/* Full moon events */
	for (t = t_begin, i = 0;
	     (t = lunar_phase_atafter(180, t)) < t_end;
	     i++) {
		t += Options.location->zone;  /* to standard time */
		yinfo->ffullmoon[i] = t - day0;
	}

	/*
	 * Solar events
	 */
	date.year = year;
	/* March Equinox, 0 degree */
	date.month = 3;
	date.day = 1;
	day_approx = fixed_from_gregorian(&date);
	t = solar_longitude_atafter(0, day_approx) + Options.location->zone;
	yinfo->equinoxdays[0] = t - day0;
	/* June Solstice, 90 degree */
	date.month = 6;
	date.day = 1;
	day_approx = fixed_from_gregorian(&date);
	t = solar_longitude_atafter(90, day_approx) + Options.location->zone;
	yinfo->solsticedays[0] = t - day0;
	/* September Equinox, 180 degree */
	date.month = 9;
	date.day = 1;
	day_approx = fixed_from_gregorian(&date);
	t = solar_longitude_atafter(180, day_approx) + Options.location->zone;
	yinfo->equinoxdays[1] = t - day0;
	/* December Solstice, 270 degree */
	date.month = 12;
	date.day = 1;
	day_approx = fixed_from_gregorian(&date);
	t = solar_longitude_atafter(270, day_approx) + Options.location->zone;
	yinfo->solsticedays[1] = t - day0;

	return yinfo;
}

static void
remember(int *index, struct cal_day **cd, struct cal_day *dp,
	 char **ed, const char *extra)
{
	static bool warned = false;

	if (*index >= MAXCOUNT - 1) {
		if (!warned)
			warnx("Event count exceeds %d, ignored", MAXCOUNT);
		warned = true;
		return;
	}

	cd[*index] = dp;
	if (ed[*index] != NULL) {
		free(ed[*index]);
		ed[*index] = NULL;
	}
	/* NOTE: copy so that multiple occurrences of events (e.g., NewMoon)
	 * would not overwrite */
	if (extra != NULL)
		ed[*index] = xstrdup(extra);

	(*index)++;
}

static const char *
getmonthname(int i)
{
	if (i <= 0 || i > NMONTHS)
		errx(1, "Invalid month number: %d", i);

	return (nmonths[i-1] ? nmonths[i-1] : months[i-1]);
}

static bool
checkmonth(const char *s, size_t *len, int *offset, const char **month)
{
	const char *p;
	size_t l;

	for (int i = 0; fnmonths[i]; i++) {
		p = fnmonths[i];
		l = strlen(p);
		if (strncasecmp(s, p, l) == 0) {
			*len = l;
			*month = p;
			*offset = i + 1;
			return (true);
		}
	}
	for (int i = 0; nmonths[i]; i++) {
		p = nmonths[i];
		l = strlen(p);
		if (strncasecmp(s, p, l) == 0) {
			*len = l;
			*month = p;
			*offset = i + 1;
			return (true);
		}
	}
	for (int i = 0; fmonths[i]; i++) {
		p = fmonths[i];
		l = strlen(p);
		if (strncasecmp(s, p, l) == 0) {
			*len = l;
			*month = p;
			*offset = i + 1;
			return (true);
		}
	}
	for (int i = 0; months[i]; i++) {
		p = months[i];
		l = strlen(p);
		if (strncasecmp(s, p, l) == 0) {
			*len = l;
			*month = p;
			*offset = i + 1;
			return (true);
		}
	}
	return (false);
}

static const char *
getdayofweekname(int i)
{
	if (i < 0 || i >= NDAYS)
		errx(1, "Invalid day-of-week number: %d", i);

	return (ndays[i] ? ndays[i] : days[i]);
}

static bool
checkdayofweek(const char *s, size_t *len, int *offset, const char **dow)
{
	const char *p;
	size_t l;

	for (int i = 0; fndays[i]; i++) {
		p = fndays[i];
		l = strlen(p);
		if (strncasecmp(s, p, l) == 0) {
			*len = l;
			*dow = p;
			*offset = i;
			return (true);
		}
	}
	for (int i = 0; ndays[i]; i++) {
		p = ndays[i];
		l = strlen(p);
		if (strncasecmp(s, p, l) == 0) {
			*len = l;
			*dow = p;
			*offset = i;
			return (true);
		}
	}
	for (int i = 0; fdays[i]; i++) {
		p = fdays[i];
		l = strlen(p);
		if (strncasecmp(s, p, l) == 0) {
			*len = l;
			*dow = p;
			*offset = i;
			return (true);
		}
	}
	for (int i = 0; days[i]; i++) {
		p = days[i];
		l = strlen(p);
		if (strncasecmp(s, p, l) == 0) {
			*len = l;
			*dow = p;
			*offset = i;
			return (true);
		}
	}
	return (false);
}

static bool
isonlydigits(const char *s, bool nostar)
{
	for (int i = 0; s[i] != '\0'; i++) {
		if (nostar == false && s[i] == '*' && s[i+1] == '\0')
			return (true);
		if (!isdigit((unsigned char)s[i]))
			return (false);
	}
	return (true);
}

static bool
parse_index(const char *s, int *index)
{
	bool parsed = false;

	if (s[0] == '+' || s[0] == '-') {
		char *endp;
		int idx = (int)strtol(s, &endp, 10);
		if (*endp != '\0')
			return false;  /* has trailing junk */
		if (idx == 0 || idx <= -6 || idx >= 6) {
			warnx("%s(): out of valid range: %d", __func__, idx);
			return false;  /* out of valid range */
		}

		*index = idx;
		parsed = true;
	}

	for (int i = 0; !parsed && sequences[i]; i++) {
		if (strcasecmp(s, sequences[i]) == 0) {
			if (i == 5) {  /* sequence is 'last' */
				*index = -1;
				parsed = true;
			}
			*index = i + 1;
			parsed = true;
		}
	}
	for (int i = 0; !parsed && nsequences[i]; i++) {
		if (strcasecmp(s, nsequences[i]) == 0) {
			if (i == 5) {  /* sequence is 'last' */
				*index = -1;
				parsed = true;
			}
			*index = i + 1;
			parsed = true;
		}
	}

	return parsed;
}

static char *
floattotime(double f)
{
	static char buf[100];
	int hh, mm, ss, i;

	f -= floor(f);
	i = (int)round(f * SECSPERDAY);

	hh = i / SECSPERHOUR;
	i %= SECSPERHOUR;
	mm = i / SECSPERMINUTE;
	i %= SECSPERMINUTE;
	ss = i;

	sprintf(buf, "%02d:%02d:%02d", hh, mm, ss);
	return (buf);
}


/*
 * Parse the specified length of a string to an integer and check its range.
 * Return the pointer to the next character of the parsed string on success,
 * otherwise return NULL.
 */
static const char *
parse_int_ranged(const char *s, size_t len, int min, int max, int *result)
{
	if (strlen(s) < len)
		return NULL;

	const char *end = s + len;
	int v = 0;
	while (s < end) {
		if (isdigit((unsigned char)*s) == 0)
			return NULL;
		v = 10 * v + (*s - '0');
		s++;
	}

	if (v < min || v > max)
		return NULL;

	*result = v;
	return end;
}

/*
 * Parse the timezone string (format: ±hh:mm, ±hhmm, or ±hh) to the number
 * of seconds east of UTC.
 * Return true on success, otherwise false.
 */
bool
parse_timezone(const char *s, int *result)
{
	if (*s != '+' && *s != '-')
		return false;
	char sign = *s++;

	int hh = 0;
	int mm = 0;
	if ((s = parse_int_ranged(s, 2, 0, 23, &hh)) == NULL)
		return false;
	if (*s != '\0') {
		if (*s == ':')
			s++;
		if ((s = parse_int_ranged(s, 2, 0, 59, &mm)) == NULL)
			return false;
	}
	if (*s != '\0')
		return false;

	int offset = hh * 3600L + mm * 60L;
	*result = (sign == '+') ? offset : -offset;

	return true;
}

/*
 * Parse a angle/coordinate string in format of a single float number or
 * [+-]d:m:s, where 'd' and 'm' are degrees and minutes of integer type,
 * and 's' is seconds of float type.
 * Return true on success, otherwise false.
 */
static bool
parse_angle(const char *s, double *result)
{
	char sign = '+';
	if (*s == '+' || *s == '-')
		sign = *s++;

	char *endp;
	double v;
	v = strtod(s, &endp);
	if (s == endp || *endp != '\0') {
		/* try to parse in format: d:m:s */
		int deg = 0;
		int min = 0;
		double sec = 0.0;
		switch (sscanf(s, "%d:%d:%lf", &deg, &min, &sec)) {
		case 3:
		case 2:
		case 1:
			if (min < 0 || min > 60 || sec < 0 || sec > 60)
				return false;
			v = deg + min / 60.0 + sec / 3600.0;
			break;
		default:
			return false;
		}
	}

	*result = (sign == '+') ? v : -v;
	return true;
}

/*
 * Parse location of format: latitude,longitude[,elevation]
 * where 'latitude' and 'longitude' can be represented as a float number or
 * in '[+-]d:m:s' format, and 'elevation' is optional and should be a
 * positive float number.
 * Return true on success, otherwise false.
 */
bool
parse_location(const char *s, double *latitude, double *longitude,
	       double *elevation)
{
	char *ds = xstrdup(s);
	const char *sep = ",";
	char *p;
	double v;

	p = strtok(ds, sep);
	if (parse_angle(p, &v) && fabs(v) <= 90) {
		*latitude = v;
	} else {
		warnx("%s: invalid latitude: '%s'", __func__, p);
		return false;
	}

	p = strtok(NULL, sep);
	if (p == NULL) {
		warnx("%s: missing longitude", __func__);
		return false;
	}
	if (parse_angle(p, &v) && fabs(v) <= 180) {
		*longitude = v;
	} else {
		warnx("%s: invalid longitude: '%s'", __func__, p);
		return false;
	}

	p = strtok(NULL, sep);
	if (p != NULL) {
		char *endp;
		v = strtod(p, &endp);
		if (p == endp || *endp != '\0' || v < 0) {
			warnx("%s: invalid elevation: '%s'", __func__, p);
			return false;
		}
		*elevation = v;
	}

	if ((p = strtok(NULL, sep)) != NULL) {
		warnx("%s: unknown value: '%s'", __func__, p);
		return false;
	}

	return true;
}

/*
 * Parse date string of format '[[[cc]yy]mm]dd' into a fixed date.
 * Return true on success, otherwise false.
 */
bool
parse_date(const char *date, int *rd_out)
{
	size_t len;
	time_t now;
	struct tm tm;
	struct date gdate;

	now = time(NULL);
	tzset();
	localtime_r(&now, &tm);

	gdate.year = tm.tm_year + 1900;
	gdate.month = tm.tm_mon + 1;
	gdate.day = tm.tm_mday;

	len = strlen(date);
	if (len < 2)
		return false;

	if (!parse_int_ranged(date+len-2, 2, 1, 31, &gdate.day))
		return false;

	if (len >= 4) {
		if (!parse_int_ranged(date+len-4, 2, 1, 12, &gdate.month))
			return false;
	}

	if (len >= 6) {
		if (!parse_int_ranged(date, len-4, 0, 9999, &gdate.year))
			return false;
		if (gdate.year < 69)  /* Y2K */
			gdate.year += 2000;
		else if (gdate.year < 100)
			gdate.year += 1900;
	}

	*rd_out = fixed_from_gregorian(&gdate);

	logdebug("%s(): |%s| -> %04d-%02d-%02d\n",
		 __func__, date, gdate.year, gdate.month, gdate.day);
	return true;
}
