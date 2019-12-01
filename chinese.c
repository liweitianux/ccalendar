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

/*
 * Implement the Chinese calendar of the 1645 version, established in the
 * second year of the Qīng dynasty.
 *
 * The winter solstice (dōngzhì; 冬至) always occurs during the eleventh
 * month of the year.  The winter-solstice-to-winter-solstice period is
 * called a suì (岁).
 *
 * The leap month of a 13-month winter-solstice-to-winter-solstice period
 * is the first month that does not contain a major solar term --- that is,
 * the first lunar month that is wholly within a solar month.
 */

#include <err.h>
#include <math.h>
#include <stdbool.h>

#include "basics.h"
#include "chinese.h"
#include "gregorian.h"
#include "moon.h"
#include "sun.h"
#include "utils.h"

/*
 * Fixed date of the start of the Chinese calendar.
 * Ref: Sec.(19.3), Eq.(19.15)
 */
static const int epoch = -963099;  /* Gregorian: -2636, February, 15 */

/*
 * Timezone (in fraction of days) of Beijing adopted in Chinese calendar
 * calculations.
 * Ref: Sec.(19.1), Eq.(19.2)
 */
static double
chinese_zone(int rd)
{
	if (gregorian_year_from_fixed(rd) < 1929)
		return (1397.0 / 180.0 / 24.0);
	else
		return (8.0 / 24.0);
}

/*
 * Location and timezone (in fraction of days) of Beijing adopted in Chinese
 * calendar calculations.
 * Ref: Sec.(19.1), Eq.(19.2)
 */
static struct location
chinese_location(int rd)
{
	struct location loc = {
		.latitude = angle2deg(116, 25, 0),
		.longitude = angle2deg(39, 55, 0),
		.elevation = 43.5,
		.zone = chinese_zone(rd),
	};

	return loc;
}

/*
 * The universal time (UT) of (clock) midnight at the start of the fixed date
 * $rd in China.
 * Ref: Sec.(19.1), Eq.(19.7)
 */
static double
midnight_in_china(int rd)
{
	return (double)rd - chinese_zone(rd);
}

/*
 * Calculate the last Chinese major solar term (zhōngqì) in range of [1, 12]
 * before the fixed date $rd.
 * Ref: Sec.(19.1), Eq.(19.1)
 */
int
current_major_solar_term(int rd)
{
	double ut = midnight_in_china(rd);
	double lon = solar_longitude(ut);
	return mod1(2 + div_floor(lon, 30), 12);
}

/*
 * Calculate the fixed date (in China) of winter solstice on or before the
 * given fixed date $rd.
 * Ref: Sec.(19.1), Eq.(19.8)
 */
int
chinese_winter_solstice_onbefore(int rd)
{
	/* longitude of Sun at winter solstice */
	const double winter = 270.0;

	/* approximate the time of winter solstice */
	double t = midnight_in_china(rd + 1);
	double approx = estimate_prior_solar_longitude(winter, t);

	/* search for the date of winter solstice */
	int day = (int)floor(approx) - 1;
	while (winter >= solar_longitude(midnight_in_china(day+1)))
		day++;

	return day;
}

/*
 * Calculate the fixed date (in China) of the first new moon on or after the
 * given fixed date $rd.
 * Ref: Sec.(19.2), Eq.(19.9)
 */
int
chinese_new_moon_onafter(int rd)
{
	double t = new_moon_atafter(midnight_in_china(rd));
	double st = t + chinese_zone(rd);  /* in standard time */

	return (int)floor(st);
}

/*
 * Calculate the fixed date (in China) of the first new moon before the
 * given fixed date $rd.
 * Ref: Sec.(19.2), Eq.(19.10)
 */
int
chinese_new_moon_before(int rd)
{
	double t = new_moon_before(midnight_in_china(rd));
	double st = t + chinese_zone(rd);  /* in standard time */

	return (int)floor(st);
}

/*
 * Determine whether the Chinese lunar month starting on the given fixed
 * date $rd has no major solar term.
 * Ref: Sec.(19.2), Eq.(19.11)
 */
bool
chinese_no_major_solar_term(int rd)
{
	int rd2 = chinese_new_moon_onafter(rd + 1);
	return (current_major_solar_term(rd) ==
		current_major_solar_term(rd2));
}

/*
 * Recursively determine whether there is a Chinese leap month on or
 * after the lunar month starting on fixed date $m1 and at or before
 * the lunar month starting on fixed date $m2.
 * Ref: Sec.(19.2), Eq.(19.12)
 */
