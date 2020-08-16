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

#include <stddef.h>

#include "days.h"
#include "parsedata.h"

#define SPECIALDAY_INIT0 \
	{ NULL, 0, NULL, 0, 0 }
#define SPECIALDAY_INIT(name, flag) \
	{ name, sizeof(name)-1, NULL, 0, (flag) }
struct specialday specialdays[] = {
	SPECIALDAY_INIT("Easter", F_EASTER),
	SPECIALDAY_INIT("Paskha", F_PASKHA),
	SPECIALDAY_INIT("ChineseNewYear", F_CNY),
	SPECIALDAY_INIT("NewMoon", F_NEWMOON),
	SPECIALDAY_INIT("FullMoon", F_FULLMOON),
	SPECIALDAY_INIT("MarEquinox", F_MAREQUINOX),
	SPECIALDAY_INIT("SepEquinox", F_SEPEQUINOX),
	SPECIALDAY_INIT("JunSolstice", F_JUNSOLSTICE),
	SPECIALDAY_INIT("DecSolstice", F_DECSOLSTICE),
	SPECIALDAY_INIT0,
};
