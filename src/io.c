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
#include "parsedata.h"
#include "utils.h"

enum { C_NONE, C_LINE, C_BLOCK };

/* default calendar file to use if exists in current dir or ~/.calendar */
static const char *calendarFile = "calendar";
/* systemd-wide calendar file to use if user doesn't have one */
static const char *calendarFileSys = CALENDAR_ETCDIR "/default";
/* paths to search for calendar files for inclusion */
static const char *calendarDirs[] = {
	".calendar",  /* relative to $HOME */
	CALENDAR_ETCDIR,
	CALENDAR_DIR,
};
/* don't send mail if this file exists in ~/.calendar */
static const char *calendarNoMail = "nomail";

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
static bool	 cal_parse(FILE *in, FILE *out);
static void	 cd_home(void);
static void	 closecal(FILE *fp);
static FILE	*opencalin(void);
static char	*skip_comment(char *line, int *comment);
static bool	 tokenize(char *line, FILE *out, bool *skip);
static void	 write_mailheader(int fd);


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
	size_t i;

	for (i = 0; i < nitems(calendarDirs); i++) {
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
tokenize(char *line, FILE *out, bool *skip)
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
		if (!cal_parse(cal_fopen(walk), out))
			return false;

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
	logdebug("%s(): d_fmt=|%s|\n", __func__, d_fmt);
	/* NOTE: BSDs use '%e' in D_FMT while Linux uses '%d' */
	return (strpbrk(d_fmt, "ed") < strchr(d_fmt, 'm'));
}

static bool
cal_parse(FILE *in, FILE *out)
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
	struct cal_day	*cdays[MAXCOUNT] = { NULL };
	struct event	*events[MAXCOUNT] = { NULL };
	char		*extradata[MAXCOUNT] = { NULL };

	if (in == NULL)
		return (false);

	d_first = locale_day_first();

	while ((linelen = getline(&line, &linecap, in)) > 0) {
		buf = skip_comment(line, &comment);
		buf = trimr(buf);  /* Need to keep the leading tabs */
		if (*buf == '\0')
			continue;

		if (*buf == '#') {
			if (!tokenize(buf, out, &skip)) {
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
			setnnames();
			locale_changed = true;
			logdebug("%s(): set LC_ALL='%s' (day_first=%s)\n",
				 __func__, lang, d_first ? "true" : "false");

			continue;
		}

#define	REPLACE(string, nvar) \
		if (strncasecmp(buf, (string), strlen(string)) == 0 &&	\
		    buf[strlen(string)]) {				\
			if (nvar != NULL)				\
				free(nvar);				\
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
			setnsequences(buf + strlen("SEQUENCE="));
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
			logdebug("%s() ignored invalid: |%s|\n", __func__, buf);
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
			logdebug("%s() ignored: |%s|\n", __func__, buf);
			continue;
		}

		/* Find the last tab */
		while (pp[1] == '\t')
			pp++;

		for (int i = 0; i < count; i++) {
			logdebug("%s() got: |%s|\n", __func__, pp);
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
		setnnames();
	}

	free(line);
	fclose(in);
	for (int i = 0; i < MAXCOUNT; i++) {
		if (extradata[i] != NULL)
			free(extradata[i]);
	}

	return (true);
}

int
cal(void)
{
	FILE *fpin;
	FILE *fpout;

	if ((fpin = opencalin()) == NULL)
		return 1;

	if (Options.allmode) {
		/*
		 * Use a temporary output file, so we can skip sending mail
		 * if there is no output.
		 */
		if ((fpout = tmpfile()) == NULL) {
			warn("tmpfile");
			fclose(fpin);
			return 1;
		}

		if (!cal_parse(fpin, fpout)) {
			warnx("Failed to parse calendar files");
			fclose(fpin);
			fclose(fpout);
			return 1;
		}

		event_print_all(fpout);
		closecal(fpout);

	} else {
		if (!cal_parse(fpin, stdout)) {
			warnx("Failed to parse calendar files");
			return 1;
		}
		event_print_all(stdout);
	}

	list_freeall(definitions, free, NULL);
	definitions = NULL;

	return 0;
}

static FILE *
opencalin(void)
{
	FILE *fpin = NULL;
	const char *calfile;
	char fpath[MAXPATHLEN];

	calfile = Options.calendarFile ? Options.calendarFile : calendarFile;

	if (Options.allmode) {
		/* already in $HOME */
		snprintf(fpath, sizeof(fpath), "%s/%s",
			 calendarDirs[0], calendarNoMail);
		if (access(fpath, F_OK) == 0)
			return (NULL);
	} else {
		fpin = fopen(calfile, "r");
		cd_home();
	}

	if (fpin == NULL) {
		if (Options.calendarFile)
			errx(1, "No calendar file: '%s'", calfile);

		snprintf(fpath, sizeof(fpath), "%s/%s",
			 calendarDirs[0], calendarFile);
		if ((fpin = fopen(fpath, "r")) == NULL &&
		    (fpin = fopen(calendarFileSys, "r")) == NULL) {
			errx(1, "No calendar file: '%s' or '~/%s'",
					calendarFile, fpath);
		}
	}

	return (fpin);
}

static void
closecal(FILE *fp)
{
	int pdes[2];
	char buf[1024];
	ssize_t nread;

	assert(Options.allmode == true);

	if (fseek(fp, 0L, SEEK_END) != 0 || ftell(fp) == 0)
		return;
	if (pipe(pdes) < 0) {
		warnx("Cannot open pipe");
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

	write_mailheader(pdes[1]);
	rewind(fp);
	while ((nread = read(fileno(fp), buf, sizeof(buf))) > 0)
		write(pdes[1], buf, (size_t)nread);
	close(pdes[1]);

done:
	fclose(fp);
	while (wait(NULL) >= 0)
		;
}

static void
write_mailheader(int fd)
{
	uid_t uid = getuid();
	struct passwd *pw = getpwuid(uid);
	struct date date;
	FILE *fp = fdopen(fd, "w");
	char dayname[32] = { 0 };
	enum dayofweek dow;

	gregorian_from_fixed(Options.today, &date);
	dow = dayofweek_from_fixed(Options.today);
	sprintf(dayname, "%s, %02d %s %04d",
		fdays[dow], date.day, fmonths[date.month-1], date.year);

	fprintf(fp,
		"From: %s (Reminder Service)\n"
		"To: %s\n"
		"Subject: %s's Calendar\n"
		"Precedence: bulk\n"
		"Auto-Submitted: auto-generated\n\n",
		pw->pw_name, pw->pw_name, dayname);
	fflush(fp);
	/* No need to close fp */
}

static void
cd_home(void)
{
	char *home;

	home = getenv("HOME");
	if (home == NULL || *home == '\0')
		errx(1, "Cannot get home directory");
	if (chdir(home) != 0)
		errx(1, "Cannot enter home directory: \"%s\"", home);
}
