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
 * @(#)calendar.c  8.3 (Berkeley) 3/25/94
 * $FreeBSD: head/usr.bin/calendar/io.c 327117 2017-12-23 21:04:32Z eadler $
 */

#include <sys/param.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <langinfo.h>
#include <locale.h>
#include <paths.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calendar.h"
#include "basics.h"
#include "gregorian.h"
#include "nnames.h"
#include "parsedata.h"
#include "utils.h"


enum { C_NONE, C_LINE, C_BLOCK };
enum { T_NONE, T_TOKEN, T_VARIABLE, T_DATE };

struct cal_entry {
	int   type;		/* type of the read entry */
	char *token;		/* token to process (T_TOKEN) */
	char *variable;		/* variable name (T_VARIABLE) */
	char *value;		/* variable value (T_VARIABLE) */
	char *date;		/* date of event (T_DATE) */
	char *contents[CAL_MAX_LINES];  /* content lines of event (T_DATE) */
};

struct cal_file {
	FILE	*fp;
	char	*line;		/* line string read from file */
	size_t	 line_cap;	/* capacity of the 'line' buffer */
	char	*nextline;	/* to store the rewinded line */
	size_t	 nextline_cap;	/* capacity of the 'nextline' buffer */
	bool	 rewinded;	/* if 'nextline' has the rewinded line */
};

static struct node *definitions = NULL;

/* National names for special days */
char *neaster = NULL;
char *npaskha = NULL;
char *ncny = NULL;
char *nfullmoon = NULL;
char *nnewmoon = NULL;
char *nmarequinox = NULL;
char *nsepequinox = NULL;
char *njunsolstice = NULL;
char *ndecsolstice = NULL;

static FILE	*cal_fopen(const char *file);
static bool	 cal_parse(FILE *in);
static bool	 cal_readentry(struct cal_file *cfile,
			       struct cal_entry *entry, bool skip);
static char	*cal_readline(struct cal_file *cfile);
static void	 cal_rewindline(struct cal_file *cfile);
static bool	 is_date_entry(char *line, char **content);
static bool	 is_variable_entry(char *line, char **value);
static bool	 process_token(char *line, bool *skip);
static void	 send_mail(FILE *fp);
static char	*skip_comment(char *line, int *comment);
static void	 write_mailheader(FILE *fp);


/*
 * XXX: Quoted or escaped comment marks are not supported yet.
 */
static char *
skip_comment(char *line, int *comment)
{
	char *p, *pp;

	if (*comment == C_LINE) {
		*line = '\0';
		*comment = C_NONE;
		return line;
	} else if (*comment == C_BLOCK) {
		for (p = line, pp = p + 1; *p; p++, pp = p + 1) {
			if (*p == '*' && *pp == '/') {
				*comment = C_NONE;
				return p + 2;
			}
		}
		*line = '\0';
		return line;
	} else {
		*comment = C_NONE;
		for (p = line, pp = p + 1; *p; p++, pp = p + 1) {
			if (*p == '/' && (*pp == '/' || *pp == '*')) {
				*comment = (*pp == '/') ? C_LINE : C_BLOCK;
				break;
			}
		}
		if (*comment != C_NONE) {
			pp = skip_comment(p, comment);
			if (pp > p)
				memmove(p, pp, strlen(pp) + 1);
		}
		return line;
	}

	return line;
}


static FILE *
cal_fopen(const char *file)
{
	FILE *fp = NULL;
	char fpath[MAXPATHLEN];

	for (size_t i = 0; calendarDirs[i] != NULL; i++) {
		snprintf(fpath, sizeof(fpath), "%s/%s",
			 calendarDirs[i], file);
		if ((fp = fopen(fpath, "r")) != NULL)
			return (fp);
	}

	warnx("Cannot open calendar file: '%s'", file);
	return (NULL);
}

/*
 * NOTE: input 'line' should have trailing comment and whitespace trimed
 */
static bool
process_token(char *line, bool *skip)
{
	char *walk;

	if (strcmp(line, "#endif") == 0) {
		*skip = false;
		return true;
	}

	if (*skip)  /* deal with nested #ifndef */
		return true;

	if (string_startswith(line, "#include ") ||
	    string_startswith(line, "#include\t")) {
		walk = triml(line + sizeof("#include"));
		if (*walk == '\0') {
			warnx("Expecting arguments after #include");
			return false;
		}
		if (*walk != '<' && *walk != '\"') {
			warnx("Expecting '<' or '\"' after #include");
			return false;
		}

		char a = *walk;
		char c = walk[strlen(walk) - 1];

		switch(c) {
		case '>':
			if (a != '<') {
				warnx("Unterminated include expecting '\"'");
				return false;
			}
			break;
		case '\"':
			if (a != '\"') {
				warnx("Unterminated include expecting '>'");
				return false;
			}
			break;
		default:
			warnx("Unterminated include expecting '%c'",
			      (a == '<') ? '>' : '\"' );
			return false;
		}

		walk++;
		walk[strlen(walk) - 1] = '\0';

		FILE *fpin = cal_fopen(walk);
		if (fpin == NULL)
			return false;
		if (!cal_parse(fpin)) {
			warnx("Failed to parse calendar files");
			fclose(fpin);
			return false;
		}

		fclose(fpin);
		return true;

	} else if (string_startswith(line, "#define ") ||
	           string_startswith(line, "#define\t")) {
		walk = triml(line + sizeof("#define"));
		if (*walk == '\0') {
			warnx("Expecting arguments after #define");
			return false;
		}

		struct node *new = list_newnode(xstrdup(walk), NULL);
		definitions = list_addfront(definitions, new);

		return true;

	} else if (string_startswith(line, "#ifndef ") ||
	           string_startswith(line, "#ifndef\t")) {
		walk = triml(line + sizeof("#ifndef"));
		if (*walk == '\0') {
			warnx("Expecting arguments after #ifndef");
			return false;
		}

		if (list_lookup(definitions, walk, strcmp, NULL))
			*skip = true;

		return true;
	}

	warnx("Unknown token line: |%s|", line);
	return false;
}

static bool
locale_day_first(void)
{
	char *d_fmt = nl_langinfo(D_FMT);
	DPRINTF("%s(): d_fmt=|%s|\n", __func__, d_fmt);
	/* NOTE: BSDs use '%e' in D_FMT while Linux uses '%d' */
	return (strpbrk(d_fmt, "ed") < strchr(d_fmt, 'm'));
}

static bool
cal_parse(FILE *in)
{
	ssize_t		linelen;
	size_t		linecap = 0;
	char		*line = NULL;
	char		*buf, *pp;
	char		dbuf[80];
	bool		d_first;
	bool		skip = false;
	bool		locale_changed = false;
	int		flags;
	int		count = 0;
	int		comment = C_NONE;
	struct cal_day	*cdays[CAL_MAX_REPEAT] = { NULL };
	struct event	*events[CAL_MAX_REPEAT] = { NULL };
	char		*extradata[CAL_MAX_REPEAT] = { NULL };

	assert(in != NULL);

	d_first = locale_day_first();

	while ((linelen = getline(&line, &linecap, in)) > 0) {
		buf = skip_comment(line, &comment);
		buf = trimr(buf);  /* Need to keep the leading tabs */
		if (*buf == '\0')
			continue;

		if (*buf == '#') {
			if (!process_token(buf, &skip)) {
				free(line);
				return (false);
			}
			continue;
		}

		if (skip)
			continue;

		/* Parse special definitions: LANG, Easter, Paskha etc */
		if (string_startswith(buf, "LANG=")) {
			const char *lang = buf + strlen("LANG=");

			if (setlocale(LC_ALL, lang) == NULL)
				warnx("Failed to set LC_ALL='%s'", lang);
			d_first = locale_day_first();
			set_nnames();
			locale_changed = true;
			DPRINTF("%s(): set LC_ALL='%s' (day_first=%s)\n",
				__func__, lang, d_first ? "true" : "false");

			continue;
		}

#define	REPLACE(string, nvar) \
		if (strncasecmp(buf, (string), strlen(string)) == 0 &&	\
		    buf[strlen(string)]) {				\
			free(nvar);					\
			nvar = xstrdup(buf + strlen(string));		\
			continue;					\
		}

		REPLACE("Easter=", neaster);
		REPLACE("Paskha=", npaskha);
		REPLACE("ChineseNewYear=", ncny);
		REPLACE("NewMoon=", nnewmoon);
		REPLACE("FullMoon=", nfullmoon);
		REPLACE("MarEquinox=", nmarequinox);
		REPLACE("SepEquinox=", nsepequinox);
		REPLACE("JunSolstice=", njunsolstice);
		REPLACE("DecSolstice=", ndecsolstice);
#undef	REPLACE

		if (string_startswith(buf, "SEQUENCE=")) {
			set_nsequences(buf + strlen("SEQUENCE="));
			continue;
		}

		/*
		 * If the line starts with a tab, the data has to be
		 * added to the previous line
		 */
		if (buf[0] == '\t') {
			for (int i = 0; i < count; i++)
				event_continue(events[i], buf);
			continue;
		}

		/* Get rid of leading spaces (non-standard) */
		buf = triml(buf);

		/* No tab in the line, then not a valid line, e.g., comment */
		if ((pp = strchr(buf, '\t')) == NULL) {
			DPRINTF("%s() ignored invalid: |%s|\n", __func__, buf);
			continue;
		}

		/* Trim spaces in front of the tab */
		while (isspace((unsigned char)pp[-1]))
			pp--;

		char ch = *pp;
		*pp = '\0';
		snprintf(dbuf, sizeof(dbuf), "%s", buf);
		*pp = ch;

		count = parsedaymonth(dbuf, &flags, cdays, extradata, buf);
		if (count == 0) {
			DPRINTF("%s() ignored: |%s|\n", __func__, buf);
			continue;
		}

		/* Find the last tab */
		while (pp[1] == '\t')
			pp++;

		for (int i = 0; i < count; i++) {
			DPRINTF("%s() got: |%s|\n", __func__, pp);
			events[i] = event_add(cdays[i], d_first,
					      ((flags & F_VARIABLE) != 0),
					      pp, extradata[i]);
		}
	}

	/*
	 * Reset to the default locale, so that one calendar file that changed
	 * the locale (by defining the "LANG" variable) does not interfere the
	 * following calendar files without the "LANG" definition.
	 */
	if (locale_changed) {
		setlocale(LC_ALL, "");
		set_nnames();
	}

	free(line);
	for (int i = 0; i < CAL_MAX_REPEAT; i++)
		free(extradata[i]);

	return (true);
}

static bool
cal_readentry(struct cal_file *cfile, struct cal_entry *entry, bool skip)
{
	char *p, *value, *content;
	int comment, ci;

	memset(entry, 0, sizeof(*entry));
	entry->type = T_NONE;
	comment = C_NONE;

	while ((p = cal_readline(cfile)) != NULL) {
		p = skip_comment(p, &comment);
		p = trimr(p);  /* Need to keep the leading tabs */
		if (*p == '\0')
			continue;

		if (*p == '#') {
			entry->type = T_TOKEN;
			entry->token = xstrdup(p);
			return true;
		}

		if (skip) {
			/* skip entries but tokens (e.g., '#endif') */
			DPRINTF("%s: skip line: |%s|\n", __func__, p);
			continue;
		}

		if (is_variable_entry(p, &value)) {
			value = triml(value);
			if (*value == '\0') {
				warnx("%s: varaible |%s| has no value",
				      __func__, p);
				continue;
			}

			entry->type = T_VARIABLE;
			entry->variable = xstrdup(p);
			entry->value = xstrdup(value);
			return true;
		}

		if (is_date_entry(p, &content)) {
			content = triml(content);
			if (*content == '\0') {
				warnx("%s: date |%s| has no content",
				      __func__, p);
				continue;
			}

			ci = 0;
			entry->type = T_DATE;
			entry->date = xstrdup(p);
			entry->contents[ci++] = xstrdup(content);

			/* Continuous contents of the event */
			while ((p = cal_readline(cfile)) != NULL) {
				p = trimr(skip_comment(p, &comment));
				if (*p == '\0')
					continue;

				if (*p == '\t') {
					content = triml(p);
					if (ci >= CAL_MAX_LINES) {
						warnx("%s: date |%s| has too "
						      "many lines of contents",
						      __func__, entry->date);
						break;
					}
					entry->contents[ci++] = xstrdup(content);
				} else {
					cal_rewindline(cfile);
					break;
				}
			}

			return true;
		}

		warnx("%s: unknown line: |%s|", __func__, p);
	}

	return false;
}

static char *
cal_readline(struct cal_file *cfile)
{
	if (cfile->rewinded) {
		cfile->rewinded = false;
		return cfile->nextline;
	}

	if (getline(&cfile->line, &cfile->line_cap, cfile->fp) <= 0)
		return NULL;

	return cfile->line;
}

static void
cal_rewindline(struct cal_file *cfile)
{
	if (cfile->nextline_cap == 0)
		cfile->nextline = xmalloc(cfile->line_cap);
	else if (cfile->nextline_cap < cfile->line_cap)
		cfile->nextline = xrealloc(cfile->nextline, cfile->line_cap);

	memcpy(cfile->nextline, cfile->line, cfile->line_cap);
	cfile->nextline_cap = cfile->line_cap;
	cfile->rewinded = true;
}

static bool
is_variable_entry(char *line, char **value)
{
	char *p, *eq;

	if (line == NULL)
		return false;
	if (!(*line == '_' || isalpha((unsigned int)*line)))
		return false;
	if ((eq = strchr(line, '=')) == NULL)
		return false;
	for (p = line+1; p < eq; p++) {
		if (!isalnum((unsigned int)*p))
			return false;
	}

	*eq = '\0';
	if (value != NULL)
		*value = eq + 1;

	return true;
}

static bool
is_date_entry(char *line, char **content)
{
	char *p;

	if (*line == '\t')
		return false;
	if ((p = strchr(line, '\t')) == NULL)
		return false;

	*p = '\0';
	if (content != NULL)
		*content = p + 1;

	return true;
}


int
cal(FILE *fpin)
{
	if (!cal_parse(fpin)) {
		warnx("Failed to parse calendar files");
		return 1;
	}

	if (Options.allmode) {
		FILE *fpout;

		/*
		 * Use a temporary output file, so we can skip sending mail
		 * if there is no output.
		 */
		if ((fpout = tmpfile()) == NULL) {
			warn("tmpfile");
			return 1;
		}
		event_print_all(fpout);
		send_mail(fpout);
	} else {
		event_print_all(stdout);
	}

	list_freeall(definitions, free, NULL);
	definitions = NULL;

	return 0;
}


static void
send_mail(FILE *fp)
{
	int ch, pdes[2];
	FILE *fpipe;

	assert(Options.allmode == true);

	if (fseek(fp, 0L, SEEK_END) == -1 || ftell(fp) == 0) {
		DPRINTF("%s(): no events; skip sending mail\n", __func__);
		return;
	}
	if (pipe(pdes) < 0) {
		warnx("pipe");
		return;
	}

	switch (fork()) {
	case -1:
		close(pdes[0]);
		close(pdes[1]);
		goto done;
	case 0:
		/* child -- set stdin to pipe output */
		if (pdes[0] != STDIN_FILENO) {
			dup2(pdes[0], STDIN_FILENO);
			close(pdes[0]);
		}
		close(pdes[1]);
		execl(_PATH_SENDMAIL, "sendmail", "-i", "-t", "-F",
		      "\"Reminder Service\"", (char *)NULL);
		warn(_PATH_SENDMAIL);
		_exit(1);
	}
	/* parent -- write to pipe input */
	close(pdes[0]);

	fpipe = fdopen(pdes[1], "w");
	if (fpipe == NULL) {
		close(pdes[1]);
		goto done;
	}

	write_mailheader(fpipe);
	rewind(fp);
	while ((ch = fgetc(fp)) != EOF)
		fputc(ch, fpipe);
	fclose(fpipe);  /* will also close the underlying fd */

done:
	fclose(fp);
	while (wait(NULL) >= 0)
		;
}

static void
write_mailheader(FILE *fp)
{
	uid_t uid = getuid();
	struct passwd *pw = getpwuid(uid);
	struct date date;
	char dayname[32] = { 0 };
	enum dayofweek dow;

	gregorian_from_fixed(Options.today, &date);
	dow = dayofweek_from_fixed(Options.today);
	sprintf(dayname, "%s, %d %s %d",
		dow_names[dow].f_name, date.day,
		month_names[date.month-1].f_name, date.year);

	fprintf(fp,
		"From: %s (Reminder Service)\n"
		"To: %s\n"
		"Subject: %s's Calendar\n"
		"Precedence: bulk\n"
		"Auto-Submitted: auto-generated\n\n",
		pw->pw_name, pw->pw_name, dayname);
	fflush(fp);
}
