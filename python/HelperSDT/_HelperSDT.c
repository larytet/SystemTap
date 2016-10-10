// systemtap python SDT marker C module
// Copyright (C) 2016 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include <Python.h>
#include <sys/sdt.h>

static PyObject *
trace_callback(PyObject *self, PyObject *args)
{
    int what;
    PyObject *frame_obj, *arg_obj;

    /* Parse the input tuple */
    if (!PyArg_ParseTuple(args, "iOO", &what, &frame_obj, &arg_obj))
	return NULL;

    /* We want to name the probes with the same name as the
     * define. This is tricky, so, we'll just save the define,
     * undefine it, call the STAP_PROBE2 macro, then redfine it. */
    switch (what) {
    case PyTrace_CALL:
#define _PyTrace_CALL PyTrace_CALL
#undef PyTrace_CALL
	STAP_PROBE2(stap_pybridge, PyTrace_CALL, frame_obj, arg_obj);
#define PyTrace_CALL _PyTrace_CALL
#undef _PyTrace_CALL
	break;
    case PyTrace_EXCEPTION:
#define _PyTrace_EXCEPTION PyTrace_EXCEPTION
#undef PyTrace_EXCEPTION
	STAP_PROBE2(stap_pybridge, PyTrace_EXCEPTION, frame_obj, arg_obj);
#define PyTrace_EXCEPTION _PyTrace_EXCEPTION
#undef _PyTrace_EXCEPTION
	break;
    case PyTrace_LINE:
#define _PyTrace_LINE PyTrace_LINE
#undef PyTrace_LINE
	STAP_PROBE2(stap_pybridge, PyTrace_LINE, frame_obj, arg_obj);
#define PyTrace_LINE _PyTrace_LINE
#undef _PyTrace_LINE
	break;
    case PyTrace_RETURN:
#define _PyTrace_RETURN PyTrace_RETURN
#undef PyTrace_RETURN
	STAP_PROBE2(stap_pybridge, PyTrace_RETURN, frame_obj, arg_obj);
#define PyTrace_RETURN _PyTrace_RETURN
#undef _PyTrace_RETURN
	break;
    // FIXME: What about PyTrace_C_CALL, PyTrace_C_EXCEPTION,
    // PyTrace_C_RETURN? Fold them into their non-'_C_' versions or
    // have unique probes?
    default:
	// FIXME: error/exception here?
	return NULL;
    }
    return Py_BuildValue("i", 0);
}

static PyMethodDef HelperSDT_methods[] = {
	{"trace_callback", trace_callback, METH_VARARGS,
	 "Trace callback function."},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

static const char const *HelperSDT_doc =
    "This module provides an interface for interfacing between Python tracing events and systemtap.";

PyMODINIT_FUNC
init_HelperSDT(void)
{
    PyObject *m;

    m = Py_InitModule3("_HelperSDT", HelperSDT_methods,
		       HelperSDT_doc);
    if (m == NULL)
	return;
}
