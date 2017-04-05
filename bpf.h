// -*- C++ -*-
// Copyright (C) 2016 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef BPF_H
#define BPF_H

struct systemtap_session;
int translate_bpf_pass (systemtap_session& s);

#endif // BPF_H
