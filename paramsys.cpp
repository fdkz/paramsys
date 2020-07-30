// Elmo Trolla, 2020-07-21
// Licence: pick one - public domain / UNLICENCE (https://www.unlicense.org) / MIT (https://opensource.org/licenses/MIT).

//  * when values are added to any type (for example 5 * u16), the init code will make room by copying the values forward (shift everything starting from count_32 by 5*2=10 bytes).
//  * TODO: a flag that shows whether we should copy the param to eeprom on every update, or change it only in RAM.
//  * you can't remove params or change param types.
//  * you can change/add/remove limits (every param value is re-validated on every bootup), defaults and param names.
//  * if you'd want to change params randomly, then eeprom should contain much more than just the current value for every parameter.
//    it would need information about the parameter type, where to find default values and limits, and also contain the parameter name.
// what happens when appending parameters?
// can't remove parameters, but appending?
//   * can't append. have to reorganize the memory layout. but would work if every type had an independent memory segment. doable on esp32.
//     but can't do on tiva. have to load previous sections to RAM and write out to new places.
//   * hmmm. maybe first params would have to be internal, address params for all the types?

#include "paramsys.h"

#include <stddef.h> // offsetof
#include <string.h> // memcpy
#include <assert.h> // assert
#include <stdio.h> // printf
#include <inttypes.h> // PRIu64, ..

#include "paramsys_impl_generated.h"

#include "helpers.h"


#pragma pack(push,1)

// little-endian note: least-significant bytes are on lower memory addresses.
union conv_t {
	u8  u8_0;
	u16 u16_0;
	u32 u32_0;
	u64 u64_0;
	i8  i8_0;
	i16 i16_0;
	i32 i32_0;
	i64 i64_0;
	f32 f32_0;
	f64 f64_0;

	u8  u8v[16];
	u16 u16v[8];
	u32 u32v[4];
	u64 u64v[2];
	i8  i8v[16];
	i16 i16v[8];
	i32 i32v[4];
	i64 i64v[2];
	f32 f32v[4];
	f64 f64v[2];

	u8  u128v[16];
	u8  padding[16*3];
};


// Memory layout in EEPROM.
// But there's always a RAM mirror of the whole parameters struct.
struct paramsys_valuemem_t {
	u8 component;
	u8 packet_type;
	u8 packet_version;
	//u32 some_magic_code..
	u8 reserved1;
	u8 reserved2;
	u8 reserved3;
	u32 values_bytes_capacity;
	u32 values_bytes_used;
	u16 count_8;  // num of values by type length in "values" array. i8, u8, flags8.
	u16 count_16; // i16, u16, flags16. address: values + count_8 * sizeof(i8)
	u16 count_32; // i32, u32, flags32. address: values + count_8 * sizeof(i8) + count_16 * sizeof(i16)
	u16 count_64; // ..
	u16 count_128; // ..
	u16 count_str; // TODO: need this? maybe.
	u32 len_str;


	// this has to be the last entry!
	// also, this HAS to be aligned at 4 bytes in relation to the struct start.
	u8  values[PARAMS_VALUES_CAPACITY_BYTES];

	// strings: [maxlen, len, ...], [maxlen, len, ...] // maxlen here is necessary if we want to support resizing the string params.

	// layout of the following arrays is only correct if magic_code matches. if does not match, then the init code
	// should fix the layout.

	// I'd very much like to do something like this, instead of the one "values" array and offsetof_* functions, but
	// c/c++ doesn't allow 0-sized arrays (we don't want to pay the max 14-byte penalty
//	u8  values_8[PARAMS_COUNT_8];
//	u16 values_16[PARAMS_COUNT_16];
//	u32 values_32[PARAMS_COUNT_32]; // i32, u32, f32
//	u64 values_64[PARAMS_COUNT_64];
//	u8  values_str[PARAMS_LEN_STR];

