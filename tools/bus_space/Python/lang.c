/*-
 * Copyright (c) 2014 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <Python.h>

#include "bus_space.h"

static PyObject *
bus_read_1(PyObject *self, PyObject *args)
{
	long ofs;
	int rid;
	uint8_t val;

	if (!PyArg_ParseTuple(args, "il", &rid, &ofs))
		return (NULL);
	if (!bs_read(rid, ofs, &val, sizeof(val))) {
		PyErr_SetString(PyExc_IOError, strerror(errno));
		return (NULL);
	}
	return (Py_BuildValue("B", val));
}

static PyObject *
bus_read_2(PyObject *self, PyObject *args)
{
	long ofs;
	int rid;
	uint16_t val;

	if (!PyArg_ParseTuple(args, "il", &rid, &ofs))
		return (NULL);
	if (!bs_read(rid, ofs, &val, sizeof(val))) {
		PyErr_SetString(PyExc_IOError, strerror(errno));
		return (NULL);
	}
	return (Py_BuildValue("H", val));
}

static PyObject *
bus_read_4(PyObject *self, PyObject *args)
{
	long ofs;
	int rid;
	uint32_t val;

	if (!PyArg_ParseTuple(args, "il", &rid, &ofs))
		return (NULL);
	if (!bs_read(rid, ofs, &val, sizeof(val))) {
		PyErr_SetString(PyExc_IOError, strerror(errno));
		return (NULL);
	}
	return (Py_BuildValue("I", val));
}

static PyObject *
bus_write_1(PyObject *self, PyObject *args)
{
	long ofs;
	int rid;
	uint8_t val;

	if (!PyArg_ParseTuple(args, "ilB", &rid, &ofs, &val))
		return (NULL);
	if (!bs_write(rid, ofs, &val, sizeof(val))) {
		PyErr_SetString(PyExc_IOError, strerror(errno));
		return (NULL);
	}
	Py_RETURN_NONE;
}

static PyObject *
bus_write_2(PyObject *self, PyObject *args)
{
	long ofs;
	int rid;
	uint16_t val;

	if (!PyArg_ParseTuple(args, "ilH", &rid, &ofs, &val))
		return (NULL);
	if (!bs_write(rid, ofs, &val, sizeof(val))) {
		PyErr_SetString(PyExc_IOError, strerror(errno));
		return (NULL);
	}
	Py_RETURN_NONE;
}

static PyObject *
bus_write_4(PyObject *self, PyObject *args)
{
	long ofs;
	int rid;
	uint32_t val;

	if (!PyArg_ParseTuple(args, "ilI", &rid, &ofs, &val))
		return (NULL);
	if (!bs_write(rid, ofs, &val, sizeof(val))) {
		PyErr_SetString(PyExc_IOError, strerror(errno));
		return (NULL);
	}
	Py_RETURN_NONE;
}

static PyObject *
bus_map(PyObject *self, PyObject *args)
{
	char *dev;
	int rid;

	if (!PyArg_ParseTuple(args, "s", &dev))
		return (NULL);
	rid = bs_map(dev);
	if (rid == -1) {
		PyErr_SetString(PyExc_IOError, strerror(errno));
		return (NULL);
	}
	return (Py_BuildValue("i", rid));
}

static PyObject *
bus_unmap(PyObject *self, PyObject *args)
{
	int rid;

	if (!PyArg_ParseTuple(args, "i", &rid))
		return (NULL);
	if (!bs_unmap(rid)) {
		PyErr_SetString(PyExc_IOError, strerror(errno));
		return (NULL);
	}
	Py_RETURN_NONE;
}

static PyObject *
bus_subregion(PyObject *self, PyObject *args)
{
	long ofs, sz;
	int rid0, rid;

	if (!PyArg_ParseTuple(args, "ill", &rid0, &ofs, &sz))
		return (NULL);
	rid = bs_subregion(rid0, ofs, sz);
	if (rid == -1) {
		PyErr_SetString(PyExc_IOError, strerror(errno));
		return (NULL);
	}
	return (Py_BuildValue("i", rid));
}

static PyMethodDef bus_space_methods[] = {
    { "read_1", bus_read_1, METH_VARARGS, "Read a 1-byte data item." },
    { "read_2", bus_read_2, METH_VARARGS, "Read a 2-byte data item." },
    { "read_4", bus_read_4, METH_VARARGS, "Read a 4-byte data item." },

    { "write_1", bus_write_1, METH_VARARGS, "Write a 1-byte data item." },
    { "write_2", bus_write_2, METH_VARARGS, "Write a 2-byte data item." },
    { "write_4", bus_write_4, METH_VARARGS, "Write a 4-byte data item." },

    { "map", bus_map, METH_VARARGS,
	"Return a resource ID for a device file created by proto(4)" },
    { "unmap", bus_unmap, METH_VARARGS,
	"Free a resource ID" },
    { "subregion", bus_subregion, METH_VARARGS,
	"Return a resource ID for a subregion of another resource ID" },

    { NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC
initbus_space(void)
{

	Py_InitModule("bus_space", bus_space_methods);
}
