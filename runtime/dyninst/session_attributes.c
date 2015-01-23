// stapdyn session attribute code
// Copyright (C) 2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef SESSION_ATTRIBUTES_C
#define SESSION_ATTRIBUTES_C

#include "session_attributes.h"

static struct _stp_session_attributes _stp_init_session_attributes = {
	.log_level = 0,
	.suppress_warnings = 0,
	.stp_pid = 0,
	.target = 0,
	.tz_gmtoff = 0,
	.tz_name = "",
	.module_name = "",
	.outfile_name = "",
};

static void stp_session_attributes_init(void)
{
	*stp_session_attributes() = _stp_init_session_attributes;
}

static int stp_session_attribute_setter(const char *name, const char *value)
{
	// Note that We start all internal variables with '@', since
	// that can't start a "real" variable.

	struct _stp_session_attributes *init = &_stp_init_session_attributes;

#define set_num(field, type)					\
	if (strcmp(name, "@" #field) == 0) {			\
		char *endp = NULL;				\
		errno = 0;					\
		init->field = strto##type(value, &endp, 0);	\
		return (endp == value || *endp != '\0') ?	\
			-EINVAL : -errno;			\
	}

#define set_string(field)					\
	if (strcmp(name, "@" #field) == 0) {			\
		size_t size = sizeof(init->field);		\
		size_t len = strlcpy(init->field, value, size);	\
		return (len < size) ? 0 : -EOVERFLOW;		\
	}

	set_num(log_level, ul);
	set_num(suppress_warnings, ul);
	set_num(stp_pid, ul);
	set_num(target, ul);
	set_num(tz_gmtoff, l);
	set_string(tz_name);
	set_string(module_name);
	set_string(outfile_name);

#undef set_num
#undef set_string

	return -ENOENT;
}

#endif // SESSION_ATTRIBUTES_C