	// offsets from start of the struct
	inline int offsetof_8()   { return offsetof(paramsys_valuemem_t, values); }
	inline int offsetof_16()  { int end = offsetof_8() + count_8; return end + (end & 1); } // aligned by 2 bytes
	inline int offsetof_32()  { int end = offsetof_16() + count_16 * 2; return end + (end & 2); } // aligned by 4 bytes
	inline int offsetof_64()  { return offsetof_32() + count_32 * 4; } // aligned by 4 bytes
	inline int offsetof_128() { return offsetof_64() + count_64 * 8; } // aligned by 4 bytes
	inline int offsetof_str() { return offsetof_128() + count_128 * 16; } // aligned by 4 bytes

	// size in bytes, including the padding bytes between arrays of the different types. pad everything to 4 bytes,
	// and assume address of the paramsys_valuemem struct is already aligned.
	//int calc_values_size() { return offsetof_str() + str_len - offsetof_8(); }
};
#pragma pack(pop)


enum { COMPONENT_PARAMS = 0xFD, };
enum { P_PARAMS_VALUEMEM = 0x06, };

paramsys_valuemem_t params_values = {
	COMPONENT_PARAMS,  // 0xFD
	P_PARAMS_VALUEMEM, // 0x06
	1,
	0,
	0,
	0,
	PARAMS_VALUES_CAPACITY_BYTES,
	PARAMS_VALUES_LEN_BYTES,
	PARAMS_COUNT_8,
	PARAMS_COUNT_16,
	PARAMS_COUNT_32,
	PARAMS_COUNT_64,
	PARAMS_COUNT_128,
	PARAMS_COUNT_STR,
	PARAMS_VALUES_STR_BYTES,
	//.values = {},
};

// These are indirection to the params_values arrays by type. We have to use this indirection because c/c++ doesn't
// allow zero-size arrays. Longer explanation in paramsys_impl_generated.h.
u8*     params_values_8   = params_values.values;
u16*    params_values_16  = (u16*)((u8*)&params_values + params_values.offsetof_16());
u32*    params_values_32  = (u32*)((u8*)&params_values + params_values.offsetof_32());
u64*    params_values_64  = (u64*)((u8*)&params_values + params_values.offsetof_64());
u8*     params_values_128 = (u8*)((u8*)&params_values + params_values.offsetof_128());
u8*     params_values_str = (u8*)((u8*)&params_values + params_values.offsetof_str());


// TODO: additional indirection. still encode length in param type.
struct {
	const char* type_name;
	u8    type_len;
	void* values;
	void* defaults;
	void* defminmax;
} paramsys_type_table[17] = {
	{"u8",      1,  params_values_8,   defaults_8,   defminmax_8 },
	{"u16",     2,  params_values_16,  defaults_16,  defminmax_16},
	{"u32",     4,  params_values_32,  defaults_32,  defminmax_32},
	{"u64",     8,  params_values_64,  defaults_64,  defminmax_64},
	{"i8",      1,  params_values_8,   defaults_8,   defminmax_8 },
	{"i16",     2,  params_values_16,  defaults_16,  defminmax_16},
	{"i32",     4,  params_values_32,  defaults_32,  defminmax_32},
	{"i64",     8,  params_values_64,  defaults_64,  defminmax_64},
	{"f32",     4,  params_values_32,  defaults_32,  defminmax_32},
	{"f64",     8,  params_values_64,  defaults_64,  defminmax_64},
	{"flags8",  1,  params_values_8,   defaults_8,   defminmax_8 },
	{"flags16", 2,  params_values_16,  defaults_16,  defminmax_16},
	{"flags32", 4,  params_values_32,  defaults_32,  defminmax_32},
	{"uuid128", 16, params_values_128, defaults_128, nullptr},
	{"time_unix_us64",  8, params_values_64,  defaults_64,  defminmax_64},
	{"time_atomic_u64", 8, params_values_64,  defaults_64,  defminmax_64},
	{"str",             0, nullptr, nullptr, nullptr}, // here only for the type name
};

