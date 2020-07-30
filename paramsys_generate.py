# Elmo Trolla, 2020-07-21
# Licence: pick one - public domain / UNLICENCE (https://www.unlicense.org) / MIT (https://opensource.org/licenses/MIT).

params_input = """
# name, type, default, min, max
# name, type, "default", maxlen # maxlen without terminating zero? there is no terminating zero..

# Line starts with "-" to disable the parameter. it will still exist in memory, but won't be accessible from the API.

# string type param values are utf8 encoded
# strings are in python format, meaning characters can be escaped. "\u1234hello" ? TODO: format
# TODO: add flags8, flags16, flags32 and date and uuid types.
# TODO: prevent float NaN and infinities. and subnormal?

# can use hex values. but not for negative values.

#    -----name------  component security_level type  defalt     min     max
#   "               "
# 0 is used internally
#   "_internalparam_", (u8)params_type_e::U32, 0x00,   0,     0,     0,  param_info_t::NO_DEFAULT},')

  1  p11_U16_minmax   1     1   u16     90       1   65535
#  1  p1_I64_minmax    1     1   i64   -100    -200   0x12c  # 0x12c is 300
  2  p2_I64           1     1   i64    -99
  3  p3_U64_minmax    1     1   u64     98
  4  p4_U64           1     1   u64     97
  5  p5_I32_minmax    1     1   i32    -96  -10000       0
  6  p6_I32           1     1   i32    -95
  7  p7_U32_minmax    1     1   u32     94       0       100
  8  p8_U32           1     1   u32     93
  9  p9_I16_minmax    1     1   i16    -92       1    0xff
 10  p10_I16          1     1   i16    -91
# 11  p11_U16_minmax   1     1   u16     90       1   65535
 11  p1_I64_minmax    1     1   i64   -100    -200   0x12c  # 0x12c is 300
 12  p12_U16          1     1   u16     89
 13  p13_I8_minmax    1     1   i8     -88     -20      30
 14  p14_I8           1     1   i8     -87
 15  p15_I8_overflw   1     1   i8     125
 16  p16_U16_overflw  1     1   u16      2
 17  p17_U16_overflw  1     1   u16  32768
 18  p18_U16_overflw  1     1   u16  32769      33   65535       

 19  p19_flags8       1     1   flags8
 20  p20_flags8       1     1   flags8   6
 21  p21_flags16      1     1   flags16  257
 22  p22_flags32      1     1   flags32  260

 23  p23_time_unix    1     1   time_unix_us64
 24  p24_time_atomic  1     1   time_atomic_us64   2014-02-11T18:46:22.66Z # possible formats:
                                                                           # 2014-02-11T18:46:22Z
                                                                           # 2014-02-11T18:46:22.4439128Z
                                                                           # 2014-02-11T18:46:22,443Z
                                                                           # 19209324924
                                                                           # 19_209_324_924

 25  p25_test_8_STR   1     1   str   "hello" 20
 26  p26_test_10_STR  1     1   str   ""      5

 27  p27_test_2_F64   1     1   f64    -10     -20     30
 28  p28_test_3_F32   1     1   f32      1       0      2

 29  p29_uuid128      1     1   uuid128      # possible formats: 12345678123456781234567812345678
                                             #                   12345678-1234-5678-1234-567812345678
                                             #                   0x12345678123456781234567812345678
                                             # TODO: endianness? seems that the 0x format is not the real memory representation. TODO: real real?

 30  p30_time_unix    1     1   time_unix_us64  1111111111111

#  3  p1_U64          1     1     i8   1000
  
#  1  test_1_I32      1     1    i32     10       5     15
#  2  test_2_F64      1     1    f64    -10     -20     30
#  3  test_3_F32      1     1    f32      1       0      2
#  4  test_4_U8       1     1     u8    255
#  5  test_5_I32      1     1    i32    100      90    110
#- 6  test_6_U32      1     1    u32    200     190    210
#  7  test_7_U32      1     1    u32    400     390    410
#  9  test_9_U32      1     1    u32    500     490    510
# 11  test_11_I16     1     1    i16   -400    -390   -410

#  8  test_8_STR      1     1    str   "hello" 20
# 10  test_10_STR     1     1    str   ""      5

"""

import logging
log = logging.getLogger(__name__)

logging.basicConfig(level=logging.NOTSET, format="%(asctime)s %(name)s %(levelname)-5s: %(message)s")

import struct
import uuid
import datetime


#FILENAME_PREPEND = "test-"
FILENAME_PREPEND = ""

# TODO: implement u128, i128. struct module doesn't support these.

u8, u16, u32, u64,\
	i8, i16, i32, i64,\
	f32, f64,\
	flags8, flags16, flags32,\
	uuid128,\
	time_unix_us64, time_atomic_us64,\
	strt = 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17

