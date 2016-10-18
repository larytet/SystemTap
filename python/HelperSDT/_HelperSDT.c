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
	STAP_PROBE2(HelperSDT, PyTrace_CALL, frame_obj, arg_obj);
#define PyTrace_CALL _PyTrace_CALL
#undef _PyTrace_CALL
	break;
    case PyTrace_EXCEPTION:
#define _PyTrace_EXCEPTION PyTrace_EXCEPTION
#undef PyTrace_EXCEPTION
	STAP_PROBE2(HelperSDT, PyTrace_EXCEPTION, frame_obj, arg_obj);
#define PyTrace_EXCEPTION _PyTrace_EXCEPTION
#undef _PyTrace_EXCEPTION
	break;
    case PyTrace_LINE:
#define _PyTrace_LINE PyTrace_LINE
#undef PyTrace_LINE
	STAP_PROBE2(HelperSDT, PyTrace_LINE, frame_obj, arg_obj);
#define PyTrace_LINE _PyTrace_LINE
#undef _PyTrace_LINE
	break;
    case PyTrace_RETURN:
#define _PyTrace_RETURN PyTrace_RETURN
#undef PyTrace_RETURN
	STAP_PROBE2(HelperSDT, PyTrace_RETURN, frame_obj, arg_obj);
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

PyDoc_STRVAR(HelperSDT_doc,
	     "This module provides an interface for interfacing between Python tracing events and systemtap.");

#if PY_MAJOR_VERSION >= 3
//
// According to <https://docs.python.org/3/c-api/module.html>:
//
// ====
// Module state may be kept in a per-module memory area that can be
// retrieved with PyModule_GetState(), rather than in static
// globals. This makes modules safe for use in multiple
// sub-interpreters.
//
// This memory area is allocated based on m_size on module creation,
// and freed when the module object is deallocated, after the m_free
// function has been called, if present.
//
// Setting m_size to -1 means that the module does not support
// sub-interpreters, because it has global state.
//
// Setting it to a non-negative value means that the module can be
// re-initialized and specifies the additional amount of memory it
// requires for its state. Non-negative m_size is required for
// multi-phase initialization.
// ====
//
// This C module has no module state, so we'll set m_size to -1 (and
// m_slots, m_traverse, m_clear, and m_free to NULL).
//
// All state information is held by the python HelperSDT module, not
// this _HelperSDT helper C extension module.

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_HelperSDT",
        HelperSDT_doc,
        -1,				/* m_size */
        HelperSDT_methods,
        NULL,				/* m_slots */
        NULL,				/* m_traverse */
        NULL,				/* m_clear */
        NULL				/* m_free */
};
#endif


PyMODINIT_FUNC
init_HelperSDT(void)
{
    PyObject *module;

#if PY_MAJOR_VERSION >= 3
    module = PyModule_Create(&moduledef);
    if (module == NULL)
	return NULL;
    return module;
#else
    module = Py_InitModule3("_HelperSDT", HelperSDT_methods,
			    HelperSDT_doc);
    if (module == NULL)
	return;
#endif
}
