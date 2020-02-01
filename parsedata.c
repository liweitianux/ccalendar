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

#include "calendar.h"
#include "parsedata.h"
#include "utils.h"

struct yearinfo {
	int year;
	int ieaster;
	int ipaskha;
	int firstcnyday;
	double ffullmoon[MAXMOONS];
	double fnewmoon[MAXMOONS];
	double ffullmooncny[MAXMOONS];
	double fnewmooncny[MAXMOONS];
	int ichinesemonths[MAXMOONS];
	double equinoxdays[2];
	double solsticedays[2];
	int *monthdays;
	struct yearinfo *next;
};
static struct yearinfo *yearinfo_list = NULL;

static struct yearinfo *calc_yearinfo(int year);
static bool	 checkdayofweek(const char *s, size_t *len, int *offset,
				const char **dow);
static bool	 checkmonth(const char *s, size_t *len, int *offset,
			    const char **month);
static int	 dayofweek_of_month(int day, int offset, int month, int year);
static bool	 determinestyle(const char *date, int *flags,
				char *month, int *imonth,
				char *dayofmonth, int *idayofmonth,
				char *dayofweek, int *idayofweek,
				char *modifieroffset, char *modifierindex,
				char *specialday, char *year, int *iyear);
static char	*floattoday(int year, double f);
static char	*floattotime(double f);
static const char *getdayofweekname(int i);
static const char *getmonthname(int i);
static bool	 isonlydigits(const char *s, bool nostar);
static bool	 parse_angle(const char *s, double *result);
static const char *parse_int_ranged(const char *s, size_t len, int min,
				    int max, int *result);
static int	 parse_index(const char *s);
static void	 remember(int *index, int *y, int *m, int *d, char **ed,
			  int yy, int mm, int dd, const char *extra);
