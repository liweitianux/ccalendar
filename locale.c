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

char *fndays[NDAYS+1] = { NULL }; /* full national day names */
char *ndays[NDAYS+1] = { NULL }; /* short national day names */
char *fnmonths[NMONTHS+1] = { NULL }; /* full national month names */
char *nmonths[NMONTHS+1] = { NULL }; /* short national month names */
char *nsequences[NSEQUENCES+1] = { NULL }; /* national sequence names */


void
setnnames(void)
{
	char buf[64];
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	for (int i = 0; i < NDAYS; i++) {
		tm.tm_wday = i;
		strftime(buf, sizeof(buf), "%a", &tm);
		if (ndays[i] != NULL)
			free(ndays[i]);
		ndays[i] = xstrdup(buf);

		strftime(buf, sizeof(buf), "%A", &tm);
		if (fndays[i] != NULL)
			free(fndays[i]);
		fndays[i] = xstrdup(buf);
	}

	memset(&tm, 0, sizeof(tm));
	for (int i = 0; i < NMONTHS; i++) {
		tm.tm_mon = i;
		strftime(buf, sizeof(buf), "%b", &tm);
		if (nmonths[i] != NULL)
			free(nmonths[i]);
		nmonths[i] = xstrdup(buf);

		strftime(buf, sizeof(buf), "%B", &tm);
		if (fnmonths[i] != NULL)
			free(fnmonths[i]);
		fnmonths[i] = xstrdup(buf);
	}
}

void
setnsequences(const char *seq)
{
	const char *p;
	int nspace = 0;

	for (p = seq; *p; p++) {
		if (*p == ' ')
			nspace++;
	}
	if (nspace != NSEQUENCES - 1) {
		warnx("Invalid SEQUENCE: %s", seq);
		return;
	}

	p = seq;
	for (int i = 0; i < NSEQUENCES; i++) {
		while (*p != ' ' && *p != '\0')
			p++;

		if (nsequences[i] != NULL)
			free(nsequences[i]);
		nsequences[i] = xcalloc(1, p - seq + 1);
		strncpy(nsequences[i], seq, p - seq);
		seq = ++p;
	}
}
