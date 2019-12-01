#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "basics.h"
#include "chinese.h"
#include "gregorian.h"
#include "moon.h"
#include "sun.h"
#include "utils.h"

int
main(void)
{
	/*
	int rds[] = { -214193, -61387, 25469, 49217, 171307, 210155,
		      253427, 369740, 400085, 434355, 452605, 470160,
		      473837, 507850, 524156, 544676, 567118, 569477,
		      601716, 613424, 626596, 645554, 664224, 671401,
		      694799, 704424, 708842, 709409, 709580, 727274,
		      728714, 744313, 764652 };
	*/
	int rds[] = { -214193, 253427, 473837, 601716, 694799, 728714 };

	int rd, rd2;
	struct g_date date;
	double c;
	printf("R.D.\t(Y, M, D)\tRD2\tEq?\tJcen\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		date = gregorian_from_fixed(rd);
		rd2 = fixed_from_gregorian(&date);
		c = julian_centuries(rd);
		printf("%7d\t(%4d, %2d, %2d)\t%7d\t%d\t%10.6lf\n",
				rd, date.year, date.month, date.day, rd2,
				rd==rd2, c);
	}

	double t;
	double ephemeris;
	double eot;
	double solar_lon;
	double lambda, next_se;
	printf("\nR.D.\tEphemeris\tEqOfTime\tSolarLongitude\tNextSolstice/Equinox\tS/E/deg\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		t = (double)rd;
		ephemeris = ephemeris_correction(t);
		eot = equation_of_time(t);
		solar_lon = solar_longitude(t + 0.5);  // at 12:00:00 UT
		lambda = mod_f(ceil(solar_lon / 90) * 90, 360);
		next_se = solar_longitude_atafter(lambda, t);
		printf("%7d\t%.8lf\t%12.8lf\t%12.8lf\t%16.8lf\t%6.2lf\n",
				rd, ephemeris, eot, solar_lon, next_se, lambda);
	}

	/* Location of Jerusalem (Eq. 14.4) */
	struct location jerusalem = {
		.latitude = 31.78,
		.longitude = 35.24,
		.elevation = 740.0,
		.zone = 2.0 / 24.0,
	};
	double t_sunset;
	printf("\nR.D.\tSunset[Jerusalem]\tSS[hh:mm:ss]\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		t_sunset = sunset(rd, &jerusalem) - (double)rd;
		int h, m, s;
		h = (int)(t_sunset * 24);
		m = (int)(t_sunset * 24*60) % 60;
		s = (int)(t_sunset * 24*60*60) % 60;
		printf("%7d\t%.8lf\t\t%02d:%02d:%02d\n",
				rd, t_sunset, h, m, s);
	}

	/* Location of Mecca (Eq. 14.3) */
	struct location mecca = {
		.latitude = angle2deg(21, 25, 24),
		.longitude = angle2deg(39, 49, 24),
		.elevation = 298.0,
		.zone = 3.0 / 24.0,
	};
	double l_lon, l_lat, l_alt, l_dis;
	printf("\nR.D.\tLunarLongitude\tLunarLatitude\tLunarAltitude\tLunarDistance\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		t = (double)rd;
		l_lon = lunar_longitude(t);
		l_lat = lunar_latitude(t);
		l_alt = lunar_altitude(t, mecca.latitude, mecca.longitude);
		l_dis = lunar_distance(t);
		printf("%7d\t%12.8lf\t%12.8lf\t%12.8lf\t%12g\n",
				rd, l_lon, l_lat, l_alt, l_dis);
	}

	double t_newmoon, t_moonrise, t_moonset;
	printf("\nR.D.\tNextNewMoon[Mecca]\tMoonrise\tMR[hh:mm:ss]\tMoonset\t\tMS[hh:mm:ss]\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		t = (double)rd;
		t_newmoon = new_moon_atafter(t);
		t_moonrise = moonrise(rd, &mecca) - (double)rd;
		t_moonset = moonset(rd, &mecca) - (double)rd;
		int h1, m1, s1;
		h1 = (int)(t_moonrise * 24);
		m1 = (int)(t_moonrise * 24*60) % 60;
		s1 = (int)(t_moonrise * 24*60*60) % 60;
		int h2, m2, s2;
		h2 = (int)(t_moonset * 24);
		m2 = (int)(t_moonset * 24*60) % 60;
		s2 = (int)(t_moonset * 24*60*60) % 60;
		printf("%7d\t%16.8lf\t%10.8lf\t%02d:%02d:%02d\t%10.8lf\t%02d:%02d:%02d\n",
				rd, t_newmoon, t_moonrise, h1, m1, s1, t_moonset, h2, m2, s2);
	}

	double ob = obliquity(rd);
	double ra = right_ascension(rd, 30, 50);
	double dec = declination(rd, 30, 50);
	double st = sidereal_from_moment(rd);
	printf("\nobliquity = %.12lf\n", ob);
	printf("ra = %.12lf; dec = %.12lf\n", ra, dec);
	printf("sidereal_time = %.12lf\n", st);

	struct g_date zh_epoch_date = { -2636, 2, 15 };
	int zh_epoch = fixed_from_gregorian(&zh_epoch_date);
	printf("\nchinese_epoch: RD (%d) <-> Gregorian (%d, %d, %d)\n",
			zh_epoch, zh_epoch_date.year,
			zh_epoch_date.month, zh_epoch_date.day);

	struct chinese_date zh_date;
	printf("\nR.D.\t(C, Y, M, L, D)\t\tRD2\tEq?\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		zh_date = chinese_from_fixed(rd);
		rd2 = fixed_from_chinese(&zh_date);
		printf("%7d\t(%2d, %2d, %2d%c, %2d)\t%7d\t%d\n",
				rd, zh_date.cycle, zh_date.year, zh_date.month,
				zh_date.leap ? '+' : ' ', zh_date.day, rd2, rd==rd2);
	}

	time_t tt;
	struct tm tm;

	printf("\n-----------------------------------------------------------\n");
	time(&tt);
	localtime_r(&tt, &tm);
	date = (struct g_date){ tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday };
	rd = fixed_from_gregorian(&date);
	zh_date = chinese_from_fixed(rd);
	printf("%7d\t(%4d, %2d, %2d)\t(%2d, %2d, %2d%c, %2d)\n",
			rd, date.year, date.month, date.day,
			zh_date.cycle, zh_date.year, zh_date.month,
			zh_date.leap ? '+' : ' ', zh_date.day);

	printf("\n-----------------------------------------------------------\n");
	show_chinese_calendar(rd);
	show_chinese_calendar(601716);

	return 0;
}