bool
chinese_prior_leap_month(int m1, int m2)
{
	int m2_prev = chinese_new_moon_before(m2);
	return ((m2 >= m1) &&
		(chinese_no_major_solar_term(m2) ||
		 chinese_prior_leap_month(m1, m2_prev)));
}

/*
 * Calculate the fixed date of Chinese New Year in suì containing the
 * given date $rd.
 * Ref: Sec.(19.2), Eq.(19.13)
 */
int
chinese_new_year_in_sui(int rd)
{
	/* prior and following winter solstice */
	int s1 = chinese_winter_solstice_onbefore(rd);
	int s2 = chinese_winter_solstice_onbefore(s1 + 370);

	/* month after the 11th month: either 12 or leap 11 */
	int m12 = chinese_new_moon_onafter(s1 + 1);
	/* month after m12: either 12 (or leap 12) or 1 */
	int m13 = chinese_new_moon_onafter(m12 + 1);
	/* the next 11th month */
	int m11_next = chinese_new_moon_before(s2 + 1);

	bool leap_year = lround((m11_next - m12) / mean_synodic_month) == 12;
	if (leap_year && (chinese_no_major_solar_term(m12) ||
			  chinese_no_major_solar_term(m13))) {
		/* either m12 or m13 is a leap month */
		return chinese_new_moon_onafter(m13 + 1);
	} else {
		return m13;
	}
}

/*
 * Calculate the fixed date of Chinese New Year on or before the given
 * fixed date $rd.
 * Ref: Sec.(19.2), Eq.(19.14)
 */
int
chinese_new_year_onbefore(int rd)
{
	int newyear = chinese_new_year_in_sui(rd);
	if (rd >= newyear)
		return newyear;
	else
		return chinese_new_year_in_sui(rd - 180);
}

/*
 * Calculate the fixed date of Chinese New Year in Gregorian year $year.
 * Ref: Sec.(19.6), Eq.(19.26)
 */
int
chinese_new_year(int year)
{
	struct g_date date = { year, 7, 1 };
	int july1 = fixed_from_gregorian(date);
	return chinese_new_year_onbefore(july1);
}

/*
 * Calculate the Chinese date (cycle, year, month, leap, day) corresponding
 * to the fixed date $rd.
 * Ref: Sec.(19.3), Eq.(19.16)
 */
struct chinese_date
chinese_from_fixed(int rd)
{
	/* prior and following winter solstice */
	int s1 = chinese_winter_solstice_onbefore(rd);
	int s2 = chinese_winter_solstice_onbefore(s1 + 370);

	/* start of month containing the given date */
	int m = chinese_new_moon_before(rd + 1);
	/* start of the previous month */
	int m_prev = chinese_new_moon_before(m);
	/* month after the last 11th month: either 12 or leap 11 */
	int m12 = chinese_new_moon_onafter(s1 + 1);
	/* the next 11th month */
	int m11_next = chinese_new_moon_before(s2 + 1);

	bool leap_year = lround((m11_next - m12) / mean_synodic_month) == 12;
	int month = (int)lround((m - m12) / mean_synodic_month);
	if (leap_year && chinese_prior_leap_month(m12, m))
		month--;
	month = mod1(month, 12);

	bool leap_month = (leap_year &&
			   chinese_no_major_solar_term(m) &&
			   !chinese_prior_leap_month(m12, m_prev));

	int elapsed_years = (int)floor(1.5 - month/12.0 +
				       (rd - epoch) / mean_tropical_year);
	int cycle = div_floor(elapsed_years - 1, 60) + 1;
	int year = mod1(elapsed_years, 60);

	int day = rd - m + 1;

	struct chinese_date date = { cycle, year, month, leap_month, day };
	return date;
}

/*
 * Calculate the fixed date corresponding to the given Chinese date $date
 * (cycle, year, month, leap, day).
 * Ref: Sec.(19.3), Eq.(19.17)
 */
int
fixed_from_chinese(const struct chinese_date *date)
{
	int midyear = (int)floor(epoch + mean_tropical_year *
				 ((date->cycle - 1) * 60 + date->year - 0.5));
	int newyear = chinese_new_year_onbefore(midyear);

	/* new moon before the given date */
	int newmoon = chinese_new_moon_onafter(
			newyear + (date->month - 1) * 29);
	struct chinese_date date2 = chinese_from_fixed(newmoon);
	if (date->month != date2.month || date->leap != date2.leap) {
		/* there was a prior leap month, so get the next month */
		newmoon = chinese_new_moon_onafter(newmoon + 1);
	}

	return newmoon + date->day - 1;
}
