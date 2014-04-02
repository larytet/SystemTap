/* -*- linux-c -*- 
 * Networking compatibility defines.
 * Copyright (C) 2014 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _COMPAT_NET_H_
#define _COMPAT_NET_H_

#include <linux/net.h>

/* Older kernels don't have this defined and don't support
 * sys_sendmmsg(2). We just need the define to get things to compile. */
#ifndef SYS_SENDMMSG
#define SYS_SENDMMSG 20
#endif

#endif /* _COMPAT_NET_H_ */