type_minmax = {
	u8: (0, 0xff), u16: (0, 0xffff), u32: (0, 0xffffffff), u64: (0, 0xffffffffffffffff),
	i8: (-0x80, 0x7f), i16: (-0x8000, 0x7fff), i32: (-0x80000000, 0x7fffffff), i64: (-0x8000000000000000, 0x7fffffffffffffff),
	flags8: (0, 0xff), flags16: (0, 0xffff), flags32: (0, 0xffffffff),
	time_atomic_us64: (-0x8000000000000000, 0x7fffffffffffffff), time_unix_us64: (-0x8000000000000000, 0x7fffffffffffffff)}

type_from_str = {
	"u8": u8, "u16": u16, "u32": u32, "u64": u64,
	"i8": i8, "i16": i16, "i32": i32, "i64": i64,
	"f32": f32, "f64": f64,
	"flags8": flags8, "flags16": flags16, "flags32": flags32,
	"uuid128": uuid128,
	"time_unix_us64": time_unix_us64, "time_atomic_us64": time_atomic_us64,
	"str": strt}

type_to_str = {
	u8: "u8", u16: "u16", u32: "u32", u64: "u64",
	i8: "i8", i16: "i16", i32: "i32", i64: "i64",
	f32: "f32", f64: "f64",
	flags8: "flags8", flags16: "flags16", flags32: "flags32",
	uuid128: "uuid128",
	time_unix_us64: "time_unix_us64", time_atomic_us64: "time_atomic_us64",
	strt: "str"}

type_to_structpack = {
	u8: ">B", u16: ">H", u32: ">I", u64: ">Q", i8: ">b", i16: ">h", i32: ">i", i64: ">q", f32: ">f", f64: ">d",
	flags8: ">B", flags16: ">H", flags32: ">I",
	time_unix_us64: ">q", time_atomic_us64: ">q"}


def timestr_to_timestamp(timestr):
	"""timestr format: '2014-02-11T18:46:22Z' | '2014-02-11T18:46:22.4439128Z' | '2014-02-11T18:46:22,443Z' """
	# this is the shit:
	#    https://stackoverflow.com/questions/8777753/converting-datetime-date-to-utc-timestamp-in-python
	if "." in timestr:   f = "%Y-%m-%dT%H:%M:%S.%fZ"
	elif "," in timestr: f = "%Y-%m-%dT%H:%M:%S,%fZ"
	else:                f = "%Y-%m-%dT%H:%M:%SZ"
	# python 3:
	# ciso8601.parse_datetime(timestr).timestamp()
	return datetime.datetime.strptime(timestr, f).replace(tzinfo=datetime.timezone.utc).timestamp()


def str_to_int(s):
	"""Convert binary strings (starting with 0b), hex strings (starting with 0x), decimal strings to int"""
	if s.startswith("0x"):
		return int(s, 16)
	elif s.startswith("0b"):
		return int(s, 2)
	else:
		return int(s)


def validate(param_type, values_list):
	minval, maxval = type_minmax[param_type]
	for v in values_list:
		if v < minval or v > maxval:
			raise RuntimeError(f"some values are out of range of {minval}..{maxval}")


class Param:
	def __init__(self, index, name, component, security_level, param_type):
		self.index = index
		self.name = name
		self.component = component
		self.security_level = security_level
		self.param_type = param_type

		self.used = True
		self.has_default = False

		self.values_index = 65535  # calculated during memory layout stage
		self.defaults_index = 65535  # calculated during memory layout stage
		# default_value or default_value and min/max converted to string depending on param type:
		#   u8, i8: "0x12"
		#   u16, i16: "0x5628"
		#   u32, i32, f32: "0x890f9344"
		#   u64, i64, f64: "0xdd89e9aa729472bd"
		#   u16, i16 with has_minmax: '"0x5628", "0xaf44", "0x3c62"'
		#   str:
		#       '124,   5, 0x6b, 0x65, 0x6c, 0x6c, 0x6f' (for max len 124 and default str "hello")
		#       '124,   0" (for max len 124 and no default str)
		self.default_value_str = None
		self.line_num = -1
		self.line_str = ""

		# flags. TODO: move to flags..
		self.has_minmax = False

	def __str__(self):
		return f"used {self.used:5} idx {self.index:2} {self.name:15} comp {self.component:3} seclevel {self.security_level:3} {type_to_str[self.param_type]:3} has_def {self.has_default:5} def_idx {self.defaults_index:2} val_idx {self.values_index:2}"


class ParamInt(Param):
	def __init__(self, index, name, component, security_level, param_type):
		assert param_type in (u8, u16, u32, u64, i8, i16, i32, i64)
		super().__init__(index, name, component, security_level, param_type)
		self.has_minmax = False  # implies has_default
		self.min_value = 0
		self.max_value = 0
		self.default_value = 0
		self.default_value_str = None

	def __str__(self):
		return super().__str__() + f' has_minmax {self.has_minmax:5} def {str(self.default_value):5} min {self.min_value:5} max {self.max_value:5} def_str {repr(self.default_value_str)}'


