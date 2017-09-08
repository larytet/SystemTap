// -*- C++ -*-
// Copyright (C) 2017 Serhei Makarov
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.
//
// ---
//
// This file incorporates code from the re2c project; please see
// the file README.stapregex for details.

#ifndef STAPREGEX_DEFINES_H
#define STAPREGEX_DEFINES_H

// XXX: currently we only support ASCII
//#define NUM_REAL_CHARS 128
#define NUM_REAL_CHARS 129
#define rchar unsigned char
// XXX: special case -- 128 is used for 'unknown character'

#endif // STAPREGEX_DEFINES_H
