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
 */

#ifndef CHINESE_H_
#define CHINESE_H_

#include <stdbool.h>

struct chinese_date {
	int	cycle;
	int	year;	/* year in the $cycle: [1, 60] */
	int	month;	/* [1, 12] */
	bool	leap;	/* whether $month is a leap month */
	int	day;
};

enum major_solar_term {
	YUSHUI = 1,	/* 雨水; 330° (solar longitude) */
	CHUNFEN,	/* 春分; 0°; Spring Equinox */
	GUYU,		/* 谷雨; 30° */
	XIAOMAN,	/* 小满; 60° */
	XIAZHI,		/* 夏至; 90°; Summer Solstice */
	DASHU,		/* 大暑; 120° */
	CHUSHU,		/* 处暑; 150° */
	QIUFEN,		/* 秋分; 180°; Autumnal Equinox */
	SHUANGJIANG,	/* 霜降; 210° */
	XIAOXUE,	/* 小雪; 240° */
	DONGZHI,	/* 冬至; 270°; Winter Solstice */
	DAHAN,		/* 大寒; 300° */
};

enum minor_solar_term {
	LICHUN = 1,	/* 立春; 315° (solar longitude) */
	JINGZHE,	/* 惊蛰; 345° */
	QINGMING,	/* 清明; 15° */
	LIXIA,		/* 立夏; 45° */
	MANGZHONG,	/* 芒种; 75° */
	XIAOSHU,	/* 小暑; 105° */
	LIQIU,		/* 立秋; 135° */
	BAILU,		/* 白露; 165° */
	HANLU,		/* 寒露; 195° */
	LIDONG,		/* 立冬; 225° */
	DAXUE,		/* 大雪; 255° */
	XIAOHAN,	/* 小寒; 285° */
};

int	chinese_new_year(int year);

struct chinese_date	chinese_from_fixed(int rd);
int	fixed_from_chinese(struct chinese_date *date);

#endif