class ParamFloat(Param):
	def __init__(self, index, name, component, security_level, param_type):
		assert param_type in (f32, f64)
		super().__init__(index, name, component, security_level, param_type)
		self.has_minmax = False  # implies has_default
		self.min_value = 0
		self.max_value = 0
		self.default_value = 0

	def __str__(self):
		return super().__str__() + f' has_minmax {self.has_minmax:5} def {str(self.default_value):5} min {self.min_value:5} max {self.max_value:5} def_str {repr(self.default_value_str)}'


class ParamFlags(Param):
	def __init__(self, index, name, component, security_level, param_type):
		assert param_type in (flags8, flags16, flags32)
		super().__init__(index, name, component, security_level, param_type)
		self.default_value = 0
		self.default_value_str = None

	def __str__(self):
		return super().__str__() + f' def {str(self.default_value):5} def_str {repr(self.default_value_str)}'


class ParamUUID(Param):
	def __init__(self, index, name, component, security_level, param_type):
		assert param_type in (uuid128,)
		super().__init__(index, name, component, security_level, param_type)
		self.default_value = 0
		self.default_value_str = None

	def __str__(self):
		return super().__str__() + f' def {str(self.default_value):5} def_str {repr(self.default_value_str)}'


class ParamTime(Param):
	def __init__(self, index, name, component, security_level, param_type):
		assert param_type in (time_unix_us64, time_atomic_us64)
		super().__init__(index, name, component, security_level, param_type)
		self.default_value = 0
		self.default_value_str = None

	def __str__(self):
		return super().__str__() + f' def {str(self.default_value):5} def_str {repr(self.default_value_str)}'


class ParamStr(Param):
	def __init__(self, index, name, component, security_level, param_type):
		assert param_type in (strt,)
		super().__init__(index, name, component, security_level, param_type)
		self.max_len = -1  # TODO: with or without zerotermination?
		self.default_value = ""

	def __str__(self):
		return super().__str__() + f' max_len {self.max_len:3} def {repr(self.default_value)}'


# - 6  PARAM_6_U32      1     1    u32    200,     190,    210
# return ParamInt, ParamFloat, ParamStr object,
def parse_line_to_param(line_num, line_str):
	try:
		l = line_str.strip()
		if "#" in l:
			l = l.split("#")[0]
		if not l or l.startswith("#"):
			return

		param_used = not l.startswith("-")
		if not param_used:
			l = l[1:].strip()

		pieces = l.split()
		if len(pieces) == 5:
			index, name, component, security_level, param_type = pieces
			line_remainder = ""
		else:
			index, name, component, security_level, param_type, line_remainder = l.split(maxsplit=5)

		index = int(index)
		assert len(name) <= 15
		component = int(component)
		security_level = int(security_level)
		if param_type not in type_from_str:
			raise RuntimeError(f"unknown param_type: {param_type!r}")
		param_type = type_from_str[param_type]
		line_remainder = line_remainder.strip()

		r = line_remainder.split()

		if param_type in [u8, u16, u32, u64, i8, i16, i32, i64]:
			param = ParamInt(index, name, component, security_level, param_type)

			if len(r) == 0:
				param.has_default = False
				param.default_value = 0
			elif len(r) == 1:
				param.has_default = True
				param.default_value = str_to_int(r[0])
			elif len(r) == 3:
				param.has_default = True
				param.has_minmax = True
				param.default_value = str_to_int(r[0])
				param.min_value = str_to_int(r[1])
				param.max_value = str_to_int(r[2])
			else:
				raise RuntimeError("error parsing line %i: %r" % (line_num, line_str))

			validate(param_type, (param.default_value, param.min_value, param.max_value))

		elif param_type in [f32, f64]:
			param = ParamFloat(index, name, component, security_level, param_type)

			if len(r) == 0:
				param.has_default = False
				param.default_value = 0.
			elif len(r) == 1:
				param.has_default = True
				param.default_value = float(r[0])
			elif len(r) == 3:
				param.has_default = True
				param.has_minmax = True
				param.default_value = float(r[0])
				param.min_value = float(r[1])
				param.max_value = float(r[2])
			else:
				raise RuntimeError("error parsing line %i: %r" % (line_num, line_str))

		elif param_type in [flags8, flags16, flags32]:
			param = ParamFlags(index, name, component, security_level, param_type)

			assert len(r) <= 1
			if len(r) == 0:
				param.has_default = False
				param.default_value = 0
			elif len(r) == 1:
				param.has_default = True
				param.default_value = str_to_int(r[0])

			validate(param_type, (param.default_value,))

		elif param_type in [uuid128,]:
			param = ParamUUID(index, name, component, security_level, param_type)

			assert len(r) <= 1
			if len(r) == 0:
				param.has_default = False
				param.default_value = uuid.UUID(int=0)
			elif len(r) == 1:
				param.has_default = True
				if r[0].startswith("0x"):
					param.default_value = uuid.UUID(int=int(r[0]))
				else:
					param.default_value = uuid.UUID(r[0])

		elif param_type in [time_unix_us64, time_atomic_us64]:
			param = ParamTime(index, name, component, security_level, param_type)

			if param_type == time_atomic_us64:
				log.warning("time_atomic_us64 handling is incorrect! currently using this exactly like time_unix_us64.")

			assert len(r) <= 1
			if len(r) == 0:
				param.has_default = False
				param.default_value = 0
			elif len(r) == 1:
				param.has_default = True
				if ":" in r[0]:  # assume it's a timestring
					param.default_value = int(timestr_to_timestamp(r[0]) * 1000000)
				else:
					param.default_value = int(r[0])

			validate(param_type, (param.default_value,))

		elif param_type in [strt]:
			param = ParamStr(index, name, component, security_level, param_type)

			#  8  PARAM_8_STR      1     1    str   "hello" 20
			# 10  PARAM_10_STR     1     1    str   ""      5

			assert len(r) == 2

			assert r[0][0] == '"' and r[0][-1] == '"' and len(r[0]) >= 2

			param.default_value = str(r[0][1:-1])  # parses escape codes. for example \t will result in 1 byte.
			param.has_default = bool(param.default_value)
			param.max_len = str_to_int(r[1])

		else:
			raise RuntimeError("error parsing line %i. unknown param type %i: %r" % (line_num, param_type, line_str))

		param.line_num = line_num
		param.line_str = line_str
		param.used = param_used

		# Remove default values for unused params. These params still have to take up space in the values
		# array in EEPROM, because removing params from EEPROM would require the firmware image to know
		# the layout of the EEPROM values array for the current and all previous versions of the firmware in
		# order to safely re-pack the values array.
		if not param.used:
			param.has_default = False
			param.has_minmax = False
			param.default_value = None

		return param

	except RuntimeError:
		log.error("error parsing line %i: %r" % (line_num, line_str))
		raise
	except:
		log.error("error parsing line %i: %r" % (line_num, line_str))
		raise
		#log.exception("")
		#raise RuntimeError("error parsing line %i: %r" % (line_num, line_str))


