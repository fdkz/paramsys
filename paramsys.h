// Elmo Trolla, 2020-07-21
// Licence: pick one - public domain / UNLICENCE (https://www.unlicense.org) / MIT (https://opensource.org/licenses/MIT).


// features:
//   can remove params from the end, never from the middle
//   can disable (hide) params from the middle
//   can change param names and default values. but not type and defaults type.

#pragma once

#include "stdints.h"

#include "paramsys_generated.h"



#define PARAMS_VALUES_CAPACITY_BYTES (65536*2) // feel free to change this. beware that changing this will reset all the params to default values.

// bits 0..2: param length in bytes, but given in left-shifts of value 1. valid if bit 3 is set.
//   001 - 1 byte
//   010 - 2 bytes
//   011 - 4 bytes
//   100 - 8 bytes
//   101 - 16 bytes
//   110 - 32 bytes
//   111 - 64 bytes
// bit 3 : param length bits are valid. if not set, then the param is of a variable length type, like str.
// bits 4..7 : 1 - type
//   0000 - uint
//   0001 - int
//   0010 - float
//   0011 - flags
//   0100 - str
//   0101 - buf?
//   0110 - unix time microseconds. u64?
//   0111 - atomic time microseconds. u64?
//   1000 - uuid128. u128?
// //   1110 - str8?
// //   1111 - str16?
//enum class params_type_e : u8 {
//	U8      = 0b00001001,
//	U16     = 0b00001010,
//	U32     = 0b00001011,
//	U64     = 0b00001100,
//	I8      = 0b00011001,
//	I16     = 0b00011010,
//	I32     = 0b00011011,
//	I64     = 0b00011100,
//	F32     = 0b00101011,
//	F64     = 0b00101100,
//	FLAGS8  = 0b00111001,
//	FLAGS16 = 0b00111010,
//	FLAGS32 = 0b00111011,
//	STR     = 0b01000000,
//	//_INTERNAL_..
//	//RESERVED  0bx11xxxxx
//	//PARAMS_HAS_MINMAXDEFAULT  0b10000000 // if bit not set, then param still has the default
//};

// Last bit is set if param is of variable-size type.
// NB! these have to start from 0 and values MUST be consecutive (except for the variable-size type bit).
// NB! this and paramsys_type_table in paramsys implementation have to be in sync!
enum class params_type_e : u8 {
	U8      = 0,
	U16     = 1,
	U32     = 2,
	U64     = 3,
	I8      = 4,
	I16     = 5,
	I32     = 6,
	I64     = 7,
	F32     = 8,
	F64     = 9,
	FLAGS8  = 10,
	FLAGS16 = 11,
	FLAGS32 = 12,
	UUID128 = 13,
	TIME_UNIX_US64   = 14, // microseconds, signed 64-bit integer
	TIME_ATOMIC_US64 = 15,
	// all variable-size types have to be after this line
	STR              = 16 | 0b10000000,
	LAST    = 17 // has to be last type num + 1
};

enum class param_error_t : u8 {
	SUCCESS = 0,
	FAIL = 1,
	NO_PARAM = 2, // param_index out of bounds, or wrong param_type
};


#pragma pack(push,1)
// This struct is meant for the public API, giving information about the parameter to the user.
struct param_info_public_t {
	u8            component;
	params_type_e type;
	u8            security_level;
	const char*   name; // zero-terminated string
	bool          has_minmax; // default_val is there for every parameter. but min and max are garbage if this is false.
	union {
		// use one according to the type variable. ignore min and max if not has_minmax.
		struct { u8  default_val, min, max; } param_u8;
		struct { u16 default_val, min, max; } param_u16;
		struct { u32 default_val, min, max; } param_u32;
		struct { u64 default_val, min, max; } param_u64;
		struct { i8  default_val, min, max; } param_i8;
		struct { i16 default_val, min, max; } param_i16;
		struct { i32 default_val, min, max; } param_i32;
		struct { i64 default_val, min, max; } param_i64;
		struct { f32 default_val, min, max; } param_f32;
		struct { f64 default_val, min, max; } param_f64;
		struct { u8  default_val[16]; }       param_uuid128;
		struct { i64 default_val; }           param_time_us64;
		//struct param_buf { u16 len; u8* ptr; };
		struct { u8 max_len; u8 len; u8* ptr; } param_str; // NOT zero-terminated
	};
};
#pragma pack(pop)


