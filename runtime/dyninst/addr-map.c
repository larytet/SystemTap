/* -*- linux-c -*- 
 * Map of addresses to disallow.
 *
 * Copyright (C) 2012-2016 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_ADDR_MAP_C_
#define _STAPDYN_ADDR_MAP_C_ 1

static int
lookup_bad_addr(const int type, const unsigned long addr, const size_t size)
{
	return 0;
}

#endif /* _STAPDYN_ADDR_MAP_C_ */