def parse_text_to_params(params_input):
	params_list = []
	for i, line in enumerate( params_input.splitlines() ):
		param = parse_line_to_param(i, line)
		params_list.append(param)

	return params_list


class ParamsProcessed:
	def __init__(self, params_list):
		self.params_unsorted = params_list[:]
		self.params = sorted(params_list, key=lambda x: x.index)

		# ensure the sorting contract and that all indices are present
		for i in range(len(self.params)-1):
			assert self.params[i+1].index - self.params[i].index == 1

		self.params_8   = [param for param in params_list if param.param_type in (i8, u8, flags8)]
		self.params_16  = [param for param in params_list if param.param_type in (i16, u16, flags16)]
		self.params_32  = [param for param in params_list if param.param_type in (i32, u32, f32, flags32)]
		self.params_64  = [param for param in params_list if param.param_type in (i64, u64, f64, time_unix_us64, time_atomic_us64)]
		self.params_128 = [param for param in params_list if param.param_type in (uuid128,)]
		self.params_str = [param for param in params_list if param.param_type == strt]

		# parameters that have defaults, but not defminmax.
		self.params_defaults_8   = [param for param in self.params_8   if param.has_default and not param.has_minmax]
		self.params_defaults_16  = [param for param in self.params_16  if param.has_default and not param.has_minmax]
		self.params_defaults_32  = [param for param in self.params_32  if param.has_default and not param.has_minmax]
		self.params_defaults_64  = [param for param in self.params_64  if param.has_default and not param.has_minmax]
		self.params_defaults_128 = [param for param in self.params_128 if param.has_default and not param.has_minmax]
		self.params_defaults_str = self.params_str[:]  # all strings have defaults. some are just 0-length.

		# parameters that have defaults with min/max limits.
		self.params_defminmax_8   = [param for param in self.params_8   if param.has_minmax]
		self.params_defminmax_16  = [param for param in self.params_16  if param.has_minmax]
		self.params_defminmax_32  = [param for param in self.params_32  if param.has_minmax]
		self.params_defminmax_64  = [param for param in self.params_64  if param.has_minmax]
		self.params_defminmax_128 = [param for param in self.params_128 if param.has_minmax]

		# generate strings from default values for every param. these get written to the generated c file.

		def gen_default_str(param):
			"""for string type: return in hex - (max_len, default_len, characters..)
			for integer and float types: return the default value in hex (i.e. 0xa8ee9311)"""
			if param.param_type == strt:
				if param.default_value:
					s = ", ".join(f"0x{b:2x}" for b in bytes(param.default_value, "utf8"))
					return f"{param.max_len:3}, {len(param.default_value):3}, " + s
				else:
					return f"{param.max_len:3},   0"

			if param.has_default:
				if param.param_type == uuid128:
					v = param.default_value.bytes
					assert len(v) == 16
					return ", ".join(f"0x{b:2x}" for b in v)
				else:
					return "0x" + struct.pack(type_to_structpack[param.param_type], param.default_value).hex()
			else:
				return ""

		def gen_defminmax_str(param):
			"""return string of concatenated def/min/max in hex, i.e. "0x2321, 0x12aa, 0xfffe" """
			defval = "0x" + struct.pack(type_to_structpack[param.param_type], param.default_value).hex()
			minval = "0x" + struct.pack(type_to_structpack[param.param_type], param.min_value).hex()
			maxval = "0x" + struct.pack(type_to_structpack[param.param_type], param.max_value).hex()
			return f"{defval}, {minval}, {maxval}"

		for param in params_list:
			if param.param_type in [u8, u16, u32, u64, i8, i16, i32, i64, f32, f64] and param.has_minmax:
				param.default_value_str = gen_defminmax_str(param)
			else:
				param.default_value_str = gen_default_str(param)


		v1, v2 = self._calc_values_len_bytes()
		self.params_values_len_bytes = v1
		self.params_values_str_bytes = v2
		self.params_defaults_str_len_bytes = self._calc_defaults_str_len_bytes()

		self._apply_memory_layout_calculations()

	def _apply_memory_layout_calculations(self):
		"""calculate defaults_index (index to defaults array in firmware image) and values_index (index
		to EEPROM values array) for every parameter."""

		# calculate values_index (index to EEPROM values array) for every parameter.

		for params_list in [self.params_8, self.params_16, self.params_32, self.params_64, self.params_128, self.params_str]:
			# ensure the sorting contract
			for i in range(len(params_list) - 1):
				assert params_list[i + 1].index - params_list[i].index >= 1

			for i, param in enumerate(params_list):
				param.values_index = i

		# calculate values_index for strings

		values_index = 0
		for param in self.params_str:
			param.values_index = values_index
			values_index += 2 + param.max_len  # 2 bytes are max str len and current len

		assert values_index == self._calc_values_str_bytes()

		# calculate defaults_index (index to defaults array in firmware image) for every fixed-size parameter.

		params_list_list = [
			self.params_defaults_8, self.params_defaults_16,
			self.params_defaults_32, self.params_defaults_64,
			self.params_defminmax_8, self.params_defminmax_16,
			self.params_defminmax_32, self.params_defminmax_64,
			self.params_defminmax_128]

		for params_list in params_list_list:
			# ensure the sorting contract
			for i in range(len(params_list) - 1):
				assert params_list[i + 1].index - params_list[i].index >= 1

			for i, param in enumerate(params_list):
				assert param.has_default
				param.defaults_index = i

		# calculate defaults_index (index to defaults array in firmware image) for every variable-size parameter.

		str_param_index = 0
		for i, param in enumerate(self.params_defaults_str):
			param.defaults_index = str_param_index
			str_param_index += 2 + (len(param.default_value))

	def _calc_values_len_bytes(self):
		"""total len of values of all fixed size types, with padding"""
		def offsetof_8(): return 0
		def offsetof_16(): end = offsetof_8() + len(self.params_8); return end + (end & 1)  # aligned by 2 bytes
		def offsetof_32(): end = offsetof_16() + len(self.params_16) * 2; return end + (end & 2)  # aligned by 4 bytes
		def offsetof_64(): return offsetof_32() + len(self.params_32) * 4  # aligned by 4 bytes
		def offsetof_128(): return offsetof_64() + len(self.params_64) * 8  # aligned by 4 bytes
		def offsetof_str(): return offsetof_128() + len(self.params_128) * 16  # aligned by 4 bytes

		strlen = self._calc_values_str_bytes()
		return offsetof_str() + strlen, strlen

	def _calc_values_str_bytes(self):
		"""sub-length of values_len_bytes. used for error checking in c code."""
		return sum(2 + s.max_len for s in self.params_str)  # 2 bytes are max str len and current len

	def _calc_defaults_str_len_bytes(self):
		# length byte + length of the default value
		return sum(2 + len(param.default_value) for param in self.params_str)


