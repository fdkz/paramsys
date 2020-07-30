
#pragma once


#include <string.h> // memset
#include <stdio.h> // snprintf

#include "stdints.h"


// example:
//     #define GENLINE(txt) CONCAT(txt_, __LINE__)
//     line 13: GENLINE(int varname_); // results in line 13 being: int varname_13;
#define CONCAT_HELPER(x, y) x##y
#define CONCAT(x, y) CONCAT_HELPER(x, y)
// http://www.pixelbeat.org/programming/gcc/static_assert.html
// http://stackoverflow.com/questions/19401887/how-to-check-the-size-of-a-structure-at-compile-time
// usage:
//     DUMB_STATIC_ASSERT(sizeof(can_aerospace_packet) == 12);
//     // if you get here an error, like "the size of an array must be greater than zero",
//     // then it means the size of the structure is not what we wanted.
#define DUMB_STATIC_ASSERT(test) typedef char CONCAT(asserter_, __LINE__)[( !!(test) )*2-1 ]
#define ELEMENTS_IN_ARRAY(array) (sizeof(array) / sizeof((array)[0]))
#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)


DUMB_STATIC_ASSERT(PARAMS_VALUES_LEN_BYTES <= PARAMS_VALUES_CAPACITY_BYTES);


void g_generate_binary_string(char* dst, u8 dst_max_len, u64 val, u8 num_bits, u8 underscore_step=0) {
	if (!dst || dst_max_len == 0 || num_bits == 0) return;

	u32 num_underscores = underscore_step > 0 ? (num_bits - 1) / underscore_step : 0;
	u32 total_dst_chars = num_bits + num_underscores + 1; // with terminating zero

	if (total_dst_chars > dst_max_len)
		total_dst_chars = dst_max_len;

	char* cur = dst + total_dst_chars - 1; // move cursor to the end of the bitstring
	*cur-- = 0;
	u32 i = 0;

	while (cur >= dst) {
		*cur-- = val & 1 ? '1' : '0';
		val >>= 1;
		if (++i == underscore_step && cur >= dst) {
			*cur-- = '_';
			i = 0;
		}
	}
}


