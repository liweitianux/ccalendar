#include <err.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "calendar.h"
#include "basics.h"
#include "chinese.h"
#include "ecclesiastical.h"
#include "gregorian.h"
#include "julian.h"
#include "moon.h"
#include "parsedata.h"
#include "sun.h"
#include "utils.h"


/*
 * globals
 * for compatible with calendar.c ... files
 */
struct cal_options Options;
const char *calendarDirs[] = { NULL };


static void
test1(void)
{
	char buf[256];

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
	struct date date, date2;

	double c;
	printf("R.D.\t(Y, M, D)\tRD2\tEq?\tJcen\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		gregorian_from_fixed(rd, &date);
		rd2 = fixed_from_gregorian(&date);
		c = julian_centuries(rd);
		printf("%7d\t(%4d, %2d, %2d)\t%7d\t%d\t%10.6lf\n",
				rd, date.year, date.month, date.day, rd2,
				rd==rd2, c);
	}

	int dow;
	printf("\nR.D.\tDoW\tJulian(Y,M,D)\tRD2\tEq?\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		dow = dayofweek_from_fixed(rd);
		julian_from_fixed(rd, &date);
		rd2 = fixed_from_julian(&date);
		printf("%7d\t%d\t(%4d, %2d, %2d)\t%7d\t%d\n",
				rd, dow, date.year, date.month, date.day,
				rd2, rd==rd2);
	}

	printf("\nR.D.\tEasterJ\tEasterJ(Y,M,D)\tEasterG\tEasterG(Y,M,D)\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		int year = gregorian_year_from_fixed(rd);
		int j_easter = orthodox_easter(year);
		int g_easter = easter(year);
		gregorian_from_fixed(j_easter, &date);
		gregorian_from_fixed(g_easter, &date2);
		printf("%7d\t%7d\t(%4d, %2d, %2d)\t%7d\t(%4d, %2d, %2d)\n",
				rd, j_easter, date.year, date.month, date.day,
				g_easter, date2.year, date2.month, date2.day);
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
	const struct location jerusalem = {
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
		format_time(buf, sizeof(buf), t_sunset);
		printf("%7d\t%.8lf\t\t%s\n", rd, t_sunset, buf);
	}

	/* Location of Mecca (Eq. 14.3) */
	const struct location mecca = {
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
		printf("%7d\t%16.8lf\t", rd, t_newmoon);
		if (isnan(t_moonrise)) {
			printf("%10s\t%8s", "(null)", "(null)");
		} else {
			format_time(buf, sizeof(buf), t_moonrise);
			printf("%10.8lf\t%s", t_moonrise, buf);
		}
		if (isnan(t_moonset)) {
			printf("\t%10s\t%8s\n", "(null)", "(null)");
		} else {
			format_time(buf, sizeof(buf), t_moonset);
			printf("\t%10.8lf\t%s\n", t_moonset, buf);
		}
	}

	double ob = obliquity(rd);
	double ra = right_ascension(rd, 30, 50);
	double dec = declination(rd, 30, 50);
	double st = sidereal_from_moment(rd);
	printf("\nobliquity = %.12lf\n", ob);
	printf("ra = %.12lf; dec = %.12lf\n", ra, dec);
	printf("sidereal_time = %.12lf\n", st);

	struct date zh_epoch_date = { -2636, 2, 15 };
	int zh_epoch = fixed_from_gregorian(&zh_epoch_date);
	printf("\nchinese_epoch: RD (%d) <-> Gregorian (%d, %d, %d)\n",
			zh_epoch, zh_epoch_date.year,
			zh_epoch_date.month, zh_epoch_date.day);

	struct chinese_date zh_date;
	printf("\nR.D.\t(C, Y, M, L, D)\t\tRD2\tEq?\n");
	for (size_t i = 0; i < nitems(rds); i++) {
		rd = rds[i];
		chinese_from_fixed(rd, &zh_date);
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
	date = (struct date){ tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday };
	rd = fixed_from_gregorian(&date);
	chinese_from_fixed(rd, &zh_date);
	printf("R.D.: %d\n", rd);
	printf("Gregorian: %4d-%02d-%02d\n", date.year, date.month, date.day);
	printf("Chinese: cycle=%2d, year=%2d, month=%2d%s, day=%2d\n",
			zh_date.cycle, zh_date.year, zh_date.month,
			zh_date.leap ? "(leap)" : "", zh_date.day);

	double t_day = (tm.tm_hour + tm.tm_min/60.0 + tm.tm_sec/3600.0) / 24.0;
	printf("Time: %02d:%02d:%02d\n", tm.tm_hour, tm.tm_min, tm.tm_sec);

	const struct location shanghai = {
		.latitude = angle2deg(31, 13, 43),
		.longitude = angle2deg(121, 28, 29),
		.elevation = 4.0,
		.zone = 8.0/24.0,
	};

	printf("\n-----------------------------------------------------------\n");
	show_chinese_calendar(rd);
	printf("\n...........................................................\n");
	show_sun_info(rd+t_day, &shanghai);
	printf("\n...........................................................\n");
	show_moon_info(rd+t_day, &shanghai);

	printf("\n-----------------------------------------------------------\n");
	date = (struct date){ 2033, 12, 25 };
	rd = fixed_from_gregorian(&date);
	show_chinese_calendar(rd);
	printf("\n...........................................................\n");
	show_sun_info(rd+t_day, &shanghai);
	printf("\n...........................................................\n");
	show_moon_info(rd+t_day, &shanghai);
}

static void
test2()
{
	int year1 = 2000, year2 = 2007;
	int rd;
	struct date date;

	printf("\n-----------------------------------------------------------\n");

	/* US Daylight Saving Start; Eq.(2.39) */
	printf("US Daylight Saving Start:\t");
	for (int y = year1; y <= year2; y++) {
		date.year = y;
		date.month = 3;
		date.day = 1;
		rd = nth_kday(2, SUNDAY, &date);
		gregorian_from_fixed(rd, &date);
		printf("%04d/%02d/%02d\t", date.year, date.month, date.day);
	}
	printf("\n");

	/* US Daylight Saving End; Eq.(2.40) */
	printf("US Daylight Saving End:\t");
	for (int y = year1; y <= year2; y++) {
		date.year = y;
		date.month = 11;
		date.day = 1;
		rd = nth_kday(1, SUNDAY, &date);
		gregorian_from_fixed(rd, &date);
		printf("%04d/%02d/%02d\t", date.year, date.month, date.day);
	}
	printf("\n");

	/* US Election Day; Eq.(2.38) */
	printf("US Election Day:\t");
	for (int y = year1; y <= year2; y++) {
		date.year = y;
		date.month = 11;
		date.day = 2;
		rd = nth_kday(1, TUESDAY, &date);
		gregorian_from_fixed(rd, &date);
		printf("%04d/%02d/%02d\t", date.year, date.month, date.day);
	}
	printf("\n");

	/* US Memorial Day; Eq.(2.37) */
	printf("US Memorial Day:\t");
	for (int y = year1; y <= year2; y++) {
		date.year = y;
		date.month = 5;
		date.day = 31;
		rd = nth_kday(-1, MONDAY, &date);
		gregorian_from_fixed(rd, &date);
		printf("%04d/%02d/%02d\t", date.year, date.month, date.day);
	}
	printf("\n");

	/* US Memorial Day; Eq.(2.37) */
	printf("US Memorial Day (2):\t");
	for (int y = year1; y <= year2; y++) {
		date.year = y;
		date.month = 6;
		date.day = 0;
		rd = nth_kday(-1, MONDAY, &date);
		gregorian_from_fixed(rd, &date);
		printf("%04d/%02d/%02d\t", date.year, date.month, date.day);
	}
	printf("\n");
}

static void
test3()
{
	struct date date = { 2020, 0, 0 };
	struct date date2;
	int rd, rd2;

	printf("\n-----------------------------------------------------------\n");

	printf("Input(Y,M,D)\tR.D.\tOutput(Y,M,D)\tR.D.2\tEq?\n");
	for (int m = 0; m < 20; m++) {
		date.month = m;
		rd = fixed_from_gregorian(&date);
		gregorian_from_fixed(rd, &date2);
		rd2 = fixed_from_gregorian(&date2);
		printf("(%4d, %2d, %2d)\t%7d\t(%4d, %2d, %2d)\t%7d\t%d\n",
			date.year, date.month, date.day, rd,
			date2.year, date2.month, date2.day, rd2,
			rd==rd2);
	}
}


/* Return the seconds east of UTC */
static int
get_utcoffset(void)
{
	time_t t;
	struct tm tm;

	time(&t);
	localtime_r(&t, &tm);
	return tm.tm_gmtoff;
}

static void
usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-L location] [-T] [-U timezone]\n", progname);
	exit(2);
}

int
main(int argc, char *argv[])
{
	int ch;
	int utcoffset = get_utcoffset();
	bool run_test = false;
	double latitude = 0.0;
	double longitude = 0.0;
	double elevation = 0.0;
	const char *progname = argv[0];

	while ((ch = getopt(argc, argv, "hL:TU:")) != -1) {
		switch (ch) {
		case 'L':
			if (!parse_location(optarg, &latitude, &longitude, &elevation))
				errx(1, "invalid location: '%s'", optarg);
			break;
		case 'T':
			run_test = true;
			break;
		case 'U':
			if (!parse_timezone(optarg, &utcoffset))
				errx(1, "invalid timezone: '%s'", optarg);
			break;
		case 'h':
		case '?':
		default:
			usage(progname);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc)
		usage(progname);

	printf("UTC offset: %d [seconds]\n", utcoffset);
	printf("Location: (latitude=%lf°, longitude=%lf°, elevation=%lfm)\n",
			latitude, longitude, elevation);

	if (run_test) {
		test1();
		test2();
		test3();
	}

	return 0;
}
