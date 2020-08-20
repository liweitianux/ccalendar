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
 * $FreeBSD: head/usr.bin/calendar/parsedata.c 326276 2017-11-27 15:37:16Z pfg $
 */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "calendar.h"
#include "basics.h"
#include "days.h"
#include "gregorian.h"
#include "io.h"
#include "nnames.h"
#include "parsedata.h"
#include "utils.h"

struct dateinfo {
	int	flags;
	int	sday_id;
	int	year;
	int	month;
	int	dayofmonth;
	int	dayofweek;
	int	offset;
	int	index;
};

static bool	 checkdayofweek(const char *s, size_t *len, int *offset,
				const char **dow);
static bool	 checkmonth(const char *s, size_t *len, int *offset,
			    const char **month);
static bool	 determinestyle(const char *date, struct dateinfo *di);
static bool	 isonlydigits(const char *s, bool nostar);
static bool	 parse_angle(const char *s, double *result);
static const char *parse_int_ranged(const char *s, size_t len, int min,
				    int max, int *result);
static bool	 parse_index(const char *s, int *index);
static void	 show_dateinfo(struct dateinfo *di);

/*
 * Expected styles:
 *
 * Date			::=	Year . '/' . Month . '/' . DayOfMonth |
 *				Year . ' ' . Month . ' ' . DayOfMonth |
 *				Month . '/' . DayOfMonth |
 *				Month . ' ' . DayOfMonth |
 *				Month . '/' . DayOfWeek . Index |
 *				Month . ' ' . DayOfWeek . Index |
 *				MonthName . '/' . AllDays |
 *				MonthName . ' ' . AllDays |
 *				AllDays . '/' . MonthName |
 *				AllDays . ' ' . MonthName |
 *				AllMonths . '/' . DayOfMonth |
 *				AllMonths . ' ' . DayOfMonth |
 *				DayOfMonth . '/' . AllMonths |
 *				DayOfMonth . ' ' . AllMonths |
 *				DayOfMonth . '/' . Month |
 *				DayOfMonth . ' ' . Month |
 *				DayOfWeek . Index . '/' . MonthName |
 *				DayOfWeek . Index . ' ' . MonthName |
 *				DayOfWeek . Index
 *				SpecialDay . Offset
 *
 * Year			::=	'0' ... '9' | '00' ... '09' | '10' ... '99' |
 *				'100' ... '999' | '1000' ... '9999'
 *
 * Month		::=	MonthName | MonthNumber
 * MonthNumber		::=	'0' ... '9' | '00' ... '09' | '10' ... '12'
 * MonthName		::=	MonthNameShort | MonthNameLong
 * MonthNameLong	::=	'January' ... 'December'
 * MonthNameShort	::=	'Jan' ... 'Dec' | 'Jan.' ... 'Dec.'
 *
 * DayOfWeek		::=	DayOfWeekShort | DayOfWeekLong
 * DayOfWeekShort	::=	'Mon' ... 'Sun'
 * DayOfWeekLong	::=	'Monday' ... 'Sunday'
 * DayOfMonth		::=	'0' ... '9' | '00' ... '09' | '10' ... '29' |
 *				'30' ... '31'
 *
 * AllMonths		::=	'*'
 * AllDays		::=	'*'
 *
 * Index		::=	'' | IndexName |
 *				'+' . IndexNumber | '-' . IndexNumber
 * IndexName		::=	'First' | 'Second' | 'Third' | 'Fourth' |
 *				'Fifth' | 'Last'
 * IndexNumber		::=	'1' ... '5'
 *
 * Offset		::=	'' | '+' . OffsetNumber | '-' . OffsetNumber
 * OffsetNumber		::=	'0' ... '9' | '00' ... '99' | '000' ... '299' |
 *				'300' ... '359' | '360' ... '365'
 *
 * SpecialDay		::=	'Easter' | 'Paskha' | 'ChineseNewYear' |
 *				'NewMoon' | 'FullMoon' |
 *				'MarEquinox' | 'SepEquinox' |
 *				'JunSolstice' | 'DecSolstice'
 */