void          params_init();
param_error_t params_get_info(u16 param_index, param_info_public_t* out_param_info);
void          params_print_all();

// we could do without param_type here, but it really helps to prevent bugs and serves as forced documentation when using this function.
param_error_t params_get(u16 param_index, params_type_e param_type, void* out_value);
param_error_t params_set(u16 param_index, params_type_e param_type, void* valueptr); // applies min/max if necessary
param_error_t params_get_str(u16 param_index, const char** out_str, u8* out_str_len);
param_error_t params_set_str(u16 param_index, const char* str, u8 str_len);
//param_error_t params_save(u16 param_index);

// convenience functions

// these will limit the value to min/max if the param has min/max set.
inline param_error_t params_set_i8 (u16 param_index, i8  value) { return params_set(param_index, params_type_e::I8,  &value); }
inline param_error_t params_set_i16(u16 param_index, i16 value) { return params_set(param_index, params_type_e::I16, &value); }
inline param_error_t params_set_i32(u16 param_index, i32 value) { return params_set(param_index, params_type_e::I32, &value); }
inline param_error_t params_set_i64(u16 param_index, i64 value) { return params_set(param_index, params_type_e::I64, &value); }
inline param_error_t params_set_u8 (u16 param_index, u8  value) { return params_set(param_index, params_type_e::U8,  &value); }
inline param_error_t params_set_u16(u16 param_index, u16 value) { return params_set(param_index, params_type_e::U16, &value); }
inline param_error_t params_set_u32(u16 param_index, u32 value) { return params_set(param_index, params_type_e::U32, &value); }
inline param_error_t params_set_u64(u16 param_index, u64 value) { return params_set(param_index, params_type_e::U64, &value); }
inline param_error_t params_set_f32(u16 param_index, f32 value) { return params_set(param_index, params_type_e::F32, &value); }
inline param_error_t params_set_f64(u16 param_index, f64 value) { return params_set(param_index, params_type_e::F64, &value); }

// these return zero if param not found
inline f32           params_get_f32(u16 param_index) { f32 v; return params_get(param_index, params_type_e::F32, &v) == param_error_t::SUCCESS ? v : 0.f; }
inline f64           params_get_f64(u16 param_index) { f64 v; return params_get(param_index, params_type_e::F64, &v) == param_error_t::SUCCESS ? v : 0.; }
inline i8            params_get_i8 (u16 param_index) { i8  v; return params_get(param_index, params_type_e::I8,  &v) == param_error_t::SUCCESS ? v : 0; }
inline i16           params_get_i16(u16 param_index) { i16 v; return params_get(param_index, params_type_e::I16, &v) == param_error_t::SUCCESS ? v : 0; }
inline i32           params_get_i32(u16 param_index) { i32 v; return params_get(param_index, params_type_e::I32, &v) == param_error_t::SUCCESS ? v : 0; }
inline i64           params_get_i64(u16 param_index) { i64 v; return params_get(param_index, params_type_e::I64, &v) == param_error_t::SUCCESS ? v : 0; }
inline u8            params_get_u8 (u16 param_index) { u8  v; return params_get(param_index, params_type_e::U8,  &v) == param_error_t::SUCCESS ? v : 0; }
inline u16           params_get_u16(u16 param_index) { u16 v; return params_get(param_index, params_type_e::U16, &v) == param_error_t::SUCCESS ? v : 0; }
inline u32           params_get_u32(u16 param_index) { u32 v; return params_get(param_index, params_type_e::U32, &v) == param_error_t::SUCCESS ? v : 0; }
inline u64           params_get_u64(u16 param_index) { u64 v; return params_get(param_index, params_type_e::U64, &v) == param_error_t::SUCCESS ? v : 0; }

