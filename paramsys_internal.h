// Elmo Trolla, 2020-07-21
// Licence: pick one - public domain / UNLICENCE (https://www.unlicense.org) / MIT (https://opensource.org/licenses/MIT).

#pragma once

#include "stdints.h"


#define PARAMS_TYPE_IS_VARIABLE_SIZE_bit ((u8)0b10000000)
#define PARAMS_TYPE_INDEX_mask           ((u8)0b01111111)
#define PARAMS_NO_INDEX            0xffff

//#define PARAMS_TYPE_len_is_first4bits_bit 0b01000000
//#define PARAMS_HAS_MINMAXDEFAULT  ((u8)0b10000000) // if bit not set, then param still has the default
// future: #define PARAMS_HAS_DEFAULT

#pragma pack(push,1)

// Every parameter is described by one of these. There's a simple array of these structs that contains every parameter.
// 24 bytes per param.
struct param_info_t {
	enum flags_e : u8 {
		DISABLED      = 1, // implies NO_DEFAULT
		NO_DEFAULT    = 2,
		HAS_MINMAX    = 4, // has min and max in addition to the default value
		VALUE_CHANGED = 128
	};
	char name[16];       // zero-terminated! so 15 useful characters.
	u8   type;           //
	u8   component;
	u8   security_level; // who can change or see the param.
	u16  defaults_index; // index to the corresponding defaults_8/defaults_16/.. or default_minmax_* array
	u16  value_index;    // pointer to param value. if type is string, then first byte is string length.
	u8   flags;          //
};

/*

defaults_str layout:
    in order to support string length changes after firmware updates (this is more important than being able to change
    parameter types), we need to keep the string max len value also in eeprom. so, the simplest strings are in memory
    like this:
        defaults: max_len, len, char1, char2, .. len, char1, char2, char3, ..
    in the future, when we need longer than 255 byte strings and more than 65k of total length, we need to add
    a different string type. str16 maybe?


    you can't remove string params. but you can rename them and make them shorter.
*/

struct defminmax_u8_t  { u8  default_val; u8  min; u8  max; };
struct defminmax_u16_t { u16 default_val; u16 min; u16 max; };
struct defminmax_u32_t { u32 default_val; u32 min; u32 max; };
struct defminmax_u64_t { u64 default_val; u64 min; u64 max; };
struct defminmax_i8_t  { i8  default_val; i8  min; i8  max; };
struct defminmax_i16_t { i16 default_val; i16 min; i16 max; };
struct defminmax_i32_t { i32 default_val; i32 min; i32 max; };
struct defminmax_i64_t { i64 default_val; i64 min; i64 max; };
struct defminmax_f32_t { f32 default_val; f32 min; f32 max; };
struct defminmax_f64_t { f64 default_val; f64 min; f64 max; };

//struct default_str_t { u8 max_len; u16 start_index; }; // max_len is without the length byte.

#pragma pack(pop)
