/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2020 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Aaron LI <aly@aaronly.me>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef PARSEDATA_H_
#define PARSEDATA_H_

#include <stdbool.h>

#define	F_NONE			0x00000
#define	F_MONTH			0x00001
#define	F_DAYOFWEEK		0x00002
#define	F_DAYOFMONTH		0x00004
#define	F_MODIFIERINDEX		0x00008
#define	F_MODIFIEROFFSET	0x00010
#define	F_SPECIALDAY		0x00020
#define	F_ALLMONTH		0x00040
#define	F_ALLDAY		0x00080
#define	F_VARIABLE		0x00100
#define	F_EASTER		0x00200
#define	F_CNY			0x00400
#define	F_PASKHA		0x00800
#define	F_NEWMOON		0x01000
#define	F_FULLMOON		0x02000
#define	F_MAREQUINOX		0x04000
#define	F_SEPEQUINOX		0x08000
#define	F_JUNSOLSTICE		0x10000
#define	F_DECSOLSTICE		0x20000
#define	F_YEAR			0x40000

#define	STRING_EASTER		"Easter"
#define	STRING_PASKHA		"Paskha"
#define	STRING_CNY		"ChineseNewYear"
#define	STRING_NEWMOON		"NewMoon"
#define	STRING_FULLMOON		"FullMoon"
#define	STRING_MAREQUINOX	"MarEquinox"
#define	STRING_SEPEQUINOX	"SepEquinox"
#define	STRING_JUNSOLSTICE	"JunSolstice"
#define	STRING_DECSOLSTICE	"DecSolstice"

/*
 * All the astronomical calculations are carried out for the meridian 120
 * degrees east of Greenwich.
 */
#define UTCOFFSET_CNY		8.0

int	parsedaymonth(const char *date, int *yearp, int *monthp, int *dayp,
		      int *flags, char **edp);
void	dodebug(const char *type);

bool	parse_timezone(const char *s, long *result);
bool	parse_location(const char *s, double *latitude, double *longitude,
		       double *elevation);

#endif