template <typename T>
static T l_clamp(T v, T lo, T hi) {
	if (lo > hi) { T t = lo; lo = hi; hi = t; }
	if (v > hi) return hi;
	if (v < lo) return lo;
	return v;
	// could use this, but for our case, hi can be lower than lo.
	//v = v > lo ? v : lo;
	//return v < hi ? v : hi;
}

inline const char* l_param_type_to_str(params_type_e param_type);
inline u32         l_param_len_bytes(param_info_t* param_info);
inline bool        l_param_is_variable_size(param_info_t* param_info);
inline bool        l_param_has_no_default(param_info_t* param_info);
inline void*       l_param_get_default_ptr(param_info_t* param_info);
inline void*       l_param_get_value_ptr(param_info_t* param_info);
inline u8*         l_param_get_default_str_ptr(param_info_t* param_info);
inline u8*         l_param_get_value_str_ptr(param_info_t* param_info);
void               l_params_set_str(param_info_t* param_info, const char* str, u8 str_len);
param_error_t      l_params_copy_from_value(param_info_t* param_info, void* out_default);
param_error_t      l_params_copy_to_value(param_info_t* param_info, void* in_value);
param_error_t      l_params_copy_default(param_info_t* param_info, void* out_default);
param_error_t      l_params_copy_defminmax_or_default(param_info_t* param_info, void* out_default);
void               l_params_copy_from_defaults_str(param_info_t* param_info, u8* out_str);
void               l_params_print_all(params_table_t* params_info);


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// public interface
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//void params_init(params_table_t* params_info) {
void params_init() {

	// copy values from eeprom to ram. (TODO:)
	// or, if first use, copy defaults to ram.

	for (int i = 0; i < ELEMENTS_IN_ARRAY(params_info.params_info); i++) {

		param_info_t* param_info = &params_info.params_info[i];

		if (!l_param_is_variable_size(param_info)) {

			conv_t* cval = (conv_t*)l_param_get_value_ptr(param_info);
			conv_t* cdef = (conv_t*)l_param_get_default_ptr(param_info);

			param_error_t e = l_params_copy_default(param_info, l_param_get_value_ptr(param_info));
			assert(e == param_error_t::SUCCESS);

		} else {
			switch (param_info->type) {
			case (u8)params_type_e::STR: {
				u8* ptr = (u8*)l_param_get_value_str_ptr(param_info);
				assert(ptr);
				l_params_copy_from_defaults_str(param_info, ptr);
				break;
			}
			default:
				assert(false);
			}
		}
	}
}

// return info about the param, including defaults and limits if present. does not return current value of the param.
param_error_t params_get_info(u16 param_index, param_info_public_t* out_param_info) {
	if (param_index > PARAMS_COUNT)
		return param_error_t::NO_PARAM;
	param_info_t* param_inf = &params_info.params_info[param_index];

	out_param_info->component      = param_inf->component;
	out_param_info->type           = (params_type_e)param_inf->type;
	out_param_info->security_level = param_inf->security_level;
	out_param_info->name           = (const char*)param_inf->name;
	out_param_info->has_minmax     = param_inf->flags & param_info_t::HAS_MINMAX;

	if (!l_param_is_variable_size(param_inf)) {
		param_error_t e = l_params_copy_defminmax_or_default(param_inf, &out_param_info->param_u8);
		assert(e == param_error_t::SUCCESS);
	} else {
		switch (out_param_info->type) {
		case params_type_e::STR:
			out_param_info->param_str.max_len = params_info.defaults_str[param_inf->defaults_index];
			out_param_info->param_str.len = params_info.defaults_str[param_inf->defaults_index + 1];
			out_param_info->param_str.ptr = &params_info.defaults_str[param_inf->defaults_index + 2];
			break;
		default:
			return param_error_t::NO_PARAM;
		}
	}

	return param_error_t::SUCCESS;
}