static void	 show_datestyle(int flags, const char *month, int imonth,
				const char *dayofmonth, int idayofmonth,
				const char *dayofweek, int idayofweek,
				const char *modifieroffset,
				const char *modifierindex,
				const char *specialday,
				const char *year, int iyear);
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
determinestyle(const char *date, int *flags,
	       char *month, int *imonth, char *dayofmonth, int *idayofmonth,
	       char *dayofweek, int *idayofweek, char *modifieroffset,
	       char *modifierindex, char *specialday, char *year, int *iyear)
{
	char *date2;
	char *p, *p1, *p2;
	const char *dow, *pmonth;
	size_t len;
	int offset;
	bool ret = false;

	date2 = xstrdup(date);
	*flags = F_NONE;
	*month = '\0';
	*imonth = 0;
	*year = '\0';
	*iyear = 0;
	*dayofmonth = '\0';
	*idayofmonth = 0;
	*dayofweek = '\0';
	*idayofweek = 0;
	*modifieroffset = '\0';
	*modifierindex = '\0';
	*specialday = '\0';

	if (debug)
		fprintf(stderr, "-------\ndate: |%s|\n", date);

#define CHECKSPECIAL(s1, s2, type)					\
	if (string_eqn(s1, s2)) {					\
		*flags |= (type | F_SPECIALDAY | F_VARIABLE);		\
		if (strlen(s1) == strlen(s2)) {				\
			strcpy(specialday, s1);				\
			ret = true;					\
			goto out;					\
		}							\
		strncpy(specialday, s1, strlen(s2));			\
		specialday[strlen(s2)] = '\0';				\
		strcpy(modifieroffset, s1 + strlen(s2));		\
		*flags |= F_MODIFIEROFFSET;				\
		ret = true;						\
		goto out;						\
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
			*flags |= (F_DAYOFWEEK | F_VARIABLE);
			*idayofweek = offset;
			if (strlen(date2) == len) {
				strcpy(dayofweek, date2);
				ret = true;
				goto out;
			}
			strncpy(dayofweek, date2, len);
			dayofweek[len] = '\0';
			strcpy(modifierindex, date2 + len);
			*flags |= F_MODIFIERINDEX;
			ret = true;
			goto out;
		}
		if (isonlydigits(date2, true)) {
			/* Assume month number only */
			*flags |= F_MONTH;
			*imonth = (int)strtol(date2, NULL, 10);
			strcpy(month, getmonthname(*imonth));
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
		/* We have a year in the string. Now this is getting tricky */
		strcpy(year, p1);
		*iyear = (int)strtol(year, NULL, 10);
		*p = '\0';
		p1 = p2;
		p2 = p + 1;
		*flags |= F_YEAR;
	}

	/* Check if there is a month-string in the date */
	if (checkmonth(p1, &len, &offset, &pmonth) ||
	    (checkmonth(p2, &len, &offset, &pmonth) && (p2 = p1))) {
		/* p2 is the non-month part */
		*flags |= F_MONTH;
		*imonth = offset;
		strcpy(month, getmonthname(offset));
		if (isonlydigits(p2, true)) {
			strcpy(dayofmonth, p2);
			*idayofmonth = (int)strtol(p2, NULL, 10);
			*flags |= F_DAYOFMONTH;
			ret = true;
			goto out;
		}
		if (strcmp(p2, "*") == 0) {
			*flags |= F_ALLDAY;
			ret = true;
			goto out;
		}
		if (checkdayofweek(p2, &len, &offset, &dow)) {
			*flags |= (F_DAYOFWEEK | F_VARIABLE);
			*idayofweek = offset;
			strcpy(dayofweek, getdayofweekname(offset));
			if (strlen(p2) == len) {
				ret = true;
				goto out;
			}
			strcpy(modifierindex, p2 + len);
			*flags |= F_MODIFIERINDEX;
			ret = true;
			goto out;
		}

		goto out;
	}

	/* Check if there is an every-day or every-month in the string */
	if ((strcmp(p1, "*") == 0 && isonlydigits(p2, true)) ||
	    (strcmp(p2, "*") == 0 && isonlydigits(p1, true) && (p2 = p1))) {
		*flags |= (F_ALLMONTH | F_DAYOFMONTH);
		*idayofmonth = (int)strtol(p2, NULL, 10);
		sprintf(dayofmonth, "%d", *idayofmonth);
		ret = true;
		goto out;
	}

	/* Month as a number, then a weekday */
	if (isonlydigits(p1, true) &&
	    checkdayofweek(p2, &len, &offset, &dow)) {
		*flags |= (F_MONTH | F_DAYOFWEEK | F_VARIABLE);
		*idayofweek = offset;
		*imonth = (int)strtol(p1, NULL, 10);
		strcpy(month, getmonthname(*imonth));
		strcpy(dayofweek, getdayofweekname(offset));
		if (strlen(p2) == len) {
			ret = true;
			goto out;
		}
		strcpy(modifierindex, p2 + len);
		*flags |= F_MODIFIERINDEX;
		ret = true;
		goto out;
	}

	/* If both the month and date are specified as numbers */
	if (isonlydigits(p1, true) && isonlydigits(p2, false)) {
		/* Now who wants to be this ambiguous? :-( */
		*flags |= (F_MONTH | F_DAYOFMONTH);
		if (strchr(p2, '*') != NULL)
			*flags |= F_VARIABLE;

		int m = (int)strtol(p1, NULL, 10);
		int d = (int)strtol(p2, NULL, 10);

		if (m > 12 && d > 12) {
			warnx("Invalid date: |%s|", date);
			goto out;
		}

		if (m > 12)
			swap(&m, &d);
		*imonth = m;
		*idayofmonth = d;
		strcpy(month, getmonthname(m));
		sprintf(dayofmonth, "%d", d);
		ret = true;
		goto out;
	}

	warnx("Unrecognized date: |%s|", date);

out:
	if (debug) {
		show_datestyle(*flags, month, *imonth, dayofmonth,
			       *idayofmonth, dayofweek, *idayofweek,
			       modifieroffset, modifierindex, specialday,
			       year, *iyear);
	}
	free(date2);
	return ret;
}

