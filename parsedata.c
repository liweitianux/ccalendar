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

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "calendar.h"
#include "parsedata.h"
#include "utils.h"

static bool	 checkdayofweek(const char *s, size_t *len, int *offset,
				const char **dow);
static bool	 checkmonth(const char *s, size_t *len, int *offset,
			    const char **month);
static void	 debug_determinestyle(int dateonly, const char *date,
				      int flags, const char *month, int imonth,
				      const char *dayofmonth, int idayofmonth,
				      const char *dayofweek, int idayofweek,
				      const char *modifieroffset,
				      const char *modifierindex,
				      const char *specialday,
				      const char *year, int iyear);
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
static int	 indextooffset(const char *s);
static bool	 isonlydigits(const char *s, bool nostar);
static bool	 parse_angle(const char *s, double *result);
static const char *parse_int_ranged(const char *s, size_t len, int min,
				    int max, int *result);
static void	 remember(int *index, int *y, int *m, int *d, char **ed,
			  int yy, int mm, int dd, const char *extra);
static char	*showflags(int flags);
static int	 wdayom(int day, int offset, int month, int year);

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
			warnx("Invalid date: %s", date);
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

out:
	free(date2);
	return ret;
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

static void
debug_determinestyle(int dateonly, const char *date, int flags,
		     const char *month, int imonth,
		     const char *dayofmonth, int idayofmonth,
		     const char *dayofweek, int idayofweek,
		     const char *modifieroffset, const char *modifierindex,
		     const char *specialday, const char *year, int iyear)
{
	if (dateonly != 0) {
		printf("-------\ndate: |%s|\n", date);
		if (dateonly == 1)
			return;
	}
	printf("flags: 0x%x - %s\n", flags, showflags(flags));
	if (modifieroffset[0] != '\0')
		printf("modifieroffset: |%s|\n", modifieroffset);
	if (modifierindex[0] != '\0')
		printf("modifierindex: |%s|\n", modifierindex);
	if (year[0] != '\0')
		printf("year: |%s| (%d)\n", year, iyear);
	if (month[0] != '\0')
		printf("month: |%s| (%d)\n", month, imonth);
	if (dayofmonth[0] != '\0')
		printf("dayofmonth: |%s| (%d)\n", dayofmonth, idayofmonth);
	if (dayofweek[0] != '\0')
		printf("dayofweek: |%s| (%d)\n", dayofweek, idayofweek);
	if (specialday[0] != '\0')
		printf("specialday: |%s|\n", specialday);
}

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
static struct yearinfo *years, *yearinfo;

/*
 * Calculate dates with offset from weekdays, like Thu-3, Wed+2, etc.
 * day is the day of the week,
 * offset the ordinal number of the weekday in the month.
 */
