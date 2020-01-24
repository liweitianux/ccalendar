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
 * $FreeBSD: head/usr.bin/calendar/events.c 326276 2017-11-27 15:37:16Z pfg $
 */

#include <sys/time.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calendar.h"
#include "utils.h"

struct event *
event_add(int year, int month, int day, char *date, bool variable,
	  char *txt, char *extra)
{
	struct event *e;

	e = xcalloc(1, sizeof(*e));
	e->month = month;
	e->day = day;
	e->variable = variable;
	e->date = xstrdup(date);
	e->text = xstrdup(txt);
	e->extra = NULL;
	if (extra != NULL && extra[0] != '\0')
		e->extra = xstrdup(extra);

	addtodate(e, year, month, day);
	return (e);
}

void
event_continue(struct event *e, char *txt)
{
	char *text;
	size_t len;

	/* Includes a '\n' and a NUL */
	len = strlen(e->text) + strlen(txt) + 2;
	text = xcalloc(1, len);
	snprintf(text, len, "%s\n%s", e->text, txt);
	free(e->text);
	e->text = text;
}

void
event_print_all(FILE *fp)
{
	struct event *e;

	while (walkthrough_dates(&e) != 0) {
#ifdef DEBUG
		fprintf(stderr, "event_print_allmonth: %d, day: %d\n",
		    month, day);
#endif

		/*
		 * Go through all events and print the text of the matching
		 * dates
		 */
		while (e != NULL) {
			fprintf(fp, "%s%c%s%s%s%s\n", e->date,
			    e->variable ? '*' : ' ', e->text,
			    e->extra != NULL ? " (" : "",
			    e->extra != NULL ? e->extra : "",
			    e->extra != NULL ? ")" : ""
			);

			e = e->next;
		}
	}
}