static void
show_datestyle(int flags, const char *month, int imonth,
	       const char *dayofmonth, int idayofmonth,
	       const char *dayofweek, int idayofweek,
	       const char *modifieroffset, const char *modifierindex,
	       const char *specialday, const char *year, int iyear)
{
	fprintf(stderr, "flags: 0x%x - %s\n", flags, showflags(flags));
	if (modifieroffset[0] != '\0')
		fprintf(stderr, "modifieroffset: |%s|\n", modifieroffset);
	if (modifierindex[0] != '\0')
		fprintf(stderr, "modifierindex: |%s|\n", modifierindex);
	if (year[0] != '\0')
		fprintf(stderr, "year: |%s| (%d)\n", year, iyear);
	if (month[0] != '\0')
		fprintf(stderr, "month: |%s| (%d)\n", month, imonth);
	if (dayofmonth[0] != '\0')
		fprintf(stderr, "dayofmonth: |%s| (%d)\n",
			dayofmonth, idayofmonth);
	if (dayofweek[0] != '\0')
		fprintf(stderr, "dayofweek: |%s| (%d)\n",
			dayofweek, idayofweek);
	if (specialday[0] != '\0')
		fprintf(stderr, "specialday: |%s|\n", specialday);
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

/*
 * Possible date formats include any combination of:
 *	3-charmonth			(January, Jan, Jan)
 *	3-charweekday			(Friday, Monday, mon.)
 *	numeric month or day		(1, 2, 04)
 *
 * Any character may separate them, or they may not be separated.  Any line,
 * following a line that is matched, that starts with "whitespace", is shown
 * along with the matched line.
 */
int
parsedaymonth(const char *date, int *yearp, int *monthp, int *dayp,
	      int *flags, char **edp)
{
	struct cal_day *dp;
	struct yearinfo *yinfo;
	char month[100], dayofmonth[100], dayofweek[100], modifieroffset[100];
	char syear[100], modifierindex[100], specialday[100];
	int idayofweek = -1, imonth = -1, idayofmonth = -1, iyear = -1;
	int remindex;
	int d, m, dow, rm, rd, offset, index;
	char *ed;
	int retvalsign = 1;

	/*
	 * CONVENTION
	 *
	 * Month:     1-12
	 * Monthname: Jan .. Dec
	 * Day:       1-31
	 * Weekday:   Mon .. Sun
	 */

	*flags = 0;

	if (determinestyle(date, flags, month, &imonth, dayofmonth,
			   &idayofmonth, dayofweek, &idayofweek, modifieroffset,
			   modifierindex, specialday, syear, &iyear) == false) {
		warnx("Cannot determine style for date: |%s|", date);
		return (0);
	}

	remindex = 0;
	for (int year = year1; year <= year2; year++) {
		int lflags = *flags;

		if ((lflags & F_YEAR) != 0 && iyear != year)
			continue;

		lflags &= ~F_YEAR;

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
		if (lflags == (F_MONTH | F_DAYOFMONTH) ||
		    lflags == (F_MONTH | F_DAYOFMONTH | F_VARIABLE)) {
			if (find_ymd(year, imonth, idayofmonth) == NULL)
				continue;
			remember(&remindex, yearp, monthp, dayp, edp,
				 year, imonth, idayofmonth, NULL);
			continue;
		}

		/* Same day every month */
		if (lflags == (F_ALLMONTH | F_DAYOFMONTH)) {
			for (m = 1; m <= 12; m++) {
				if (find_ymd(year, m, idayofmonth) == NULL)
					continue;
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, m, idayofmonth, NULL);
			}
			continue;
		}

		/* Every day of a month */
		if (lflags == (F_ALLDAY | F_MONTH)) {
			for (d = 1; d <= yinfo->monthdays[imonth]; d++) {
				if (find_ymd(year, imonth, d) == NULL)
					continue;
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, imonth, d, NULL);
			}
			continue;
		}

		/* One day of every month */
		if (lflags == (F_ALLMONTH | F_DAYOFWEEK)) {
			for (m = 1; m <= 12; m++) {
				if (find_ymd(year, m, idayofmonth) == NULL)
					continue;
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, m, idayofmonth, NULL);
			}
			continue;
		}

		/* Every dayofweek of the year */
		if (lflags == (F_DAYOFWEEK | F_VARIABLE)) {
			dow = first_dayofweek_of_year(year);
			if (dow == -1)  /* not in the date range */
				continue;
			d = (idayofweek - dow + 8) % 7;
			while (d <= 366) {
				if ((dp = find_yd(year, d))) {
					rm = dp->month->month;
					rd = dp->dayofmonth;
					remember(&remindex,
					    yearp, monthp, dayp, edp,
					    year, rm, rd, NULL);
				}
				d += 7;
			}
			continue;
		}

		/*
		 * One indexed day-of-week of every month of the year,
		 * e.g., 'Thu-3'
		 */
		if (lflags == (F_DAYOFWEEK | F_MODIFIERINDEX | F_VARIABLE)) {
			index = parse_index(modifierindex);
			for (m = 1; m <= NMONTHS; m++) {
				d = dayofweek_of_month(idayofweek, index,
						       m, year);
				if (d == -1)  /* not in the date range */
					continue;
				if (find_ymd(year, m, d)) {
					remember(&remindex,
					    yearp, monthp, dayp, edp,
					    year, m, d, NULL);
					continue;
				}
			}
			continue;
		}

		/*
		 * One indexed day-of-week of a month, e.g., 'Jan/Thu-3'
		 */
		if (lflags ==
		    (F_MONTH | F_DAYOFWEEK | F_MODIFIERINDEX | F_VARIABLE)) {
			index = parse_index(modifierindex);
			d = dayofweek_of_month(idayofweek, index,
					       imonth, year);
			if (d == -1)  /* not in the date range */
				continue;
			if (find_ymd(year, imonth, d)) {
				remember(&remindex, yearp, monthp,
					 dayp, edp, year, imonth, d, NULL);
				continue;
			}
			continue;
		}

		/* Every dayofweek of the month */
		if (lflags == (F_DAYOFWEEK | F_MONTH | F_VARIABLE)) {
			dow = first_dayofweek_of_month(year, imonth);
			if (dow == -1)  /* not in the date range */
				continue;
			d = (idayofweek - dow + 8) % 7;
			while (d <= yinfo->monthdays[imonth]) {
				if (find_ymd(year, imonth, d)) {
					remember(&remindex,
					    yearp, monthp, dayp, edp,
					    year, imonth, d, NULL);
				}
				d += 7;
			}
			continue;
		}

		/* Easter */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_EASTER)) {
			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			if ((dp = find_yd(year, yinfo->ieaster + offset))) {
				rm = dp->month->month;
				rd = dp->dayofmonth;
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, rm, rd, NULL);
			}
			continue;
		}

		/* Paskha */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_PASKHA)) {
			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			if ((dp = find_yd(year, yinfo->ipaskha + offset))) {
				rm = dp->month->month;
				rd = dp->dayofmonth;
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, rm, rd, NULL);
			}
			continue;
		}

		/* Chinese New Year */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_CNY)) {
			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			if ((dp = find_yd(year, yinfo->firstcnyday + offset))) {
				rm = dp->month->month;
				rd = dp->dayofmonth;
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, rm, rd, NULL);
			}
			continue;
		}

		/* FullMoon */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_FULLMOON)) {
			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			for (int i = 0; yinfo->ffullmoon[i] > 0; i++) {
				dp = find_yd(year, (int)floor(yinfo->ffullmoon[i]) + offset);
				if (dp) {
					rm = dp->month->month;
					rd = dp->dayofmonth;
					ed = floattotime(yinfo->ffullmoon[i]);
					remember(&remindex,
					    yearp, monthp, dayp, edp,
					    year, rm, rd, ed);
				}
			}
			continue;
		}

		/* NewMoon */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_NEWMOON)) {
			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			for (int i = 0; yinfo->ffullmoon[i] > 0; i++) {
				dp = find_yd(year, (int)floor(yinfo->fnewmoon[i]) + offset);
				if (dp) {
					rm = dp->month->month;
					rd = dp->dayofmonth;
					ed = floattotime(yinfo->fnewmoon[i]);
					remember(&remindex,
					    yearp, monthp, dayp, edp,
					    year, rm, rd, ed);
				}
			}
			continue;
		}

		/* (Mar|Sep)Equinox */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_MAREQUINOX)) {
			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			dp = find_yd(year, (int)yinfo->equinoxdays[0] + offset);
			if (dp) {
				rm = dp->month->month;
				rd = dp->dayofmonth;
				ed = floattotime(yinfo->equinoxdays[0]);
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, rm, rd, ed);
			}
			continue;
		}
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_SEPEQUINOX)) {
			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			dp = find_yd(year, (int)yinfo->equinoxdays[1] + offset);
			if (dp) {
				rm = dp->month->month;
				rd = dp->dayofmonth;
				ed = floattotime(yinfo->equinoxdays[1]);
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, rm, rd, ed);
			}
			continue;
		}

		/* (Jun|Dec)Solstice */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_JUNSOLSTICE)) {
			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			dp = find_yd(year, (int)yinfo->solsticedays[0] + offset);
			if (dp) {
				rm = dp->month->month;
				rd = dp->dayofmonth;
				ed = floattotime(yinfo->solsticedays[0]);
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, rm, rd, ed);
			}
			continue;
		}
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_DECSOLSTICE)) {
			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			dp = find_yd(year, (int)yinfo->solsticedays[1] + offset);
			if (dp) {
				rm = dp->month->month;
				rd = dp->dayofmonth;
				ed = floattotime(yinfo->solsticedays[1]);
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, rm, rd, ed);
			}
			continue;
		}

		if (debug) {
			fprintf(stderr, "Unprocessed:\n");
			fprintf(stderr, "date: |%s|\n", date);
			show_datestyle(lflags, month, imonth,
				       dayofmonth, idayofmonth, dayofweek,
				       idayofweek, modifieroffset,
				       modifierindex, specialday,
				       syear, iyear);
		}

		retvalsign = -1;
	}

	if (retvalsign == -1)
		return (-remindex - 1);
	else
		return (remindex);
}