static int
wdayom(int day, int offset, int month, int year)
{
	int wday1;  /* Weekday of first day in month */
	int wdayn;  /* Weekday of first day in month */
	int d;

	wday1 = first_dayofweek_of_month(year, month);
	if (wday1 < 0)                          /* not set */
		return (wday1);

	/*
	 * Date of zeroth or first of our weekday in month, depending on the
	 * relationship with the first of the month.  The range is -6:6.
	 */
	d = (day - wday1 + 1) % 7;
	/*
	 * Which way are we counting?  Offset 0 is invalid, abs (offset) > 5 is
	 * meaningless, but that's OK.  Offset 5 may or may not be meaningless,
	 * so there's no point in complaining for complaining's sake.
	 */
	if (offset < 0) {			/* back from end of month */
						/* FIXME */
		wdayn = d;
		while (wdayn <= yearinfo->monthdays[month])
			wdayn += 7;
		d = offset * 7 + wdayn;
	} else if (offset > 0) {
		if (d > 0)
			d += offset * 7 - 7;
		else
			d += offset * 7;
	} else {
		warnx("Invalid offset 0");
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
	char month[100], dayofmonth[100], dayofweek[100], modifieroffset[100];
	char syear[100];
	char modifierindex[100], specialday[100];
	int idayofweek = -1, imonth = -1, idayofmonth = -1, iyear = -1;
	int year, remindex;
	int d, m, dow, rm, rd, offset;
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

	if (debug)
		debug_determinestyle(1, date, *flags, month, imonth,
		    dayofmonth, idayofmonth, dayofweek, idayofweek,
		    modifieroffset, modifierindex, specialday, syear, iyear);
	if (determinestyle(date, flags, month, &imonth, dayofmonth,
			   &idayofmonth, dayofweek, &idayofweek, modifieroffset,
			   modifierindex, specialday, syear, &iyear) == false) {
		if (debug)
			printf("Failed!\n");
		return (0);
	}

	if (debug)
		debug_determinestyle(0, date, *flags, month, imonth,
		    dayofmonth, idayofmonth, dayofweek, idayofweek,
		    modifieroffset, modifierindex, specialday, syear, iyear);

	remindex = 0;
	for (year = year1; year <= year2; year++) {
		int lflags = *flags;
		/* If the year is specified, only do it if it is this year! */
		if ((lflags & F_YEAR) != 0)
			if (iyear != year)
				continue;
		lflags &= ~F_YEAR;

		/* Get important dates for this year */
		yearinfo = years;
		while (yearinfo != NULL) {
			if (yearinfo->year == year)
				break;
			yearinfo = yearinfo -> next;
		}
		if (yearinfo == NULL) {
			yearinfo = xcalloc(1, sizeof(*yearinfo));
			yearinfo->year = year;
			yearinfo->next = years;
			years = yearinfo;

			yearinfo->monthdays = monthdaytab[isleap(year)];
			yearinfo->ieaster = easter(year);
			yearinfo->ipaskha = paskha(year);
			fpom(year, UTCOffset, yearinfo->ffullmoon,
			    yearinfo->fnewmoon);
			fpom(year, UTCOFFSET_CNY, yearinfo->ffullmooncny,
			    yearinfo->fnewmooncny);
			fequinoxsolstice(year, UTCOffset,
			    yearinfo->equinoxdays, yearinfo->solsticedays);

			/*
			 * CNY: Match day with sun longitude at 330` with new
			 * moon
			 */
			yearinfo->firstcnyday = calculatesunlongitude30(year,
			    UTCOFFSET_CNY, yearinfo->ichinesemonths);
			for (m = 0; yearinfo->fnewmooncny[m] >= 0; m++) {
				if (yearinfo->fnewmooncny[m] >
				    yearinfo->firstcnyday) {
					yearinfo->firstcnyday = (int)
					    floor(yearinfo->fnewmooncny[m - 1]);
					break;
				}
			}
		}

		/* Same day every year */
		if (lflags == (F_MONTH | F_DAYOFMONTH)) {
			if (find_ymd(year, imonth, idayofmonth) == NULL)
				continue;
			remember(&remindex, yearp, monthp, dayp, edp,
			    year, imonth, idayofmonth, NULL);
			continue;
		}

		/* XXX Same day every year, but variable */
		if (lflags == (F_MONTH | F_DAYOFMONTH | F_VARIABLE)) {
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
			for (d = 1; d <= yearinfo->monthdays[imonth]; d++) {
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
			d = (idayofweek - dow + 8) % 7;
			while (d <= 366) {
				if (remember_yd(year, d, &rm, &rd))
					remember(&remindex,
					    yearp, monthp, dayp, edp,
					    year, rm, rd, NULL);
				d += 7;
			}
			continue;
		}

		/*
		 * Every so-manied dayofweek of every month of the year:
		 * Thu-3
		 */
		if (lflags == (F_DAYOFWEEK | F_MODIFIERINDEX | F_VARIABLE)) {
			offset = indextooffset(modifierindex);

			for (m = 0; m <= 12; m++) {
	                        d = wdayom (idayofweek, offset, m, year);
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
		 * A certain dayofweek of a month
		 * Jan/Thu-3
		 */
		if (lflags ==
		    (F_MONTH | F_DAYOFWEEK | F_MODIFIERINDEX | F_VARIABLE)) {
			offset = indextooffset(modifierindex);
			dow = first_dayofweek_of_month(year, imonth);
			d = (idayofweek - dow + 8) % 7;

			if (offset > 0) {
				while (d <= yearinfo->monthdays[imonth]) {
					if (--offset == 0
					    && find_ymd(year, imonth, d)) {
						remember(&remindex,
						    yearp, monthp, dayp, edp,
						    year, imonth, d, NULL);
						continue;
					}
					d += 7;
				}
				continue;
			}
			if (offset < 0) {
				while (d <= yearinfo->monthdays[imonth])
					d += 7;
				while (offset != 0) {
					offset++;
					d -= 7;
				}
				if (find_ymd(year, imonth, d)) {
					remember(&remindex,
					    yearp, monthp, dayp, edp,
					    year, imonth, d, NULL);
				}
				continue;
			}
			continue;
		}

		/* Every dayofweek of the month */
		if (lflags == (F_DAYOFWEEK | F_MONTH | F_VARIABLE)) {
			dow = first_dayofweek_of_month(year, imonth);
			d = (idayofweek - dow + 8) % 7;
			while (d <= yearinfo->monthdays[imonth]) {
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
			if (remember_yd(year, yearinfo->ieaster + offset,
					&rm, &rd)) {
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
			if (remember_yd(year, yearinfo->ipaskha + offset,
					&rm, &rd)) {
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
			if (remember_yd(year, yearinfo->firstcnyday + offset,
					&rm, &rd)) {
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, rm, rd, NULL);
			}
			continue;
		}

		/* FullMoon */
		if ((lflags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_FULLMOON)) {
			int i;

			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			for (i = 0; yearinfo->ffullmoon[i] > 0; i++) {
				if (remember_yd(year,
				    (int)floor(yearinfo->ffullmoon[i]) + offset,
				    &rm, &rd)) {
					ed = floattotime(
					    yearinfo->ffullmoon[i]);
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
			int i;

			offset = 0;
			if ((lflags & F_MODIFIEROFFSET) != 0)
				offset = (int)strtol(modifieroffset, NULL, 10);
			for (i = 0; yearinfo->ffullmoon[i] > 0; i++) {
				if (remember_yd(year,
				    (int)floor(yearinfo->fnewmoon[i]) + offset,
				    &rm, &rd)) {
					ed = floattotime(yearinfo->fnewmoon[i]);
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
			if (remember_yd(year,
					(int)yearinfo->equinoxdays[0] + offset,
					&rm, &rd)) {
				ed = floattotime(yearinfo->equinoxdays[0]);
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
			if (remember_yd(year,
					(int)yearinfo->equinoxdays[1] + offset,
					&rm, &rd)) {
				ed = floattotime(yearinfo->equinoxdays[1]);
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
			if (remember_yd(year,
					(int)yearinfo->solsticedays[0] + offset,
					&rm, &rd)) {
				ed = floattotime(yearinfo->solsticedays[0]);
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
			if (remember_yd(year,
					(int)yearinfo->solsticedays[1] + offset,
					&rm, &rd)) {
				ed = floattotime(yearinfo->solsticedays[1]);
				remember(&remindex, yearp, monthp, dayp, edp,
				    year, rm, rd, ed);
			}
			continue;
		}

		if (debug) {
			printf("Unprocessed:\n");
			debug_determinestyle(2, date, lflags, month, imonth,
			    dayofmonth, idayofmonth, dayofweek, idayofweek,
			    modifieroffset, modifierindex, specialday, syear,
			    iyear);
		}
		retvalsign = -1;
	}

	if (retvalsign == -1)
		return (-remindex - 1);
	else
		return (remindex);
}

static char *
showflags(int flags)
{
	static char s[1000];
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

static const char *
getmonthname(int i)
{
	if (i <= 0 || i > NMONTHS)
		errx(1, "Invalid month index: %d", i);

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
		errx(1, "Invalid day-of-week index: %d", i);

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
indextooffset(const char *s)
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
