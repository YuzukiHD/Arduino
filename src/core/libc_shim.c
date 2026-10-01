/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Symbols a sketch needs from a C library other than the one of the image: the libm of the
 * compiler that links the sketch (newlib) reports errors through __errno().
 */
#include <errno.h>

int *__errno(void)
{
	return &errno;
}
