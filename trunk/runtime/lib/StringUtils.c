/** String formatting functions for primitive types. */

#include "tart_object.h"
#include "tart_string.h"

#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#if HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if WIN32
  #define bool int
  #define snprintf _snprintf
#endif

const String * bool_toString(bool value) {
  if (value) {
    return String_create("True", 4);
  } else {
    return String_create("False", 5);
  }

  return NULL;
}

const String * char_toString(uint32_t value) {
  char data[16];
  size_t length;
  if (value == 0) {
    length = snprintf(data, sizeof(data), "\\0");
  } else if (value < 0x32) {
    length = snprintf(data, sizeof(data), "\\x%x", value);
  } else {
    length = snprintf(data, sizeof(data), "%lc", value);
  }

  if (length == (size_t) -1) {
    return String_create("\xff\xfd", 2);
  } else {
    return String_create(data, length);
  }
}

const String * int8_toString(int8_t value) {
  char data[16];
  size_t length = snprintf(data, sizeof(data), "%d", value);
  return String_create(data, length);
}

const String * int16_toString(int16_t value) {
  char data[16];
  int length = snprintf(data, sizeof(data), "%d", value);
  return String_create(data, length);
}

const String * int32_toString(int32_t value) {
  char data[16];
  int length = snprintf(data, sizeof(data), "%d", value);
  return String_create(data, length);
}

const String * int64_toString(int64_t value) {
  char data[32];
#if SIZEOF_LONG_LONG == 8
  int length = snprintf(data, sizeof(data), "%lld", (long long)value);
#elif SIZEOF_LONG == 8
  int length = snprintf(data, sizeof(data), "%ld", (long)value);
#else
  #error Unsupported size for int64_t
#endif
  return String_create(data, length);
}

const String * uint8_toString(uint8_t value) {
  char data[16];
  int length = snprintf(data, sizeof(data), "%u", value);
  return String_create(data, length);
}

const String * uint16_toString(uint16_t value) {
  char data[16];
  int length = snprintf(data, sizeof(data), "%u", value);
  return String_create(data, length);
}

const String * uint32_toString(uint32_t value) {
  char data[16];
  int length = snprintf(data, sizeof(data), "%u", value);
  return String_create(data, length);
}

const String * uint64_toString(uint64_t value) {
  char data[32];
#if SIZEOF_LONG_LONG == 8
  int length = snprintf(data, sizeof(data), "%llu", (unsigned long long)value);
#elif SIZEOF_LONG == 8
  int length = snprintf(data, sizeof(data), "%lu", (unsigned long)value);
#else
  #error Unsupported size for int64_t
#endif
  return String_create(data, length);
}

const String * float_toString(float value) {
  char data[16];
  int length = snprintf(data, sizeof(data), "%f", value);
  return String_create(data, length);
}

const String * double_toString(double value) {
  char data[32];
  int length = snprintf(data, sizeof(data), "%lf", value);
  return String_create(data, length);
}

//int32 String_toDouble(const TartString * s, double * result) {
//  double d = strtod(s, ??, ??);
//}
