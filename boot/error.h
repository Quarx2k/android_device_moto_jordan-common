#ifndef __ERROR_H__
#define __ERROR_H__
#include "types.h"

typedef enum {
  HW_UNINITIALIZED_IPU = 0,
  IMG_INCORRECT_CHECKSUM,
  IMG_NOT_PROVIDED,
  DSP_IS_NOT_RUNNING,
} error_t;

void critical_error(error_t err);

#endif // __ERROR_H__
