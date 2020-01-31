/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2019-2020 The DragonFly Project.  All rights reserved.
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
 *
 * Reference:
 * Calendrical Calculations, The Ultimate Edition (4th Edition)
 * by Edward M. Reingold and Nachum Dershowitz
 * 2018, Cambridge University Press
 */

#include <err.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

static const double eps = 1e-6;

/*
 * Divide integer $x by integer $y, rounding towards minus infinity.
 */
int
div_floor(int x, int y)
{
	int q = x / y;
	int r = x % y;
	if ((r != 0) && ((r < 0) != (y < 0)))
		q--;
	return q;
}

/*
 * Calculate the remainder of $x divided by $y; the result has the same
 * sign as $y.
 * Ref: Sec.(1.7), Eq.(1.17)
 */
int
mod(int x, int y)
{
	return x - y * div_floor(x, y);
}

double
mod_f(double x, double y)
{
	return x - y * floor(x / y);
}

/*
 * Return the value of ($x % $y) with $y instead of 0, i.e., with value
 * range being [1, $y].
 */
int
mod1(int x, int y)
{
	return y + mod(x, -y);
}

/*
 * Calculate the interval modulus of $x, i.e., shifted into the range
 * [$a, $b).  Return $x if $a = $b.
 * Ref: Sec.(1.7), Eq.(1.24)
 */
int
mod3(int x, int a, int b)
{
	if (a == b)
		return x;
	else
		return a + mod(x - a, b - a);
}

double
mod3_f(double x, double a, double b)
{
	if (fabs(a - b) < eps)
		return x;
	else
		return a + mod_f(x - a, b - a);
}

/*
 * Calculate the polynomial: c[0] + c[1] * x + ... + c[n-1] * x^(n-1)
 */
double
poly(double x, const double *coefs, size_t n)
{
	double p = 0.0;
	double t = 1.0;
	for (size_t i = 0; i < n; i++) {
		p += t * coefs[i];
		t *= x;
	}
	return p;
}

/*
 * Calculate the sine value of degree $deg.
 */
double
sin_deg(double deg)
{
	return sin(M_PI * deg / 180.0);
}

/*
 * Calculate the cosine value of degree $deg.
 */
double
cos_deg(double deg)
{
	return cos(M_PI * deg / 180.0);
}

/*
 * Calculate the tangent value of degree $deg.
 */
double
tan_deg(double deg)
{
	return tan(M_PI * deg / 180.0);
}

/*
 * Calculate the arc sine value (in degrees) of $x.
 */
double
arcsin_deg(double x)
{
	return asin(x) * 180.0 / M_PI;
}

/*
 * Calculate the arc cosine value (in degrees) of $x.
 */
double
arccos_deg(double x)
{
	return acos(x) * 180.0 / M_PI;
}

/*
 * Calculate the arc tangent value (in degrees from 0 to 360) of $y / $x.
 * Error if $x and $y are both zero.
 */
double
arctan_deg(double y, double x)
{
	errno = 0;
	double v = atan2(y, x);
	if (errno == EDOM)
		errx(10, "%s(%g, %g) invalid!", __func__, y, x);
	return mod_f(v * 180.0 / M_PI, 360);
}

/*
 * Convert angle in (degree, arcminute, arcsecond) to degree.
 */
double
angle2deg(int deg, int min, double sec)
{
	return deg + min/60.0 + sec/3600.0;
}

/*
 * Use bisection search to find the inverse of the given angular function
 * $f(x) at value $y (degrees) within time interval [$a, $b].
 * Ref: Sec.(1.8), Eq.(1.36)
 */
double
invert_angular(double (*f)(double), double y, double a, double b)
{
	double x;

	do {
		x = (a + b) / 2.0;
		if (mod_f(f(x) - y, 360) < 180.0)
			b = x;
		else
			a = x;
	} while (fabs(a-b) >= eps);

	return x;
}


/*
 * Like calloc(3) but exit if allocation fails.
 */
void *
xcalloc(size_t number, size_t size)
{
	void *ptr = calloc(number, size);
	if (ptr == NULL)
		errx(1, "xcalloc(%zu, %zu): out of memory", number, size);
	return ptr;
}

/*
 * Like strdup(3) but exit if fail.
 */
char *
xstrdup(const char *str)
{
	char *p = strdup(str);
	if (p == NULL)
		errx(1, "xstrdup: out of memory (length: %zu)", strlen(str));
	return p;
}

/*
 * Count the number of character $ch in string $s.
 */
size_t
count_char(const char *s, int ch)
{
	size_t count = 0;

	for ( ; *s; s++) {
		if (*s == ch)
			count++;
	}

	return count;
}
