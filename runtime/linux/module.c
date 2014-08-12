/*
 * module init/exit file for Linux
 *
 * This code is here (instead of in linux/runtime.h) because
 * linux/runtime.h gets included too early.
 *
 * Copyright (C) 2014 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _LINUX_MODULE_H_
#define _LINUX_MODULE_H_

static int _stp_runtime_module_init(void)
{
  int rc = 0;

#ifdef STP_MMV
  rc = _stp_mmv_init();
  if (rc)
    _stp_mmv_exit();
#endif

  return rc;
}

static void _stp_runtime_module_exit(void)
{
#ifdef STP_MMV
  _stp_mmv_exit();
#endif
}

#endif _LINUX_MODULE_H_