class GeneratedHeader:
	def __init__(self, public_filenamepath, impl_filenamepath):
		self.file_public = open(public_filenamepath, "wt")
		self.file_impl = open(impl_filenamepath, "wt")

	def write_impl_file(self, params_processed):
		p = params_processed
		f = self.file_impl

		f.write(
			"// autogenerated file. changes in here will be overwritten!\n"
			"\n"
			"#pragma once\n"
			"\n"
			'#include "stdints.h"\n'
			"\n"
			'#include "paramsys.h"\n'
			'#include "paramsys_internal.h"\n'
			"\n"
			"\n"
			f"#define PARAMS_COUNT {len(p.params)}\n"
			"\n"
			f"#define PARAMS_COUNT_8   {len(p.params_8)}\n"
			f"#define PARAMS_COUNT_16  {len(p.params_16)}\n"
			f"#define PARAMS_COUNT_32  {len(p.params_32)}\n"
			f"#define PARAMS_COUNT_64  {len(p.params_64)}\n"
			f"#define PARAMS_COUNT_128 {len(p.params_128)}\n"
			f"#define PARAMS_COUNT_STR {len(p.params_str)}\n"
			"\n"
			f"#define PARAMS_COUNT_DEFAULTS_8    {len(p.params_defaults_8)}\n"
			f"#define PARAMS_COUNT_DEFAULTS_16   {len(p.params_defaults_16)}\n"
			f"#define PARAMS_COUNT_DEFAULTS_32   {len(p.params_defaults_32)}\n"
			f"#define PARAMS_COUNT_DEFAULTS_64   {len(p.params_defaults_64)}\n"
			f"#define PARAMS_COUNT_DEFAULTS_128  {len(p.params_defaults_128)}\n"
			"\n"
			f"#define PARAMS_COUNT_DEFMINMAX_8   {len(p.params_defminmax_8)}\n"
			f"#define PARAMS_COUNT_DEFMINMAX_16  {len(p.params_defminmax_16)}\n"
			f"#define PARAMS_COUNT_DEFMINMAX_32  {len(p.params_defminmax_32)}\n"
			f"#define PARAMS_COUNT_DEFMINMAX_64  {len(p.params_defminmax_64)}\n"
			f"#define PARAMS_COUNT_DEFMINMAX_128 {len(p.params_defminmax_128)}\n"
			"\n"
			f"#define PARAMS_VALUES_LEN_BYTES {p.params_values_len_bytes}  // with padding\n"
			f"#define PARAMS_VALUES_STR_BYTES {p.params_values_str_bytes}  // sub-len of PARAMS_VALUES_LEN_BYTES\n"
			"\n"
			f"#define PARAMS_DEFAULTS_STR_LEN_BYTES {p.params_defaults_str_len_bytes}\n"
			"\n"
			"\n"
		)

		# now generate this part:
		#
		# struct params_table_t {
		# 	param_info_t     params_info[PARAMS_COUNT];
		#
		# 	u8               defaults_8[PARAMS_COUNT_DEFAULTS_8];
		# 	//u16            defaults_16[PARAMS_COUNT_DEFAULTS_16]; // no such params. commented out because c/c++ doesn't allow 0-sized arrays
		# 	//u32            defaults_32[PARAMS_COUNT_DEFAULTS_32]; // no such params. commented out because c/c++ doesn't allow 0-sized arrays
		# 	//u64            defaults_64[PARAMS_COUNT_DEFAULTS_64]; // no such params. commented out because c/c++ doesn't allow 0-sized arrays
		#
		# 	//defminmax_8_t  defminmax_8[PARAMS_COUNT_DEFMINMAX_8]; // no such params. commented out because c/c++ doesn't allow 0-sized arrays
		# 	defminmax_16_t   defminmax_16[PARAMS_COUNT_DEFMINMAX_16];
		# 	defminmax_32_t   defminmax_32[PARAMS_COUNT_DEFMINMAX_32];
		# 	defminmax_64_t   defminmax_64[PARAMS_COUNT_DEFMINMAX_64];
		#
		# 	u8               defaults_str[PARAMS_DEFAULTS_STR_LEN_BYTES];
		# };

		f.write(
			"// Contains default values, min/max values, default string values for all parameters. This struct is not in eeprom.\n"
			"struct params_table_t {\n"
			"	param_info_t      params_info[PARAMS_COUNT];\n"
			"\n")

		lamentation = "no such params. commented out because c/c++ doesn't allow 0-sized arrays"

		if p.params_defaults_8:
			f.write(f"\tu8                defaults_8[PARAMS_COUNT_DEFAULTS_8];\n")
		else:
			f.write(f"\t//u8              defaults_8[PARAMS_COUNT_DEFAULTS_8]; // {lamentation}\n")

		if p.params_defaults_16:
			f.write(f"\tu16               defaults_16[PARAMS_COUNT_DEFAULTS_16];\n")
		else:
			f.write(f"\t//u16             defaults_16[PARAMS_COUNT_DEFAULTS_16]; // {lamentation}\n")

		if p.params_defaults_32:
			f.write(f"\tu32               defaults_32[PARAMS_COUNT_DEFAULTS_32];\n")
		else:
			f.write(f"\t//u32             defaults_32[PARAMS_COUNT_DEFAULTS_32]; // {lamentation}\n")

		if p.params_defaults_64:
			f.write(f"\tu64               defaults_64[PARAMS_COUNT_DEFAULTS_64];\n")
		else:
			f.write(f"\t//u64             defaults_64[PARAMS_COUNT_DEFAULTS_64]; // {lamentation}\n")

		if p.params_defaults_128:
			f.write(f"\tu8                defaults_128[PARAMS_COUNT_DEFAULTS_128*16];\n")
		else:
			f.write(f"\t//u8              defaults_128[PARAMS_COUNT_DEFAULTS_128*16]; // {lamentation}\n")

		f.write("\n")

		if p.params_defminmax_8:
			f.write(f"\tdefminmax_u8_t    defminmax_8[PARAMS_COUNT_DEFMINMAX_8];\n")
		else:
			f.write(f"\t//defminmax_u8_t  defminmax_8[PARAMS_COUNT_DEFMINMAX_8]; // {lamentation}\n")

		if p.params_defminmax_16:
			f.write(f"\tdefminmax_u16_t   defminmax_16[PARAMS_COUNT_DEFMINMAX_16];\n")
		else:
			f.write(f"\t//defminmax_u16_t defminmax_16[PARAMS_COUNT_DEFMINMAX_16]; // {lamentation}\n")

		if p.params_defminmax_32:
			f.write(f"\tdefminmax_u32_t   defminmax_32[PARAMS_COUNT_DEFMINMAX_32];\n")
		else:
			f.write(f"\t//defminmax_u32_t defminmax_32[PARAMS_COUNT_DEFMINMAX_32]; // {lamentation}\n")

		if p.params_defminmax_64:
			f.write(f"\tdefminmax_u64_t   defminmax_64[PARAMS_COUNT_DEFMINMAX_64];\n")
		else:
			f.write(f"\t//defminmax_u64_t defminmax_64[PARAMS_COUNT_DEFMINMAX_64]; // {lamentation}\n")

		f.write("\n")

		if p.params_str:
			f.write(f"\tu8                defaults_str[PARAMS_DEFAULTS_STR_LEN_BYTES];\n")
		else:
			f.write(f"\t//u8              defaults_str[PARAMS_DEFAULTS_STR_LEN_BYTES]; // {lamentation}\n")

		f.write("};\n")
		f.write("\n")
		f.write("\n")

		#
		# now generate the next part..
		#

		f.write(
			"params_table_t params_info = {\n"
				"\t{ // params_info\n"
					'\t\t// param_name, type, component, security_level, defaults_index, values_index, flags\n'
					'\t\t//                                          component\n'
					'\t\t//                                          |       security_levelt\n'
					'\t\t// param name                               |       |      defaults_index\n'
					'\t\t// |                type                    |       |      |      values_index\n'
					'\t\t//******15******* , |                       |       |      |      |   flags\n')
			#		'\t\t{"_internalparam_", (u8)params_type_e::U32, 0x00,   0,     0,     0,  param_info_t::NO_DEFAULT},\n')

		def gen_param_info_str(param):
			name_str = f'"{param.name}"'
			param_type_str = type_to_str[param.param_type].upper()   # F32
			param_type_str = f"(u8)params_type_e::{param_type_str}"  # (u8)params_type_e::F32
			param_flags_str = "0"
			if not param.used:
				param_flags_str = "param_info_t::DISABLED"
			else:
				if param.param_type in [u8, u16, u32, u64, i8, i16, i32, i64, f32, f64]:
					if param.has_minmax:
						param_flags_str = "param_info_t::HAS_MINMAX"
					elif not param.has_default:
						param_flags_str = "param_info_t::NO_DEFAULT"
				elif not param.has_default:
					param_flags_str = "param_info_t::NO_DEFAULT"

			return f'{{{name_str:17}, {param_type_str:22}, 0x{param.component:02x}, {param.security_level:3}, {param.defaults_index:5}, {param.values_index:5},  {param_flags_str}}},\n'

		for param in p.params_unsorted:
			f.write("\t\t" + gen_param_info_str(param))

		f.write("\t},\n")
		f.write("\n")

		def write_defaults_arrary(params, comment):
			if params:
				f.write(f"\t{{ // {comment}\n")
				for param in params:
					f.write(f"\t\t{param.default_value_str}, // {type_to_str[param.param_type]} {param.name} {param.default_value}\n")
				f.write("\t},\n")
				f.write("\n")

		write_defaults_arrary(p.params_defaults_8, "defaults_8")
		write_defaults_arrary(p.params_defaults_16, "defaults_16")
		write_defaults_arrary(p.params_defaults_32, "defaults_32")
		write_defaults_arrary(p.params_defaults_64, "defaults_64")
		write_defaults_arrary(p.params_defaults_128, "defaults_128")

		def write_defminmax_arrary(params, comment):
			if params:
				f.write(f"\t{{ // {comment}\n")
				for param in params:
					f.write(f"\t\t{{{param.default_value_str}}}, // {type_to_str[param.param_type]} {param.name} {param.default_value} min {param.min_value} max {param.max_value}\n")
				f.write("\t},\n")
				f.write("\n")

		write_defminmax_arrary(p.params_defminmax_8, "defminmax_8")
		write_defminmax_arrary(p.params_defminmax_16, "defminmax_16")
		write_defminmax_arrary(p.params_defminmax_32, "defminmax_32")
		write_defminmax_arrary(p.params_defminmax_64, "defminmax_64")
		write_defminmax_arrary(p.params_defminmax_128, "defminmax_128")

		if p.params_str:
			f.write(f"\t{{ // defaults_str\n")
			for param in p.params_str:
				if param.has_default:
					f.write(f'\t\t{param.default_value_str}, // {param.name} max_len {param.max_len} len {len(param.default_value)} {param.default_value!r}\n')
				else:
					f.write(f'\t\t{param.default_value_str}, // {param.name} max_len {param.max_len}\n')
			f.write("\t},\n")
			f.write("\n")

		f.write("};\n")
		f.write("\n")

		f.write(
			"// These are indirection to the params_info defaults arrays.\n"
			"// We can't use \"params_info.defaults_*\" directly, because if the corresponding defaults array is empty, it's not\n"
			"// even declared in the params_info struct by our python generator. Reason is that C/C++ doesn't allow zero-length\n"
			"// arrays and we really don't want to live with the overhead of every param type potentially having one completely\n"
			"// unused defaults array entry. If we don't have any 64-bit and 32-bit params, the overhead will be 8+8*3 + 4+4*3 =\n"
			"// 48 bytes. But it really grows when we'll add 128-bit or uuid types.\n")

		f.write("\n")

		f.write(f'u8*  defaults_8   = {"params_info.defaults_8"   if p.params_defaults_8   else "nullptr"};\n')
		f.write(f'u16* defaults_16  = {"params_info.defaults_16"  if p.params_defaults_16  else "nullptr"};\n')
		f.write(f'u32* defaults_32  = {"params_info.defaults_32"  if p.params_defaults_32  else "nullptr"};\n')
		f.write(f'u64* defaults_64  = {"params_info.defaults_64"  if p.params_defaults_64  else "nullptr"};\n')
		f.write(f'u8*  defaults_128 = {"params_info.defaults_128" if p.params_defaults_128 else "nullptr"};\n')
		f.write("\n")
		f.write(f'defminmax_u8_t*   defminmax_8   = {"params_info.defminmax_8"   if p.params_defminmax_8   else "nullptr"};\n')
		f.write(f'defminmax_u16_t*  defminmax_16  = {"params_info.defminmax_16"  if p.params_defminmax_16  else "nullptr"};\n')
		f.write(f'defminmax_u32_t*  defminmax_32  = {"params_info.defminmax_32"  if p.params_defminmax_32  else "nullptr"};\n')
		f.write(f'defminmax_u64_t*  defminmax_64  = {"params_info.defminmax_64"  if p.params_defminmax_64  else "nullptr"};\n')
		f.write(f'u8*               defminmax_128 = {"params_info.defminmax_128" if p.params_defminmax_128 else "nullptr"};\n')
		f.write("\n")
		f.write(f'u8* defaults_str = {"params_info.defaults_str" if p.params_defaults_str else "nullptr"};\n')
		f.write("\n")

		#for param in p.params_unsorted:
		#	print(str(param))

	def write_public_file(self, params_processed):
		p = params_processed
		f = self.file_public

		f.write(
			"// autogenerated file. changes in here will be overwritten!\n"
			"\n"
			"#pragma once"
			"\n"
			"\n")
		for param in params_processed.params[1:]: # skip the first special _internalparam_
			name = param.name + "_index"
			if param.used:
				# padding: 21 = 15 (max param name len) + 6 (len of "_index")
				f.write(f"#define PARAM_{name:21} {param.index}\n")
			else:
				f.write(f"//#define PARAM_{name:21} {param.index} // param is disabled\n")


