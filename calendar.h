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

#include <sys/types.h>

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

#ifndef nitems
#define nitems(x)	(sizeof(x) / sizeof((x)[0]))
#endif

/* Not yet categorized */

extern bool debug;
extern int year1, year2;
extern time_t t1, t2;
extern const char *calendarFile;
extern struct fixs neaster, npaskha, ncny, nfullmoon, nnewmoon;
extern struct fixs nmarequinox, nsepequinox, njunsolstice, ndecsolstice;
extern struct tm tm_now;  /* time/date of calendar events to remind */
extern double UTCOffset;
extern double EastLongitude;

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

/*
 * Random number of maximum number of repeats of an event. Should be 52 (number
 * of weeks per year).  If you want to show two years then it should be 104.
 * If you are seeing more than this you are using this program wrong.
 */
#define	MAXCOUNT	125

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

struct fixs {
	char	*name;
	size_t	len;
};

extern const char *days[];
extern const char *fdays[];
extern const char *fmonths[];
extern const char *months[];
extern const char *sequences[];
extern struct fixs fndays[8];		/* full national days names */
extern struct fixs fnmonths[13];	/* full national months names */
extern struct fixs ndays[8];		/* short national days names */
extern struct fixs nmonths[13];		/* short national month names */
extern struct fixs nsequences[10];

void	setnnames(void);
void	setnsequences(char *);

/* day.c */
void	settimes(time_t now, int before, int after, int friday,
		 struct tm *tp1, struct tm *tp2);
time_t	Mktime(const char *s);

/* io.c */
void	cal(bool doall);

/* ostern.c / paskha.c */
int	paskha(int);
int	easter(int);

/* dates.c */
extern int cumdaytab[][14];
extern int monthdaytab[][14];
void	generatedates(struct tm *tp1, struct tm *tp2);
void	dumpdates(void);
bool	remember_ymd(int y, int m, int d);
bool	remember_yd(int y, int d, int *rm, int *rd);
int	first_dayofweek_of_year(int y);
int	first_dayofweek_of_month(int y, int m);
int	walkthrough_dates(struct event **e);
void	addtodate(struct event *e, int year, int month, int day);

/* pom.c */
#define	MAXMOONS	18
void	pom(int year, double UTCoffset, int *fms, int *nms);
void	fpom(int year, double UTCoffset, double *ffms, double *fnms);

/* sunpos.c */
void	equinoxsolstice(int year, double UTCoffset, int *equinoxdays, int *solsticedays);
void	fequinoxsolstice(int year, double UTCoffset, double *equinoxdays, double *solsticedays);
int	calculatesunlongitude30(int year, int degreeGMToffset, int *ichinesemonths);
