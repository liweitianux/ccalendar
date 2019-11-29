/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2019 The DragonFly Project.  All rights reserved.
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

#include <stddef.h>

#include "basics.h"
#include "utils.h"

/*
 * Time for the "mean sun" traval from one mean vernal equinox to the next.
 * Ref: Sec.(14.4), Eq.(14.31)
 */
const double mean_tropical_year = 365.242189;

/*
 * Calculate the longitudinal nutation (in degrees) at moment $t.
 * Ref: Sec.(14.4), Eq.(14.34)
 */
double
nutation(double t)
{
	double c = julian_centuries(t);
	double coefsA[] = { 124.90, -1934.134, 0.002063 };
	double coefsB[] = { 201.11, 72001.5377, 0.00057 };
	double A = poly(c, coefsA, nitems(coefsA));
	double B = poly(c, coefsB, nitems(coefsB));
	return -0.004778 * sin_deg(A) - 0.0003667 * sin_deg(B);
}

/*
 * Calculate the aberration (in degrees) at moment $t.
 * Ref: Sec.(14.4), Eq.(14.35)
 */
double
aberration(double t)
{
	double c = julian_centuries(t);
	double A = 177.63 + 35999.01848 * c;
	return 0.0000974 * cos_deg(A) - 0.005575;
}

/*
 * Argument data used by 'solar_longitude()' for calculating the solar
 * longitude.
 * Ref: Sec.(14.4), Table(14.1)
 */
static const struct solar_longitude_arg {
	int	x;
	double	y;
	double	z;
} solar_longitude_data[] = {
	{ 403406, 270.54861,      0.9287892 },
	{ 195207, 340.19128,  35999.1376958 },
	{ 119433,  63.91854,  35999.4089666 },
	{ 112392, 331.26220,  35998.7287385 },
	{   3891, 317.843  ,  71998.20261   },
	{   2819,  86.631  ,  71998.4403    },
	{   1721, 240.052  ,  36000.35726   },
	{    660, 310.26   ,  71997.4812    },
	{    350, 247.23   ,  32964.4678    },
	{    334, 260.87   ,    -19.4410    },
	{    314, 297.82   , 445267.1117    },
	{    268, 343.14   ,  45036.8840    },
	{    242, 166.79   ,      3.1008    },
	{    234,  81.53   ,  22518.4434    },
	{    158,   3.50   ,    -19.9739    },
	{    132, 132.75   ,  65928.9345    },
	{    129, 182.95   ,   9038.0293    },
	{    114, 162.03   ,   3034.7684    },
	{     99,  29.8    ,  33718.148     },
	{     93, 266.4    ,   3034.448     },
	{     86, 249.2    ,  -2280.773     },
	{     78, 157.6    ,  29929.992     },
	{     72, 257.8    ,  31556.493     },
	{     68, 185.1    ,    149.588     },
	{     64,  69.9    ,   9037.750     },
	{     46,   8.0    , 107997.405     },
	{     38, 197.1    ,  -4444.176     },
	{     37, 250.4    ,    151.771     },
	{     32,  65.3    ,  67555.316     },
	{     29, 162.7    ,  31556.080     },
	{     28, 341.5    ,  -4561.540     },
	{     27, 291.6    , 107996.706     },
	{     27,  98.5    ,   1221.655     },
	{     25, 146.7    ,  62894.167     },
	{     24, 110.0    ,  31437.369     },
	{     21,   5.2    ,  14578.298     },
	{     21, 342.6    , -31931.757     },
	{     20, 230.9    ,  34777.243     },
	{     18, 256.1    ,   1221.999     },
	{     17,  45.3    ,  62894.511     },
	{     14, 242.9    ,  -4442.039     },
	{     13, 115.2    , 107997.909     },
	{     13, 151.8    ,    119.066     },
	{     13, 285.3    ,  16859.071     },
	{     12,  53.3    ,     -4.578     },
	{     10, 126.6    ,  26895.292     },
	{     10, 205.7    ,    -39.127     },
	{     10,  85.9    ,  12297.536     },
	{     10, 146.1    ,  90073.778     },
};

/*
* Calculate the longitude (in degrees) of Sun at moment $t.
 * Ref: Sec.(14.4), Eq.(14.33)
 */
double
solar_longitude(double t)
{
	double c = julian_centuries(t);

	double sum = 0.0;
	const struct solar_longitude_arg *arg;
	for (size_t i = 0; i < nitems(solar_longitude_data); i++) {
		arg = &solar_longitude_data[i];
		sum += arg->x * sin_deg(arg->y + arg->z * c);
	}
	double lambda = (282.7771834 + 36000.76953744 * c +
			 0.000005729577951308232 * sum);

	double ab = aberration(t);
	double nu = nutation(t);

	return mod_f(lambda + ab + nu, 360);
}

/*
 * Calculate the approximate moment at or before the given moment $t when
 * the solar longitude just exceeded the given degree $lambda.
 * Ref: Sec.(14.5), Eq.(14.42)
 */
double
estimate_prior_solar_longitude(double lambda, double t)
{
	double rate = mean_tropical_year / 360.0;

	/* first approximation */
	double lon = solar_longitude(t);
	double tau = t - rate * mod_f(lon - lambda, 360);

	/* refine the estimate to within a day */
	lon = solar_longitude(tau);
	double delta = mod3_f(lon - lambda, -180, 180);
	double t2 = tau - rate * delta;

	return (t < t2) ? t : t2;
}

/*
 * Calculate the geocentric altitude of Sun at moment $t and at location
 * ($latitude, $longitude), ignoring parallax and refraction.
 * Ref: Sec.(14.4), Eq.(14.41)
 */
double
solar_altitude(double t, double latitude, double longitude)
{
	double lambda = solar_longitude(t);
	double alpha = right_ascension(t, 0, lambda);
	double delta = declination(t, 0, lambda);
	double theta = sidereal_from_moment(t);
	double H = mod_f(theta + longitude - alpha, 360);

	double v = (sin_deg(latitude) * sin_deg(delta) +
		    cos_deg(latitude) * cos_deg(delta) * cos_deg(H));
	return mod3_f(arcsin_deg(v), -180, 180);
}

/*
 * Ref: Sec.(14.7), Eq.(14.68)
 */
double
approx_depression_moment(double t)
{
	// TODO
	return 0.0;
}

/*
 * Calculate the moment, in local mean time, when the depression angle
 * of the geometric center of Sun is $alpha degrees below (or above if
 * the $alpha is negative) the geometric horizon at sea level at a given
 * location around a given moment $t.
 * Ref: Sec.(14.7), Eq.(14.68)
 */
double
depression_moment(double t)
{
	// TODO
	return 0.0;
}