void params_print_all() {
	l_params_print_all(&params_info);
}

param_error_t params_get(u16 param_index, params_type_e param_type, void* out_value) {
	if (param_index > PARAMS_COUNT)
		return param_error_t::NO_PARAM;
	param_info_t* param_info = &params_info.params_info[param_index];
	if (param_info->type != (u8)param_type)
		return param_error_t::NO_PARAM;

	param_error_t e = l_params_copy_from_value(param_info, out_value);
	return e;
}

param_error_t params_set(u16 param_index, params_type_e param_type, void* valueptr) {
	if (param_index > PARAMS_COUNT)
		return param_error_t::NO_PARAM;
	param_info_t *param_info = &params_info.params_info[param_index];
	if (param_info->type != (u8) param_type)
		return param_error_t::NO_PARAM;

	u32 value_len = l_param_len_bytes(param_info);
	conv_t val; // temporary. used when value has to be clamped.
	void* validated_value;

	// if param has_minmax:
	//     copy value to internal buf
	//     apply limits to the value in internal buf
	//     point validated_value to the resulting value
	// else:
	//     point validated_value to the wanted value given by the user
	//
	// if validated_value is different from the real RAM values* buf:
	//     set the param VALUE_CHANGED flag
	//
	// copy validated_value to RAM values* buf.

	bool has_minmax = param_info->flags & param_info_t::HAS_MINMAX;

	// If has_minmax, then we need to copy the wanted value to local buf in order to apply the minmax limits.
	// Otherwise we'd overwrite the value given us by the user in *valueptr, and that's not ok.
	if (has_minmax) {

		memcpy(&val, valueptr, value_len);
		void* default_ptr = l_param_get_default_ptr(param_info);

		switch (param_type) {
		case params_type_e::U8: {
			auto d = (defminmax_u8_t *) default_ptr;
			val.u8_0 = l_clamp(val.u8_0, d->min, d->max);
			break;
		}
		case params_type_e::U16: {
			auto d = (defminmax_u16_t *) default_ptr;
			val.u16_0 = l_clamp(val.u16_0, d->min, d->max);
			break;
		}
		case params_type_e::U32: {
			auto d = (defminmax_u32_t *) default_ptr;
			val.u32_0 = l_clamp(val.u32_0, d->min, d->max);
			break;
		}
		case params_type_e::U64: {
			auto d = (defminmax_u64_t *) default_ptr;
			val.u64_0 = l_clamp(val.u64_0, d->min, d->max);
			break;
		}
		case params_type_e::I8: {
			auto d = (defminmax_i8_t *) default_ptr;
			val.i8_0 = l_clamp(val.i8_0, d->min, d->max);
			break;
		}
		case params_type_e::I16: {
			auto d = (defminmax_i16_t *) default_ptr;
			val.i16_0 = l_clamp(val.i16_0, d->min, d->max);
			break;
		}
		case params_type_e::I32: {
			auto d = (defminmax_i32_t *) default_ptr;
			val.i32_0 = l_clamp(val.i32_0, d->min, d->max);
			break;
		}
		case params_type_e::I64: {
			auto d = (defminmax_i64_t *) default_ptr;
			val.i64_0 = l_clamp(val.i64_0, d->min, d->max);
			break;
		}
		case params_type_e::F32: {
			auto d = (defminmax_f32_t *) default_ptr;
			val.f32_0 = l_clamp(val.f32_0, d->min, d->max);
			break;
		}
		case params_type_e::F64: {
			auto d = (defminmax_f64_t *) default_ptr;
			val.f64_0 = l_clamp(val.f64_0, d->min, d->max);
			break;
		}
		default:
			assert(false);
			return param_error_t::NO_PARAM;
		}

		validated_value = &val;

	} else {
		validated_value = valueptr;
	}

	void* param_value_ptr = l_param_get_value_ptr(param_info);
	assert(param_value_ptr);

	// Check if current value and wanted value differ.
	if (memcmp(validated_value, param_value_ptr, value_len) != 0) {
		param_info->flags |= param_info_t::VALUE_CHANGED;
	}

	// copy wanted value to the current values array.
	memcpy(param_value_ptr, validated_value, value_len);

	return param_error_t::SUCCESS;
}

