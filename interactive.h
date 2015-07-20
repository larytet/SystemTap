// Interactive mode header.
// Copyright (C) 2015 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef INTERACTIVE_H
#define INTERACTIVE_H

#include <vector>

#include "session.h"
#include "remote.h"

extern int interactive_mode (systemtap_session &s, std::vector<remote*> targets);

#endif
