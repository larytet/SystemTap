/*
 * Copyright (C) 2014 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _MMV_C_
#define _MMV_C_

#if defined(__KERNEL__)
#include "linux/mmv.c"
#elif defined(__DYNINST__)
//#include "dyninst/mmv.c"
#error "Not written yet"
#endif

#endif /* _MMV_C_ */