param_error_t params_get_str(u16 param_index, const char** out_str, u8* out_str_len) {
	if (param_index > PARAMS_COUNT)
		return param_error_t::NO_PARAM;
	param_info_t* param_info = &params_info.params_info[param_index];
	if (param_info->type != (u8)params_type_e::STR)
		return param_error_t::NO_PARAM;

	u8* src = l_param_get_value_str_ptr(param_info);
	*out_str_len = src[1];
	*out_str = (char*)src + 2;

	return param_error_t::SUCCESS;
}

param_error_t params_set_str(u16 param_index, const char* str, u8 str_len) {
	if (param_index > PARAMS_COUNT)
		return param_error_t::NO_PARAM;
	param_info_t* param_info = &params_info.params_info[param_index];
	if (param_info->type != (u8)params_type_e::STR)
		return param_error_t::NO_PARAM;

	l_params_set_str(param_info, str, str_len);
	return param_error_t::SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// private functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


inline const char* l_param_type_to_str(params_type_e param_type) {
	assert(((u8)param_type & PARAMS_TYPE_INDEX_mask) < (u8)params_type_e::LAST);
	return paramsys_type_table[(u8)param_type & PARAMS_TYPE_INDEX_mask].type_name;
}

// returns 0 for any variable-size types (like str)
inline u32 l_param_len_bytes(param_info_t* param_info) {
	return paramsys_type_table[(u8)param_info->type & PARAMS_TYPE_INDEX_mask].type_len;
}

inline bool l_param_is_variable_size(param_info_t* param_info) {
	return param_info->type & PARAMS_TYPE_IS_VARIABLE_SIZE_bit;
}

inline bool l_param_has_no_default(param_info_t* param_info) {
	return (param_info->flags & param_info_t::NO_DEFAULT) || (param_info->flags & param_info_t::DISABLED);
}

// Return pointer to the default value. Does no error-checking, so returns garbage if no default exists.
inline void* l_param_get_default_ptr(param_info_t* param_info) {
	if (param_info->flags & param_info_t::HAS_MINMAX) {
		return (u8*)paramsys_type_table[(u8)param_info->type & PARAMS_TYPE_INDEX_mask].defminmax +
			l_param_len_bytes(param_info) * param_info->defaults_index * 3;
	} else {
		return (u8*)paramsys_type_table[(u8)param_info->type & PARAMS_TYPE_INDEX_mask].defaults +
			l_param_len_bytes(param_info) * param_info->defaults_index;
	}
}

inline void* l_param_get_value_ptr(param_info_t* param_info) {
	return (u8*)paramsys_type_table[(u8) param_info->type & PARAMS_TYPE_INDEX_mask].values +
		l_param_len_bytes(param_info) * param_info->value_index;
}

// return nullptr if the str has no default
inline u8* l_param_get_default_str_ptr(param_info_t* param_info) {
	assert(l_param_is_variable_size(param_info));
	return &params_info.defaults_str[param_info->defaults_index];
}

// return nullptr if the str has no default
inline u8* l_param_get_value_str_ptr(param_info_t* param_info) {
	assert(l_param_is_variable_size(param_info));
	return &params_values_str[param_info->value_index];
}

// TODO: rename str to buf? str should always have a terminating zero?
// str_len is without terminating zero.
void l_params_set_str(param_info_t* param_info, const char* str, u8 str_len) {
	u8* dst = l_param_get_value_str_ptr(param_info);
	u8 max_len = dst[0];
	assert(params_info.defaults_str[param_info->defaults_index] == max_len);
	if (str_len > max_len) str_len = max_len;
	dst[1] = str_len;
	memcpy(dst+2, str, str_len);

	// TODO: check if changed and set the changed flag.
}

// Copy param value from internal RAM param values buf to out_value. Works only for fixed-size types.
param_error_t l_params_copy_from_value(param_info_t* param_info, void* out_default) {
	assert(param_info);
	assert(out_default);

	if (!param_info || !out_default || l_param_is_variable_size(param_info))
		return param_error_t::FAIL;

	void* ptr = l_param_get_value_ptr(param_info);
	assert(ptr);
	memcpy(out_default, ptr, l_param_len_bytes(param_info));

	return param_error_t::SUCCESS;
}

// Copy param value from given ptr to the internal RAM param values buf. Works only for fixed-size types.
param_error_t l_params_copy_to_value(param_info_t* param_info, void* in_value) {
	assert(param_info);
	assert(in_value);

	if (!param_info || !in_value || l_param_is_variable_size(param_info))
		return param_error_t::FAIL;

	void* ptr = l_param_get_value_ptr(param_info);
	assert(ptr);
	memcpy(ptr, in_value, l_param_len_bytes(param_info));

	return param_error_t::SUCCESS;
}

// Copy default value for fixed-size types.
// If param HAS_MINMAX, then copies only the default_val of the defminmax triplet.
// If param has no default, then fills out_default with zeroes.
param_error_t l_params_copy_default(param_info_t* param_info, void* out_default) {
	assert(param_info);
	assert(out_default);

	if (!param_info || !out_default || l_param_is_variable_size(param_info))
		return param_error_t::FAIL;

	u32 len = l_param_len_bytes(param_info);

	if (l_param_has_no_default(param_info)) {
		// No default is given in the param struct. Use 0 then, just memset the out_default to 0.
		memset(out_default, 0, len);
	} else {
		void* ptr = l_param_get_default_ptr(param_info);
		assert(ptr);
		memcpy(out_default, ptr, len);
	}
	return param_error_t::SUCCESS;
}

// Copy default value for fixed-size types.
// If param HAS_MINMAX, then copies the whole defminmax triplet.
// If param has no default, then fills out_default with zeroes, but skips the min/max fields.
param_error_t l_params_copy_defminmax_or_default(param_info_t* param_info, void* out_default) {
	assert(param_info);
	assert(out_default);

	if (!param_info || !out_default || l_param_is_variable_size(param_info))
		return param_error_t::FAIL;

	u32 len = l_param_len_bytes(param_info);

	if (l_param_has_no_default(param_info)) {
		// No default is given in the param struct. Use 0 then, just memset the out_default to 0.
		memset(out_default, 0, len);
	} else {
		void* ptr = l_param_get_default_ptr(param_info);
		assert(ptr);
		if (param_info->flags & param_info_t::HAS_MINMAX)
			len *= 3;
		memcpy(out_default, ptr, len);
	}
	return param_error_t::SUCCESS;
}

// Copy the string default value to destination. Include the 2 header bytes (max_len, len).
//
// out_str[0] is max_len
// out_str[1] is current_len
// out_str[2..] current_len bytes of string follow
void l_params_copy_from_defaults_str(param_info_t* param_info, u8* out_str) {
	assert(param_info);
	assert(out_str);
	//assert(out_str_max_len > 0);

	if (!param_info || !out_str)// || out_str_max_len == 0)
		return;

	u8* src = l_param_get_default_str_ptr(param_info);
	u8 max_len = src[0];
	u8 str_len = src[1];

	assert(str_len <= max_len);
//	assert(str_len <= out_str_max_len);

//	if (str_len > out_str_max_len) str_len = out_str_max_len;

	out_str[0] = max_len;
	out_str[1] = str_len;
	memcpy(out_str+2, src+2, str_len);
}

void l_build_param_info_base_str(param_info_t* param_info, const char* prefix, char* dst, u32 dst_max_len) {
	assert(param_info);
	const char* type_str = l_param_type_to_str((params_type_e)param_info->type);
	snprintf(dst, dst_max_len, "%s%-15s type %3s component 0x%02X sec_level %i ",
	        prefix, param_info->name, type_str, param_info->component, param_info->security_level);
}

void l_params_print_all(params_table_t* params_info) {
	if (!params_info) return;

	for (int i = 0; i < ELEMENTS_IN_ARRAY(params_info->params_info); i++) {

		param_info_t* param_info = &params_info->params_info[i];
		if (param_info->flags & param_info_t::DISABLED)
			continue;

		param_info_public_t param_inf;
		param_error_t e = params_get_info(i, &param_inf);
		assert(e == param_error_t::SUCCESS);
		if (e != param_error_t::SUCCESS) {
			printf("ERROR ASSERT\n");
			continue;
		}

		const char* pre = "    "; // parameter line prefix. used for nice indentation.
		char base_str[100];
		char s[200]; // buffer for snprintf. parameter info line is built into this buf before sent to the logger.

		l_build_param_info_base_str(param_info, pre, base_str, sizeof(base_str));

		// d is param_inf.param_*. str_buf is a temporary buffer for building the string. temp buf is necessary because esp32 jtag debugging doesn't support 64-bit wide printf parameters.
		#define PRINT_PARAM_I64(str_buf, str_buf_size, param_inf, param_value, d) \
			if ((param_inf).has_minmax) \
				snprintf((str_buf), (str_buf_size), "%sval %" PRIi64 " def %" PRIi64 " min %" PRIi64 " max %" PRIi64, base_str, (i64)(param_value), (i64)(d).default_val, (i64)(d).min, (i64)(d).max); \
			else \
				snprintf((str_buf), (str_buf_size), "%sval %" PRIi64 " def %" PRIi64, base_str, (i64)(param_value), (i64)(d).default_val); \
			puts((str_buf));

		#define PRINT_PARAM_U64(str_buf, str_buf_size, param_inf, param_value, d) \
			if ((param_inf).has_minmax) \
				snprintf((str_buf), (str_buf_size), "%sval %" PRIu64 " def %" PRIu64 " min %" PRIu64 " max %" PRIu64, base_str, (u64)(param_value), (u64)(d).default_val, (u64)(d).min, (u64)(d).max); \
			else \
				snprintf((str_buf), (str_buf_size), "%sval %" PRIu64 " def %" PRIu64, base_str, (u64)(param_value), (u64)(d).default_val); \
			puts((str_buf));

		// no need for f32 since f32 is automatically converted to f64 by printf.
		#define PRINT_PARAM_F64(str_buf, str_buf_size, param_inf, param_value, d) \
			if ((param_inf).has_minmax) \
				snprintf((str_buf), (str_buf_size), "%sval %f def %f min %f max %f", base_str, (param_value), (d).default_val, (d).min, (d).max); \
			else \
				snprintf((str_buf), (str_buf_size), "%sval %f def %f", base_str, (param_value), (d).default_val); \
			puts((str_buf));


		if (!l_param_is_variable_size(param_info)) {

			conv_t* val = (conv_t*)l_param_get_value_ptr(param_info);
			conv_t  def_val;
			conv_t* default_val = &def_val;
			(conv_t*)l_param_get_default_ptr(param_info);

			assert(l_params_copy_default(param_info, &def_val) == param_error_t::SUCCESS);

			switch (param_inf.type) {
			case params_type_e::I8: {
				PRINT_PARAM_I64(s, sizeof(s), param_inf, val->i8_0, param_inf.param_i8);
				break;
			}
			case params_type_e::I16: {
				PRINT_PARAM_I64(s, sizeof(s), param_inf, val->i16_0, param_inf.param_i16);
				break;
			}
			case params_type_e::I32: {
				PRINT_PARAM_I64(s, sizeof(s), param_inf, val->i32_0, param_inf.param_i32);
				break;
			}
			case params_type_e::I64: {
				PRINT_PARAM_I64(s, sizeof(s), param_inf, val->i64_0, param_inf.param_i64);
				break;
			}
			case params_type_e::U8: {
				PRINT_PARAM_U64(s, sizeof(s), param_inf, val->u8_0, param_inf.param_u8);
				break;
			}
			case params_type_e::U16: {
				PRINT_PARAM_U64(s, sizeof(s), param_inf, val->u16_0, param_inf.param_u16);
				break;
			}
			case params_type_e::U32: {
				PRINT_PARAM_U64(s, sizeof(s), param_inf, val->u32_0, param_inf.param_u32);
				break;
			}
			case params_type_e::U64: {
				PRINT_PARAM_U64(s, sizeof(s), param_inf, val->u64_0, param_inf.param_u64);
				break;
			}
			case params_type_e::F32: {
				PRINT_PARAM_F64(s, sizeof(s), param_inf, val->f32_0, param_inf.param_f32);
				break;
			}
			case params_type_e::F64: {
				PRINT_PARAM_F64(s, sizeof(s), param_inf, val->f64_0, param_inf.param_f64);
				break;
			}
			case params_type_e::FLAGS8:
			case params_type_e::FLAGS16:
			case params_type_e::FLAGS32: {
				char bin_str_val[33];
				char bin_str_default[33];

				u8 num_bits = l_param_len_bytes(param_info) * 8;
				u32 local_value = 0, local_default = 0;

				if (param_info->type == (u8)params_type_e::FLAGS8) {
					local_value = val->u8_0;
					local_default = default_val->u8_0;
				} else if (param_info->type == (u8)params_type_e::FLAGS16) {
					local_value = val->u16_0;
					local_default = default_val->u16_0;
				} else if (param_info->type == (u8)params_type_e::FLAGS32) {
					local_value = val->u32_0;
					local_default = default_val->u32_0;
				}

				g_generate_binary_string(bin_str_val, sizeof(bin_str_val), local_value, num_bits, 8);
				g_generate_binary_string(bin_str_default, sizeof(bin_str_default), local_default, num_bits, 8);
				snprintf(s, sizeof(s), "%sval %s def %s", base_str, bin_str_val, bin_str_default);
				puts(s);
				break;
			}
			case params_type_e::UUID128: {
				char uuid_str_val[37];
				char uuid_str_default[37];

				g_uuid_bin_to_str_canonical(&val->u8_0, uuid_str_val, sizeof(uuid_str_val));
				g_uuid_bin_to_str_canonical(&default_val->u8_0, uuid_str_default, sizeof(uuid_str_default));
				snprintf(s, sizeof(s), "%sval %s def %s", base_str, uuid_str_val, uuid_str_default);
				puts(s);
				break;
			}
			case params_type_e::TIME_UNIX_US64:
			case params_type_e::TIME_ATOMIC_US64: {
				char time_str_val[25];
				char time_str_default[25];
				//g_time_struct_t ts;
				//g_timestamp_us_to_time(val->i64_0, &ts);
				g_timestamp_us_to_iso8601(val->i64_0, time_str_val, sizeof(time_str_val));
				g_timestamp_us_to_iso8601(default_val->i64_0, time_str_default, sizeof(time_str_default));
				snprintf(s, sizeof(s), "%sval %s def %s", base_str, time_str_val, time_str_default);
				puts(s);
				break;
			}
			default:
				assert(false);
			};

		} else {

			switch (param_inf.type) {
			case params_type_e::STR: {
				const char *value_str;
				u8 value_str_len;
				param_error_t err = params_get_str(i, &value_str, &value_str_len);
				if (err != param_error_t::SUCCESS) {
					printf("ERROR ASSERT\n");
					break;
				}

				printf("%sval '%.*s' def '%.*s'\n", base_str,
				       value_str_len, value_str,
				       param_inf.param_str.len, param_inf.param_str.ptr);
				break;
			}
			default: break;
			};
		}
	}
}
