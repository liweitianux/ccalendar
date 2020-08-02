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

#ifndef __unused
#define __unused	__attribute__((__unused__))
#endif
#ifndef __daed2
#define __dead2		__attribute__((__noreturn__))
#endif

#define	NDAYS		7
#define	NMONTHS		12
#define	NSEQUENCES	6

/* Maximum number of new/full moons in a year */
#define	MAXMOONS	18

/*
 * Random number of maximum number of repeats of an event.  Should be 52
 * (number of weeks per year).  If you want to show two years then it should
 * be 104.  If you are seeing more than this you are using this program wrong.
 */
#define	MAXCOUNT	125

static inline bool
isleap(int y)
{
	return (((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0;
}


struct location;

struct cal_options {
	struct location *location;
	double time;  /* [0, 1) time of now in unit of days */
	int today;  /* R.D. of today to remind events */
	int day_begin;  /* beginning of date range to remind events */
	int day_end;  /* end of date range to remind events */
	int year1;  /* year of the beginning day */
	int year2;  /* year of the ending day */
	bool allmode;  /* whether to process calendars for all users */
	bool debug;
};

extern struct cal_options Options;
extern const char *calendarDirs[];  /* paths to search for calendar files */

void	logdebug(const char *format, ...);


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

int	cal(FILE *fp);

/* dates.c */
struct cal_day;
struct event;

void	generate_dates(void);
int	first_dayofweek_of_year(int y);
int	first_dayofweek_of_month(int y, int m);
struct cal_day *find_yd(int yy, int dd);
struct cal_day *find_ymd(int yy, int mm, int dd);

struct event *event_add(struct cal_day *dp, bool day_first, bool variable,
			char *txt, char *extra);
void	event_continue(struct event *events, char *txt);
void	event_print_all(FILE *fp);

#endif