static bool
determinestyle(const char *date, struct dateinfo *di)
{
	struct specialday *sday;
	const char *dow, *pmonth;
	char *date2;
	char *p, *p1, *p2;
	size_t len;
	bool ret;
	int offset;

	DPRINTF("-------\ndate: |%s|\n", date);
	date2 = xstrdup(date);
	ret = false;

	if ((p = strchr(date2, ' ')) == NULL &&
	    (p = strchr(date2, '/')) == NULL) {
		for (size_t i = 0; specialdays[i].id != SD_NONE; i++) {
			sday = &specialdays[i];
			if (strncasecmp(date2, sday->name, sday->len) == 0) {
				len = sday->len;
			} else if (sday->n_len > 0 && strncasecmp(
					date2, sday->n_name, sday->n_len) == 0) {
				len = sday->n_len;
			} else {
				continue;
			}

			di->flags |= (F_SPECIALDAY | F_VARIABLE);
			di->sday_id = sday->id;
			if (strlen(date2) == len) {
				ret = true;
				goto out;
			}
			di->offset = (int)strtol(date2+len, NULL, 10);
			di->flags |= F_OFFSET;
			ret = true;
			goto out;
		}

		if (checkdayofweek(date2, &len, &offset, &dow)) {
			di->flags |= (F_DAYOFWEEK | F_VARIABLE);
			di->dayofweek = offset;
			if (strlen(date2) == len) {
				ret = true;
				goto out;
			}
			if (parse_index(date2+len, &di->index)) {
				di->flags |= F_INDEX;
				ret = true;
			}
			goto out;
		}
		if (isonlydigits(date2, true)) {
			/* Assume month number only */
			di->flags |= F_MONTH;
			di->month = (int)strtol(date2, NULL, 10);
			ret = true;
			goto out;
		}

		goto out;
	}

	*p = '\0';
	p1 = date2;
	p2 = p + 1;
	/* Now p1 and p2 point to the first and second fields, respectively */

	if ((p = strchr(p2, ' ')) != NULL ||
	    (p = strchr(p2, '/')) != NULL) {
		/* Got a year in the date string. */
		di->flags |= F_YEAR;
		di->year = (int)strtol(p1, NULL, 10);
		*p = '\0';
		p1 = p2;
		p2 = p + 1;
	}

	/* Check if there is a month string */
	if (checkmonth(p1, &len, &offset, &pmonth) ||
	    (checkmonth(p2, &len, &offset, &pmonth) && (p2 = p1))) {
		/* Now p2 is the non-month part */

		di->flags |= F_MONTH;
		di->month = offset;

		if (isonlydigits(p2, true)) {
			di->dayofmonth = (int)strtol(p2, NULL, 10);
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
			di->dayofweek = offset;
			if (strlen(p2) == len) {
				ret = true;
				goto out;
			}
			if (parse_index(p2+len, &di->index)) {
				di->flags |= F_INDEX;
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
		di->dayofmonth = (int)strtol(p2, NULL, 10);
		ret = true;
		goto out;
	}

	/* Month as a number, then a weekday */
	if (isonlydigits(p1, true) &&
	    checkdayofweek(p2, &len, &offset, &dow)) {
		di->flags |= (F_MONTH | F_DAYOFWEEK | F_VARIABLE);
		di->month = (int)strtol(p1, NULL, 10);
		di->dayofweek = offset;
		if (strlen(p2) == len) {
			ret = true;
			goto out;
		}
		if (parse_index(p2+len, &di->index)) {
			di->flags |= F_INDEX;
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
		di->month = m;
		di->dayofmonth = d;
		ret = true;
		goto out;
	}

	warnx("%s: unrecognized date: |%s|", __func__, date);

out:
	if (Options.debug)
		show_dateinfo(di);
	free(date2);
	return ret;
}

static void
show_dateinfo(struct dateinfo *di)
{
	struct specialday *sday;

	fprintf(stderr, "flags: 0x%x -", di->flags);

	if ((di->flags & F_YEAR) != 0)
		fprintf(stderr, " year(%d)", di->year);
	if ((di->flags & F_MONTH) != 0)
		fprintf(stderr, " month(%d)", di->month);
	if ((di->flags & F_DAYOFWEEK) != 0)
		fprintf(stderr, " dayofweek(%d)", di->dayofweek);
	if ((di->flags & F_DAYOFMONTH) != 0)
		fprintf(stderr, " dayofmonth(%d)", di->dayofmonth);
	if ((di->flags & F_INDEX) != 0)
		fprintf(stderr, " index(%d)", di->index);
	if ((di->flags & F_OFFSET) != 0)
		fprintf(stderr, " offset(%d)", di->offset);

	if ((di->flags & F_SPECIALDAY) != 0) {
		fprintf(stderr, " specialday");
		for (size_t i = 0; specialdays[i].id != SD_NONE; i++) {
			sday = &specialdays[i];
			if (di->sday_id == sday->id)
				fprintf(stderr, "(%s)", sday->name);
		}
	}

	if ((di->flags & F_ALLMONTH) != 0)
		fprintf(stderr, " allmonth");
	if ((di->flags & F_ALLDAY) != 0)
		fprintf(stderr, " allday");
	if ((di->flags & F_VARIABLE) != 0)
		fprintf(stderr, " variable");
	fprintf(stderr, "\n");
	fflush(stderr);
}

int
parse_cal_date(const char *date, int *flags, struct cal_day **dayp, char **edp)
{
	struct specialday *sday;
	struct dateinfo di;
	int index, offset;

	memset(&di, 0, sizeof(di));
	di.flags = F_NONE;

	if (!determinestyle(date, &di))
		return -1;

	*flags = di.flags;
	index = (di.flags & F_INDEX) ? di.index : 0;
	offset = (di.flags & F_OFFSET) ? di.offset : 0;

	/* Specified year, month and day (e.g., '2020/Aug/16') */
	if ((di.flags & ~F_VARIABLE) == (F_YEAR | F_MONTH | F_DAYOFMONTH))
		return find_days_ymd(di.year, di.month, di.dayofmonth,
				     dayp, edp);

	/* Specified month and day (e.g., 'Aug/16') */
	if ((di.flags & ~F_VARIABLE) == (F_MONTH | F_DAYOFMONTH))
		return find_days_ymd(-1, di.month, di.dayofmonth,
				     dayp, edp);

	/* Same day every month (e.g., '* 16') */
	if (di.flags == (F_ALLMONTH | F_DAYOFMONTH))
		return find_days_dom(di.dayofmonth, dayp, edp);

	/* Every day of a month (e.g., 'Aug *') */
	if (di.flags == (F_ALLDAY | F_MONTH))
		return find_days_month(di.month, dayp, edp);

	/*
	 * Every day-of-week of a month (e.g., 'Aug/Sun')
	 * One indexed day-of-week of a month (e.g., 'Aug/Sun+3')
	 */
	if ((di.flags & ~F_INDEX) == (F_MONTH | F_DAYOFWEEK | F_VARIABLE))
		return find_days_mdow(di.month, di.dayofweek, index,
				      dayp, edp);

	/*
	 * Every day-of-week of the year (e.g., 'Sun')
	 * One indexed day-of-week of every month (e.g., 'Sun+3')
	 */
	if ((di.flags & ~F_INDEX) == (F_DAYOFWEEK | F_VARIABLE))
		return find_days_mdow(-1, di.dayofweek, index, dayp, edp);

	/* Special days with optional offset (e.g., 'ChineseNewYear+14') */
	if ((di.flags & F_SPECIALDAY) != 0) {
		for (size_t i = 0; specialdays[i].id != SD_NONE; i++) {
			sday = &specialdays[i];
			if (di.sday_id == sday->id && sday->find_days != NULL)
				return (sday->find_days)(offset, dayp, edp);
		}
	}

	warnx("%s: unprocessed date: |%s|", __func__, date);
	if (Options.debug)
		show_dateinfo(&di);

	return 0;
}

static bool
checkmonth(const char *s, size_t *len, int *offset, const char **month)
{
	struct nname *nname;

	for (int i = 0; month_names[i].name != NULL; i++) {
		nname = &month_names[i];

		if (nname->fn_name &&
		    strncasecmp(s, nname->fn_name, nname->fn_len) == 0) {
			*month = nname->fn_name;
			*len = nname->fn_len;
			*offset = i + 1;
			return (true);
		}

		if (nname->n_name &&
		    strncasecmp(s, nname->n_name, nname->n_len) == 0) {
			*month = nname->n_name;
			*len = nname->n_len;
			*offset = i + 1;
			return (true);
		}

		if (nname->f_name &&
		    strncasecmp(s, nname->f_name, nname->f_len) == 0) {
			*month = nname->f_name;
			*len = nname->f_len;
			*offset = i + 1;
			return (true);
		}

		if (strncasecmp(s, nname->name, nname->len) == 0) {
			*month = nname->name;
			*len = nname->len;
			*offset = i + 1;
			return (true);
		}
	}

	return (false);
}

static bool
checkdayofweek(const char *s, size_t *len, int *offset, const char **dow)
{
	struct nname *nname;

	for (int i = 0; dow_names[i].name != NULL; i++) {
		nname = &dow_names[i];

		if (nname->fn_name &&
		    strncasecmp(s, nname->fn_name, nname->fn_len) == 0) {
			*dow = nname->fn_name;
			*len = nname->fn_len;
			*offset = i;
			return (true);
		}

		if (nname->n_name &&
		    strncasecmp(s, nname->n_name, nname->n_len) == 0) {
			*dow = nname->n_name;
			*len = nname->n_len;
			*offset = i;
			return (true);
		}

		if (nname->f_name &&
		    strncasecmp(s, nname->f_name, nname->f_len) == 0) {
			*dow = nname->f_name;
			*len = nname->f_len;
			*offset = i;
			return (true);
		}

		if (strncasecmp(s, nname->name, nname->len) == 0) {
			*dow = nname->name;
			*len = nname->len;
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
		int v = (int)strtol(s, &endp, 10);
		if (*endp != '\0')
			return false;  /* has trailing junk */
		if (v == 0 || v <= -6 || v >= 6) {
			warnx("%s(): invalid value: %d", __func__, v);
			return false;
		}

		*index = v;
		parsed = true;
	}

	for (int i = 0; !parsed && sequence_names[i].name; i++) {
		if (strcasecmp(s, sequence_names[i].name) == 0 ||
		    (sequence_names[i].n_name &&
		     strcasecmp(s, sequence_names[i].n_name) == 0)) {
			parsed = true;
			*index = (i == 5) ? -1 : (i+1);  /* 'Last' -> '-1' */
		}
	}

	DPRINTF("%s(): |%s| -> %d (%s)\n",
		__func__, s, *index, (parsed ? "ok" : "fail"));
	return parsed;
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

	int offset = hh * 3600 + mm * 60;
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
		/* try to parse format of 'd:m:s' */
		int deg = 0;
		int min = 0;
		double sec = 0.0;
		switch (sscanf(s, "%d:%d:%lf", &deg, &min, &sec)) {
		case 3:
		case 2:
		case 1:
			if (min < 0 || min >= 60 || sec < 0 || sec >= 60)
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
		warnx("%s: invalid latitude: |%s|", __func__, p);
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
		warnx("%s: invalid longitude: |%s|", __func__, p);
		return false;
	}

	p = strtok(NULL, sep);
	if (p != NULL) {
		char *endp;
		v = strtod(p, &endp);
		if (p == endp || *endp != '\0' || v < 0) {
			warnx("%s: invalid elevation: |%s|", __func__, p);
			return false;
		}
		*elevation = v;
	}

	if ((p = strtok(NULL, sep)) != NULL) {
		warnx("%s: unknown value: |%s|", __func__, p);
		return false;
	}

	return true;
}

/*
 * Parse date string of format '[[[CC]YY]MM]DD' into a fixed date.
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
	date_set(&gdate, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

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

	DPRINTF("%s: parsed |%s| -> %d-%02d-%02d\n",
		__func__, date, gdate.year, gdate.month, gdate.day);
	return true;
}