// Input : 16 bytes (uuid)
// Output: 36 bytes ascii (out_str), "123e4567-e89b-12d3-a456-426655440000", with following caveats:
//
// Return:  0 on success (out_str_len >= 36)
//            First 36 bytes are filled with the uuid, 37-th byte (if available) with the zero-termination.
//         -1 on error (fills out_str with minus signs)
int g_uuid_bin_to_str_canonical(u8* uuid, char* out_str, u8 out_str_len) {
	if (out_str_len < 36) {
		memset(out_str, '-', out_str_len);
		return -1;
	}
	// "123e4567-e89b-12d3-a456-426655440000"
	u8* buf = uuid;
	u8 chunks[5] = { 4, 2, 2, 2, 6 };
	u8 i = 0;
	u8 k = 0;
	while (k < sizeof(chunks)) {
		if (k > 0)
			*out_str++ = '-';
		u8 j = 0;
		while (j++ < chunks[k]) {
			const char* hexes = "0123456789abcdef";
			u8 b = *buf++;
			*out_str++ = hexes[b >> 4];
			*out_str++ = hexes[b & 0xf];
		}
		k++;
	}
	if (out_str_len > 36)
		*out_str = 0;
	return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// time
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct g_time_struct_t {
	u16 year;
	u8  month; // [1, 12]
	u8  mday;  // [1, 31]
	u8  hour;
	u8  minute;
	i32 usec;  // 0..1e6-1
};

// Wikipedia:
//   When a leap second occurs, so that the UTC day is not exactly 86,400 seconds long, a discontinuity occurs in the Unix time number.
//   Observe that when a positive leap second occurs (i.e., when a leap second is inserted) the Unix time numbers repeat themselves.
//
// Original code from: https://gmbabar.wordpress.com/2010/12/01/mktime-slow-use-custom-function/
//
// month and day start from 1.
// usec can be negative. But make sure the wanted time is after the 1970 epoch. And i32 can represent
//     only 0xffffffff / 1000000 / 2 = +/-2147 seconds.
//
// return microseconds
// return 0 if year is < 1970 or month is > 12.
i64 g_time_to_timestamp_us(u16 year, u8 month, u8 day, u8 hour, u8 minute, i32 usec)
{
	// cumulative days per month, using values {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
	const int cumulative_month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

	if (year < 1970 || month > 12) return 0;

	uint32_t tyears = year - 1970;
	uint32_t leaps = (tyears + 2) / 4;
	if ((tyears+2) % 4 == 0 && month <= 2) leaps--;
	// uncomment these next two lines after year 2100?
	//uint32_t i = (year - 100) / 100;
	//leaps -= ( (i/4)*3 + i%4 );
	uint32_t tdays = cumulative_month_days[month-1] + (day - 1) + (tyears * 365) + leaps;

	return ((int64_t)tdays * 86400 + (int64_t)hour * 3600 + (int64_t)minute * 60) * 1000000 + usec;

	// some more random info:
	//http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_15
	//tm_sec + tm_min*60 + tm_hour*3600 + tm_yday*86400 +
	//    (tm_year-70)*31536000 + ((tm_year-69)/4)*86400 -
	//    ((tm_year-1)/100)*86400 + ((tm_year+299)/400)*86400
}

// Move epoch from 01.01.1970 to 01.03.0000 (yes, Year 0) - this is the first
// day of a 400-year long "era", right after additional day of leap year.
// This adjustment is required only for date calculation, so instead of
// modifying time_t value (which would require 64-bit operations to work
// correctly) it's enough to adjust the calculated number of days since epoch.
#define EPOCH_ADJUSTMENT_DAYS 719468L

#define ADJUSTED_EPOCH_YEAR   0               // year to which the adjustment was made
#define ADJUSTED_EPOCH_WDAY   3               // 1st March of year 0 is Wednesday
#define DAYS_PER_ERA          146097L         // there are 97 leap years in 400-year periods. ((400 - 97) * 365 + 97 * 366)
#define DAYS_PER_CENTURY      36524L          // there are 24 leap years in 100-year periods. ((100 - 24) * 365 + 24 * 366)
#define DAYS_PER_4_YEARS      (3 * 365 + 366) // there is one leap year every 4 years
#define DAYS_PER_YEAR         365             // number of days in a non-leap year
#define DAYS_IN_JANUARY       31              // number of days in January
#define DAYS_IN_FEBRUARY      28              // number of days in non-leap February
#define YEARS_PER_ERA         400             // number of years per era

#define USECSPERMIN           (60L * 1000000L)
#define MINSPERHOUR           60L
#define HOURSPERDAY           24L
#define USECSPERHOUR          ((int64_t)USECSPERMIN * MINSPERHOUR)
#define USECSPERDAY           ((int64_t)USECSPERHOUR * HOURSPERDAY)
#define DAYSPERWEEK           7
#define MONSPERYEAR           12

#define YEAR_BASE                      0
#define EPOCH_YEAR                     1970
#define EPOCH_WDAY                     4
#define EPOCH_YEARS_SINCE_LEAP         2
#define EPOCH_YEARS_SINCE_CENTURY      70
#define EPOCH_YEARS_SINCE_LEAP_CENTURY 370

//#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

/*
 gmtime_r.c
 Original Author: Adapted from tzcode maintained by Arthur David Olson.
 Modifications:
 - Changed to mktm_r and added __tzcalc_limits - 04/10/02, Jeff Johnston
 - Fixed bug in mday computations - 08/12/04, Alex Mogilnikov <alx@intellectronika.ru>
 - Fixed bug in __tzcalc_limits - 08/12/04, Alex Mogilnikov <alx@intellectronika.ru>
 - Move code from _mktm_r() to gmtime_r() - 05/09/14, Freddie Chopin <freddie_chopin@op.pl>
 - Fixed bug in calculations for dates after year 2069 or before year 1901. Ideas for
   solution taken from musl's __secs_to_tm() - 07/12/2014, Freddie Chopin <freddie_chopin@op.pl>
 - Use faster algorithm from civil_from_days() by Howard Hinnant - 12/06/2014, Freddie Chopin <freddie_chopin@op.pl>
..
 - Add microsecond precision, un-standardize, and break some important things for sure.. 2019-06-12 Elmo Trolla.
*/

// should work after year 2038
void g_timestamp_us_to_time(int64_t timestamp_us, g_time_struct_t* res)
{
	int32_t days;
	int64_t rem;
	int32_t era, year;
	uint32_t erayear, yearday, month, day;
	uint32_t eraday;

	//uint64_t timestamp_s = timestamp_us / 1000000; // can't use uint32_t here because of we need it to work after year 2038

	days = timestamp_us / USECSPERDAY + EPOCH_ADJUSTMENT_DAYS;
	rem  = timestamp_us % USECSPERDAY;
	if (rem < 0) {
		rem += USECSPERDAY;
		--days;
	}

	// Compute hour, min, and sec
	res->hour   = rem / USECSPERHOUR;
	rem        %= USECSPERHOUR;
	res->minute = rem / USECSPERMIN;
	res->usec   = rem % USECSPERMIN;

	// Compute year, month, day & day of year. For description of this algorithm see
	// http://howardhinnant.github.io/date_algorithms.html#civil_from_days
	era     = (days >= 0 ? days : days - (DAYS_PER_ERA - 1)) / DAYS_PER_ERA;
	eraday  = days - era * DAYS_PER_ERA; // [0, 146096]
	erayear = (eraday - eraday / (DAYS_PER_4_YEARS - 1) + eraday / DAYS_PER_CENTURY - eraday / (DAYS_PER_ERA - 1)) / 365; // [0, 399]
	yearday = eraday - (DAYS_PER_YEAR * erayear + erayear / 4 - erayear / 100); // [0, 365]
	month   = (5 * yearday + 2) / 153; // [0, 11]
	day     = yearday - (153 * month + 2) / 5 + 1; // [1, 31]
	month  += month < 10 ? 2 : -10;
	year    = ADJUSTED_EPOCH_YEAR + erayear + era * YEARS_PER_ERA + (month <= 1);

	res->year  = year - YEAR_BASE;
	res->month = month + 1;
	res->mday  = day;
}

// "2019-03-08T22:23:15Z"
// Make sure out_str is at least 21 characters long (this includes the zero-termination).
void g_timestamp_us_to_iso8601(i64 timestamp_us, char* out_str, i32 out_str_max_len) {
	if (out_str_max_len < 21) { out_str[0] = 0; return; }
	g_time_struct_t t;
	g_timestamp_us_to_time(timestamp_us, &t);
	snprintf(out_str, out_str_max_len, "%04i-%02i-%02iT%02i:%02i:%02iZ", t.year, t.month, t.mday, t.hour, t.minute, t.usec/1000000);
}
