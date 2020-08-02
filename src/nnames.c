/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2020 The DragonFly Project.  All rights reserved.
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "calendar.h"
#include "nnames.h"
#include "utils.h"


#define NNAME_INIT0 \
	{ NULL, 0, NULL, 0, NULL, 0, NULL, 0 }
#define NNAME_INIT1(name) \
	{ name, sizeof(name)-1, NULL, 0, NULL, 0, NULL, 0 }
#define NNAME_INIT2(name, f_name) \
	{ name, sizeof(name)-1, f_name, sizeof(f_name)-1, NULL, 0, NULL, 0 }

/* names of every day of week */
struct nname dow_names[NDOWS+1] = {
	NNAME_INIT2("Sun", "Sunday"),
	NNAME_INIT2("Mon", "Monday"),
	NNAME_INIT2("Tue", "Tuesday"),
	NNAME_INIT2("Wed", "Wednesday"),
	NNAME_INIT2("Thu", "Thursday"),
	NNAME_INIT2("Fri", "Friday"),
	NNAME_INIT2("Sat", "Saturday"),
	NNAME_INIT0,
};

/* names of every month */
struct nname month_names[NMONTHS+1] = {
	NNAME_INIT2("Jan", "January"),
	NNAME_INIT2("Feb", "February"),
	NNAME_INIT2("Mar", "March"),
	NNAME_INIT2("Apr", "April"),
	NNAME_INIT2("May", "May"),
	NNAME_INIT2("Jun", "June"),
	NNAME_INIT2("Jul", "July"),
	NNAME_INIT2("Aug", "August"),
	NNAME_INIT2("Sep", "September"),
	NNAME_INIT2("Oct", "October"),
	NNAME_INIT2("Nov", "November"),
	NNAME_INIT2("Dec", "December"),
	NNAME_INIT0,
};

/* names of every sequence */
struct nname sequence_names[NSEQUENCES+1] = {
	NNAME_INIT1("First"),
	NNAME_INIT1("Second"),
	NNAME_INIT1("Third"),
	NNAME_INIT1("Fourth"),
	NNAME_INIT1("Fifth"),
	NNAME_INIT1("Last"),
	NNAME_INIT0,
};


void
set_nnames(void)
{
	char buf[64];
	struct tm tm;
	struct nname *nname;

	memset(&tm, 0, sizeof(tm));
	for (int i = 0; i < NDOWS; i++) {
		nname = &dow_names[i];
		tm.tm_wday = i;

		strftime(buf, sizeof(buf), "%a", &tm);
		free(nname->n_name);
		nname->n_name = xstrdup(buf);
		nname->n_len = strlen(nname->n_name);

		strftime(buf, sizeof(buf), "%A", &tm);
		free(nname->fn_name);
		nname->fn_name = xstrdup(buf);
		nname->fn_len = strlen(nname->fn_name);
	}

	memset(&tm, 0, sizeof(tm));
	for (int i = 0; i < NMONTHS; i++) {
		nname = &month_names[i];
		tm.tm_mon = i;

		strftime(buf, sizeof(buf), "%b", &tm);
		free(nname->n_name);
		/* The month may have a leading blank (e.g., on *BSD) */
		nname->n_name = xstrdup(triml(buf));
		nname->n_len = strlen(nname->n_name);

		strftime(buf, sizeof(buf), "%B", &tm);
		free(nname->fn_name);
		nname->fn_name = xstrdup(triml(buf));
		nname->fn_len = strlen(nname->fn_name);
	}
}

void
set_nsequences(const char *seq)
{
	struct nname *nname;
	const char *p = seq;
	size_t len;

	if (count_char(seq, ' ') != NSEQUENCES - 1) {
		warnx("Invalid SEQUENCE: |%s|", seq);
		return;
	}

	for (int i = 0; i < NSEQUENCES; i++) {
		while (*p != ' ' && *p != '\0')
			p++;

		len = (size_t)(p - seq);
		nname = &sequence_names[i];
		free(nname->n_name);
		nname->n_name = xcalloc(1, len + 1);
		strncpy(nname->n_name, seq, len);
		nname->n_len = strlen(nname->n_name);

		seq = ++p;
	}
}