def main():

	# 1. parse the params_input string to a list of ParamInt, ParamFloat, ParamStr, .. objects.
	# 2. ensure that param indices start from 1 and there are no missing and reused indices.
	# 3. prepend the _internalparam_ with index 0 for the c implementation.
	# 4. parse the params input file
	# 5. generate output c header files.

	params_list = [param for param in parse_text_to_params(params_input) if param]

	# find out if some indices are missing

	indices = set(param.index for param in params_list)
	params_missing = False
	for i in range(1, max(indices)):
		if i not in indices:
			params_missing = True
			log.error(f"missing parameter with index {i}")

	# find out if param names are reused

	params_names = {}
	params_reused = False
	for param in params_list:
		if param.name in params_names:
			params_reused = True
			log.error(f"param named {param.name!r} used multiple times")
		params_names[param.name] = 1

	if params_missing or params_reused:
		return

	# add the internal parameter
	params_list.insert(0, ParamInt(0, "_internalparam_", 0, 0, u32))

	# parse params and generate the c header files

	params_processed = ParamsProcessed(params_list)
	generated_header_file = GeneratedHeader(FILENAME_PREPEND + "paramsys_generated.h", FILENAME_PREPEND + "paramsys_impl_generated.h")
	generated_header_file.write_impl_file(params_processed)
	generated_header_file.write_public_file(params_processed)

	for param in params_list:
		log.info(f"index {param.index:03} {param.name!r:17} type {type_to_str[param.param_type]}")


if __name__ == "__main__":
	main()
