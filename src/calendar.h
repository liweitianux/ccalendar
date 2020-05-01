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
 * $FreeBSD: head/usr.bin/calendar/calendar.h 326025 2017-11-20 19:49:47Z pfg $
 */

#ifndef CALENDAR_H_
#define CALENDAR_H_

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#define	SECSPERDAY	(24 * 60 * 60)
#define	SECSPERHOUR	(60 * 60)
#define	SECSPERMINUTE	(60)
#define	MINSPERHOUR	(60)
#define	HOURSPERDAY	(24)
#define	FSECSPERDAY	(24.0 * 60.0 * 60.0)
#define	FSECSPERHOUR	(60.0 * 60.0)
#define	FSECSPERMINUTE	(60.0)
#define	FMINSPERHOUR	(60.0)
#define	FHOURSPERDAY	(24.0)

#define	DAYSPERYEAR	365
#define	DAYSPERLEAPYEAR	366

#define	NDAYS		7
#define	NMONTHS		12
#define	NSEQUENCES	6

/*
 * Random number of maximum number of repeats of an event.  Should be 52
 * (number of weeks per year).  If you want to show two years then it should
 * be 104.  If you are seeing more than this you are using this program wrong.
 */
#define	MAXCOUNT	125


struct cal_options {
	const char *calendarFile;  /* name of calendar file */
	struct tm now;  /* time/date of calendar events to remind */
	int year1;  /* year of the beginning day */
	int year2;  /* year of the ending day */
	double UTCOffset;
	double EastLongitude;
	bool debug;
};

extern struct cal_options Options;


static inline bool
isleap(int y)
{
	return (((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0;
}

static inline void
logdebug(const char *format, ...)
{
	if (!Options.debug)
		return;

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}


/* events.c */
/*
 * Event sorting related functions:
 * - Use event_add() to create a new event
 * - Use event_continue() to add more text to the last added event
 * - Use event_print_all() to display them in time chronological order
 */
struct event *event_add(int year, int month, int day, char *date,
			bool variable, char *txt, char *extra);
void	event_continue(struct event *events, char *txt);
void	event_print_all(FILE *fp);
struct event {
	int	year;
	int	month;
	int	day;
	bool	variable;  /* Whether a variable event ? */
	char	*date;  /* human readable */
	char	*text;
	char	*extra;
	struct event *next;
};

/* locale.c */
extern const char *fdays[];		/* full day names */
extern const char *days[];		/* short day names */
extern const char *fmonths[];		/* full month names */
extern const char *months[];		/* short month names */
extern const char *sequences[];		/* sequence names */
extern char *fndays[];			/* full national day names */
extern char *ndays[];			/* short national day names */
extern char *fnmonths[];		/* full national month names */
extern char *nmonths[];			/* short national month names */
extern char *nsequences[];		/* national sequence names */

void	setnnames(void);
void	setnsequences(const char *seq);

/* io.c */
extern char *neaster, *npaskha, *ncny, *nfullmoon, *nnewmoon;
extern char *nmarequinox, *nsepequinox, *njunsolstice, *ndecsolstice;

int	cal(bool doall);

/* ostern.c / paskha.c */
int	paskha(int year);
int	easter(int year);

/* dates.c */
struct cal_year {
	int year;	/* 19xx, 20xx, 21xx */
	int easter;	/* Julian day */
	int paskha;	/* Julian day */
	int cny;	/* Julian day */
	int firstdayofweek; /* day of week on Jan 1; values: 0 .. 6 */
	struct cal_month *months;
	struct cal_year *nextyear;
};

struct cal_month {
	int month;			/* 01 .. 12 */
	int firstdayjulian;		/* 000 .. 366 */
	int firstdayofweek;		/* 0 .. 6 */
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

extern int cumdaytab[][14];
extern int monthdaytab[][14];

void	generatedates(struct tm *tp1, struct tm *tp2);
void	dumpdates(void);
int	first_dayofweek_of_year(int y);
int	first_dayofweek_of_month(int y, int m);
bool	walkthrough_dates(struct event **e);
void	addtodate(struct event *e, int year, int month, int day);
struct cal_day *find_yd(int yy, int dd);
struct cal_day *find_ymd(int yy, int mm, int dd);

/* pom.c */
#define	MAXMOONS	18
void	pom(int year, double UTCoffset, int *fms, int *nms);
void	fpom(int year, double UTCoffset, double *ffms, double *fnms);

/* sunpos.c */
void	equinoxsolstice(int year, double UTCoffset, int *equinoxdays,
			int *solsticedays);
void	fequinoxsolstice(int year, double UTCoffset, double *equinoxdays,
			 double *solsticedays);
int	calculatesunlongitude30(int year, int degreeGMToffset,
				int *ichinesemonths);

#endif