static struct yearinfo *
calc_yearinfo(int year)
{
	struct yearinfo *yinfo = xcalloc(1, sizeof(*yinfo));

	yinfo->year = year;
	yinfo->monthdays = monthdaytab[isleap(year)];
	yinfo->ieaster = easter(year);
	yinfo->ipaskha = paskha(year);
	fpom(year, UTCOffset, yinfo->ffullmoon, yinfo->fnewmoon);
	fpom(year, UTCOFFSET_CNY, yinfo->ffullmooncny, yinfo->fnewmooncny);
	fequinoxsolstice(year, UTCOffset, yinfo->equinoxdays,
			 yinfo->solsticedays);

	/* CNY: Match day with sun longitude at 330° with new moon */
	yinfo->firstcnyday = calculatesunlongitude30(year,
	    UTCOFFSET_CNY, yinfo->ichinesemonths);
	for (int m = 0; yinfo->fnewmooncny[m] >= 0; m++) {
		if (yinfo->fnewmooncny[m] > yinfo->firstcnyday) {
			yinfo->firstcnyday =
				(int)floor(yinfo->fnewmooncny[m - 1]);
			break;
		}
	}

	return yinfo;
}

static void
remember(int *index, int *y, int *m, int *d, char **ed,
	 int yy, int mm, int dd, const char *extra)
{
	static bool warned = false;

	if (*index >= MAXCOUNT - 1) {
		if (!warned)
			warnx("Event count exceeds %d, ignored", MAXCOUNT);
		warned = true;
		return;
	}

	y[*index] = yy;
	m[*index] = mm;
	d[*index] = dd;

	if (ed[*index] != NULL) {
		free(ed[*index]);
		ed[*index] = NULL;
	}
	if (extra != NULL)
		ed[*index] = xstrdup(extra);

	*index += 1;
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

static int
parse_index(const char *s)
{
	int i;
	char *es;
	const char *p;

	if (s[0] == '+' || s[0] == '-') {
		i = (int)strtol(s, &es, 10);
		if (*es != '\0')                      /* trailing junk */
			errx (1, "Invalid specifier format: %s\n", s);
		return (i);
	}

	for (i = 0; sequences[i]; i++) {
		if (strcasecmp(s, sequences[i]) == 0) {
			if (i == 5)
				return (-1);
			return (i + 1);
		}
	}
	for (i = 0; nsequences[i]; i++) {
		p = nsequences[i];
		if (strncasecmp(s, p, strlen(p)) == 0) {
			if (i == 5)
				return (-1);
			return (i + 1);
		}
	}
	return (0);
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

static char *
floattoday(int year, double f)
{
	static char buf[100];
	int i, m, d, hh, mm, ss;
	int *cumdays = cumdaytab[isleap(year)];

	for (i = 0; 1 + cumdays[i] < f; i++)
		;
	m = --i;
	d = (int)floor(f - 1 - cumdays[i]);
	f -= floor(f);
	i = (int)round(f * SECSPERDAY);

	hh = i / SECSPERHOUR;
	i %= SECSPERHOUR;
	mm = i / SECSPERMINUTE;
	i %= SECSPERMINUTE;
	ss = i;

	sprintf(buf, "%02d-%02d %02d:%02d:%02d", m, d, hh, mm, ss);
	return (buf);
}

void
dodebug(const char *type)
{
	int year;

	printf("UTCOffset: %g\n", UTCOffset);
	printf("EastLongitude: %g\n", EastLongitude);

	if (strcmp(type, "moon") == 0) {
		double ffullmoon[MAXMOONS], fnewmoon[MAXMOONS];
		int i;

		for (year = year1; year <= year2; year++) {
			fpom(year, UTCOffset, ffullmoon, fnewmoon);
			printf("Full moon %d:\t", year);
			for (i = 0; ffullmoon[i] >= 0; i++) {
				printf("%g (%s) ", ffullmoon[i],
				    floattoday(year, ffullmoon[i]));
			}
			printf("\nNew moon %d:\t", year);
			for (i = 0; fnewmoon[i] >= 0; i++) {
				printf("%g (%s) ", fnewmoon[i],
				    floattoday(year, fnewmoon[i]));
			}
			printf("\n");
		}
		return;
	}

	if (strcmp(type, "sun") == 0) {
		double equinoxdays[2], solsticedays[2];
		for (year = year1; year <= year2; year++) {
			printf("Sun in %d:\n", year);
			fequinoxsolstice(year, UTCOffset, equinoxdays,
			    solsticedays);
			printf("e[0] - %g (%s)\n",
			    equinoxdays[0],
			    floattoday(year, equinoxdays[0]));
			printf("e[1] - %g (%s)\n",
			    equinoxdays[1],
			    floattoday(year, equinoxdays[1]));
			printf("s[0] - %g (%s)\n",
			    solsticedays[0],
			    floattoday(year, solsticedays[0]));
			printf("s[1] - %g (%s)\n",
			    solsticedays[1],
			    floattoday(year, solsticedays[1]));
		}
		return;
	}
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
parse_timezone(const char *s, long *result)
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

	long offset = hh * 3600L + mm * 60L;
	*result = (sign == '+') ? offset : -offset;

	return true;
}

/*
 * Parse a angle/coordinate string in format of a signle float number or
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
