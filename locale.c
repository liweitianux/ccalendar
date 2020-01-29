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
 * $FreeBSD: head/usr.bin/calendar/locale.c 326025 2017-11-20 19:49:47Z pfg $
 */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "calendar.h"
#include "utils.h"

const char *fdays[NDAYS+1] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
	"Saturday", NULL,
};

const char *days[NDAYS+1] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL,
};

const char *fmonths[NMONTHS+1] = {
	"January", "February", "March", "April", "May", "June", "Juli",
	"August", "September", "October", "November", "December", NULL,
};

const char *months[NMONTHS+1] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL,
};

const char *sequences[NSEQUENCES+1] = {
	"First", "Second", "Third", "Fourth", "Fifth", "Last", NULL,
};

struct fixs fndays[NDAYS+1];		/* full national days names */
struct fixs ndays[NDAYS+1];		/* short national days names */
struct fixs fnmonths[NMONTHS+1];	/* full national months names */
struct fixs nmonths[NMONTHS+1];		/* short national month names */
struct fixs nsequences[NSEQUENCES+1];	/* national sequence names */


void
setnnames(void)
{
	char buf[80];
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	for (int i = 0; i < NDAYS; i++) {
		tm.tm_wday = i;
		strftime(buf, sizeof(buf), "%a", &tm);
		if (ndays[i].name != NULL)
			free(ndays[i].name);
		ndays[i].name = xstrdup(buf);
		ndays[i].len = strlen(buf);

		strftime(buf, sizeof(buf), "%A", &tm);
		if (fndays[i].name != NULL)
			free(fndays[i].name);
		fndays[i].name = xstrdup(buf);
		fndays[i].len = strlen(buf);
	}
	ndays[NDAYS].name = NULL;
	ndays[NDAYS].len = 0;
	fndays[NDAYS].name = NULL;
	fndays[NDAYS].len = 0;

	memset(&tm, 0, sizeof(tm));
	for (int i = 0; i < NMONTHS; i++) {
		tm.tm_mon = i;
		strftime(buf, sizeof(buf), "%b", &tm);
		if (nmonths[i].name != NULL)
			free(nmonths[i].name);
		nmonths[i].name = xstrdup(buf);
		nmonths[i].len = strlen(buf);

		strftime(buf, sizeof(buf), "%B", &tm);
		if (fnmonths[i].name != NULL)
			free(fnmonths[i].name);
		fnmonths[i].name = xstrdup(buf);
		fnmonths[i].len = strlen(buf);
	}
	nmonths[NMONTHS].name = NULL;
	nmonths[NMONTHS].len = 0;
	fnmonths[NMONTHS].name = NULL;
	fnmonths[NMONTHS].len = 0;
}

void
setnsequences(char *seq)
{
	int i;
	char *p;

	p = seq;
	for (i = 0; i < NSEQUENCES-1; i++) {
		nsequences[i].name = p;
		if ((p = strchr(p, ' ')) == NULL) {
			/* Oh oh there is something wrong. Erase! Erase! */
			for (i = 0; i < 5; i++) {
				nsequences[i].name = NULL;
				nsequences[i].len = 0;
			}
			return;
		}
		*p = '\0';
		p++;
	}
	nsequences[NSEQUENCES-1].name = p;

	for (i = 0; i < NSEQUENCES-1; i++) {
		nsequences[i].name = xstrdup(nsequences[i].name);
		nsequences[i].len = nsequences[i + 1].name - nsequences[i].name;
	}
	nsequences[NSEQUENCES-1].name = xstrdup(nsequences[NSEQUENCES-1].name);
	nsequences[NSEQUENCES-1].len = strlen(nsequences[NSEQUENCES-1].name);

	nsequences[NSEQUENCES].name = NULL;
	nsequences[NSEQUENCES].len = 0;
}
