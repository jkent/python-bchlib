/*
 * Python C module for BCH encoding/decoding.
 *
 * Copyright © 2013, 2017-2018 Jeff Kent <jeff@jkent.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <Python.h>
#include <structmember.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "bch.h"


typedef struct {
	PyObject_HEAD
	struct bch_control *bch;
	bool reversed;
} BCHObject;

static void
reverse_bytes(uint8_t *dest, const uint8_t *src, int length)
{
	while(length--) {
		*dest = ((*src * 0x0202020202ULL) & 0x010884422010ULL) % 1023;
		src++;
		dest++;
	}
}

static void
BCH_dealloc(BCHObject *self)
{
	if (self->bch)
		free_bch(self->bch);

	Py_TYPE(self)->tp_free((PyObject *)self);
}

static int
BCH_init(BCHObject *self, PyObject *args, PyObject *kwds)
{
	unsigned int prim_poly;
	int t;
	PyObject *reversed = NULL;
	static char *kwlist[] = {"polynomial", "t", "reverse", NULL};
	int m;
	unsigned int tmp;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Ii|O", kwlist,
			&prim_poly, &t, &reversed))
		return -1;

	self->reversed = 0;
	if (reversed) {
		Py_INCREF(reversed);
		self->reversed = (PyObject_IsTrue(reversed) == 1);
		Py_DECREF(reversed);
	}

	tmp = prim_poly;
	m = 0;
	while (tmp >>= 1)
		m++;

	self->bch = init_bch(m, t, prim_poly);
	if (!self->bch) {
		PyErr_SetString(PyExc_RuntimeError,
			"unable to inititalize BCH, bad parameters?");
		return -1;
	}

	return 0;
}

static PyObject *
BCH_encode(BCHObject *self, PyObject *args, PyObject *kwds)
{
	Py_buffer data, ecc = {0};
	static char *kwlist[] = {"data", "ecc", NULL};
	unsigned int ecc_bytes = self->bch->ecc_bytes;
	uint8_t result_ecc[ecc_bytes];

#if PY_MAJOR_VERSION >= 3
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*|y*", kwlist, &data, &ecc))
#else
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s*|s*", kwlist, &data, &ecc))
#endif
		return NULL;

	if (ecc.buf) {
		if (ecc.len != ecc_bytes) {
			PyErr_Format(PyExc_ValueError,
				"ecc length should be %d bytes",
				ecc_bytes);
			return NULL;
		}
		memcpy(result_ecc, ecc.buf, ecc_bytes);
	} else {
		memset(result_ecc, 0, ecc_bytes);
	}

	if (self->reversed) {
		uint8_t *reversed = malloc(data.len);
		reverse_bytes(reversed, data.buf, data.len);
		encode_bch(self->bch, reversed, data.len, result_ecc);
		free(reversed);
	}
	else {
		encode_bch(self->bch, data.buf, data.len, result_ecc);
	}

	return PyByteArray_FromStringAndSize((char *)result_ecc, ecc_bytes);
}

static PyObject *
BCH_decode(BCHObject *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {"data", "ecc", NULL};
	Py_buffer data, ecc;
	unsigned int errloc[self->bch->t];
	uint8_t *result_data, *result_ecc;
	PyObject *result = NULL;

#if PY_MAJOR_VERSION >= 3
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*y*", kwlist, &data, &ecc))
#else
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s*s*", kwlist, &data, &ecc))
#endif
		return NULL;

	if (ecc.len != self->bch->ecc_bytes) {
		PyErr_Format(PyExc_ValueError,
			"ecc length should be %d bytes",
			self->bch->ecc_bytes);
		return NULL;
	}

	result_data = malloc(data.len);
	result_ecc = malloc(ecc.len);

	if (self->reversed) {
		reverse_bytes(result_data, data.buf, data.len);
	} else {
		memcpy(result_data, data.buf, data.len);
	}

	memcpy(result_ecc, ecc.buf, ecc.len);

	int nerr = decode_bch(self->bch, data.buf, data.len, ecc.buf, NULL, NULL,
				errloc);

	if (nerr < 0) {
		if (nerr == -EINVAL) {
			PyErr_SetString(PyExc_ValueError,
				"invalid parameters");
			goto cleanup;
		} else if (nerr == -EBADMSG) {
			nerr = -1;
		} else {
			goto cleanup;
		}
	}

	for (int i = 0; i < nerr; i++) {
		unsigned int bitnum = errloc[i];
		if (bitnum >= data.len*8 + ecc.len*8) {
			PyErr_SetString(PyExc_IndexError,
				"uncorrectable error");
			goto cleanup;
		}
		if (bitnum < data.len*8)
			result_data[bitnum/8] ^= 1 << (bitnum & 7);
		else 
			result_ecc[bitnum/8 - data.len] ^= 1 << (bitnum & 7);
	}

	if (self->reversed) {
		reverse_bytes(result_data, result_data, data.len);
	}

	/* TODO: eliminate extra copies created by PyByteArray_FromStringAndSize */
	result = PyTuple_New(3);
	PyTuple_SetItem(result, 0, PyLong_FromLong(nerr));
	PyTuple_SetItem(result, 1,
		PyByteArray_FromStringAndSize((char *)result_data, data.len));
	PyTuple_SetItem(result, 2,
		PyByteArray_FromStringAndSize((char *)result_ecc, ecc.len));

cleanup:
	free(result_data);
	free(result_ecc);
	PyBuffer_Release(&data);
	PyBuffer_Release(&ecc);
	return result;
}

static PyObject *
BCH_decode_inplace(BCHObject *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {"data", "ecc", NULL};
	Py_buffer data, ecc;
	unsigned int errloc[self->bch->t];
	PyObject *result = NULL;

#if PY_MAJOR_VERSION >= 3
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*y*", kwlist, &data, &ecc))
#else
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s*s*", kwlist, &data, &ecc))
#endif
		return NULL;

	if (data.readonly) {
		PyErr_SetString(PyExc_ValueError,
			"data must not be readonly");
		return NULL;
	}

	if (ecc.len != self->bch->ecc_bytes) {
		PyErr_Format(PyExc_ValueError,
			"ecc length should be %d bytes",
			self->bch->ecc_bytes);
		return NULL;
	}

	if (self->reversed) {
		reverse_bytes(data.buf, data.buf, data.len);
	}

	int nerr = decode_bch(self->bch, data.buf, data.len, ecc.buf, NULL, NULL,
				errloc);

	if (nerr < 0) {
		if (nerr == -EINVAL) {
			PyErr_SetString(PyExc_ValueError,
				"invalid parameters");
			goto cleanup;
		} else if (nerr == -EBADMSG) {
			nerr = -1;
		} else {
			goto cleanup;
		}
	}

	for (int i = 0; i < nerr; i++) {
		unsigned int bitnum = errloc[i];
		if (bitnum >= data.len*8 + ecc.len*8) {
			PyErr_SetString(PyExc_IndexError,
				"uncorrectable error");
			goto cleanup;
		}
		if (bitnum < data.len*8) {
			((char *)data.buf)[bitnum/8] ^= 1 << (bitnum & 7);
		} else if (!ecc.readonly) {
			((char *)ecc.buf)[bitnum/8 - data.len] ^= 1 << (bitnum & 7);
		}
	}

	if (self->reversed) {
		reverse_bytes(data.buf, data.buf, data.len);
	}

	result = PyLong_FromLong(nerr);

cleanup:
	PyBuffer_Release(&data);
	PyBuffer_Release(&ecc);
	return result;
}

static PyObject *
BCH_decode_syndromes(BCHObject *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {"data", "syndromes", NULL};
	Py_buffer data;
	PyObject *po_syndromes;
	unsigned int errloc[self->bch->t];
	uint8_t *result_data;
	PyObject *result = NULL;

#if PY_MAJOR_VERSION >= 3
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*O", kwlist, &data,
									 &po_syndromes))
#else
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s*O", kwlist, &data,
									 &po_syndromes))
#endif
		return NULL;

	if (!PySequence_Check(po_syndromes)) {
		PyErr_SetString(PyExc_TypeError,
			"'syndromes' must be a sequence type");
		return NULL;
	}

	if (PySequence_Length(po_syndromes) != self->bch->t*2) {
		PyErr_Format(PyExc_ValueError,
			"'syndromes' must have %d elements",
			self->bch->t*2);
		return NULL;
	}

	result_data = malloc(data.len);

	if (self->reversed) {
		reverse_bytes(result_data, data.buf, data.len);
	} else {
		memcpy(result_data, data.buf, data.len);
	}

	Py_INCREF(po_syndromes);
	unsigned int syndromes[self->bch->t];
	for (unsigned int i = 0; i < self->bch->t*2; i++) {
		PyObject *value = PySequence_GetItem(po_syndromes, i);
		Py_INCREF(value);
		long ltmp = PyLong_AsLong(value);
		if (ltmp == -1 && PyErr_Occurred()) {
			Py_DECREF(value);
			return NULL;
		}
		syndromes[i] = ltmp;
		Py_DECREF(value);
	}
 
	int nerr = decode_bch(self->bch, NULL, data.len, NULL, NULL, syndromes,
						  errloc);
	Py_DECREF(po_syndromes);

	if (nerr < 0) {
		if (nerr == -EINVAL) {
			PyErr_SetString(PyExc_ValueError,
				"invalid parameters");
			goto cleanup;
		} else if (nerr == -EBADMSG) {
			nerr = -1;
		} else {
			goto cleanup;
		}
	}

	for (int i = 0; i < nerr; i++) {
		unsigned int bitnum = errloc[i];
		if (bitnum >= data.len*8 + self->bch->ecc_bytes*8) {
			PyErr_SetString(PyExc_IndexError,
				"uncorrectable error");
			goto cleanup;
		}
		if (bitnum < data.len*8)
			result_data[bitnum/8] ^= 1 << (bitnum & 7);
	}

	if (self->reversed) {
		reverse_bytes(result_data, result_data, data.len);
	}

	/* TODO: eliminate extra copies created by PyByteArray_FromStringAndSize */
	result = PyTuple_New(2);
	PyTuple_SetItem(result, 0, PyLong_FromLong(nerr));
	PyTuple_SetItem(result, 1,
		PyByteArray_FromStringAndSize((char *)result_data, data.len));

cleanup:
	free(result_data);
	PyBuffer_Release(&data);
	return result;
}

static PyObject *
BCH_compute_even_syndromes(BCHObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *po_syndromes;
	static char *kwlist[] = {"syndromes", NULL};
	PyObject *result = NULL;
	PyObject *value = NULL;
	unsigned int *syndromes = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist,
			&po_syndromes))
		return NULL;

	Py_INCREF(po_syndromes);

	if (!PySequence_Check(po_syndromes)) {
		PyErr_SetString(PyExc_TypeError,
			"'syndromes' must be a sequence type");
		goto cleanup;
	}

	if (PySequence_Length(po_syndromes) != self->bch->t*2) {
		PyErr_Format(PyExc_ValueError,
			"'syndromes' should have length of %d",
			self->bch->t*2);
		goto cleanup;
	}

	syndromes = malloc(self->bch->t*2*sizeof(*syndromes));
	for (unsigned int i = 0; i < self->bch->t*2; i++) {
		value = PySequence_GetItem(po_syndromes, i);
		Py_INCREF(value);
		long tmp = PyLong_AsLong(value);
		if (tmp == -1 && PyErr_Occurred()) {
			Py_DECREF(value);
			goto cleanup;
		}
		syndromes[i] = tmp;
		Py_DECREF(value);
	}

	compute_even_syndromes(self->bch, syndromes);

	result = PyTuple_New(self->bch->t*2);
	for (unsigned int i = 0; i < self->bch->t*2; i++) {
		value = PyLong_FromLong(syndromes[i]);
		PyTuple_SetItem(result, i, value);
	}

cleanup:
	if (syndromes) {
		free(syndromes);
	}
	Py_DECREF(po_syndromes);
	return result;	
}

static PyObject *
BCH_getattr(BCHObject *self, PyObject *name)
{
	PyObject *value;
	PyObject *result = NULL;
	unsigned int i;

	Py_INCREF(name);
#if PY_MAJOR_VERSION >= 3
	const char *cname = PyUnicode_AsUTF8(name);
#else
	const char *cname = PyString_AsString(name);
#endif

	if (strcmp(cname, "syndromes") == 0) {
		if (self->bch->syn) {
			result = PyTuple_New(self->bch->t*2);
			for (i = 0; i < self->bch->t*2; i++) {
				value = PyLong_FromLong(self->bch->syn[i]);
				PyTuple_SetItem(result, i, value);
			}
		}
		else {
			result = Py_None;
			Py_INCREF(result);
		}
	} else if (strcmp(cname, "ecc_bits") == 0) {
		result = PyLong_FromLong(self->bch->ecc_bits);
	} else if (strcmp(cname, "ecc_bytes") == 0) {
		result = PyLong_FromLong(self->bch->ecc_bytes);
	} else if (strcmp(cname, "m") == 0) {
		result = PyLong_FromLong(self->bch->m);
	} else if (strcmp(cname, "n") == 0) {
		result = PyLong_FromLong(self->bch->n);
	} else if (strcmp(cname, "t") == 0) {
		result = PyLong_FromLong(self->bch->t);
	} else {
		result = PyObject_GenericGetAttr((PyObject *)self, name);
	}

	Py_DECREF(name);
	return result;
}

static PyMemberDef BCH_members[] = {
	{"ecc_size", -1, 0, READONLY|RESTRICTED, NULL},
	{"syndromes", -1, 0, READONLY|RESTRICTED, NULL},
	{"t", -1, 0, READONLY|RESTRICTED, NULL},
	{NULL}
};

static PyMethodDef BCH_methods[] = {
	{"encode", (PyCFunction)BCH_encode, METH_VARARGS, NULL},
	{"decode", (PyCFunction)BCH_decode, METH_VARARGS, NULL},
	{"decode_inplace", (PyCFunction)BCH_decode_inplace, METH_VARARGS, NULL},
	{"decode_syndromes", (PyCFunction)BCH_decode_syndromes, METH_VARARGS, NULL},
	{"compute_even_syndromes", (PyCFunction)BCH_compute_even_syndromes,
		METH_VARARGS, NULL},
	{NULL}
};

static PyTypeObject BCHType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name      = "bchlib.BCH",
  .tp_basicsize = sizeof(BCHObject),
  .tp_dealloc   = (destructor)BCH_dealloc,
  .tp_getattro  = (getattrofunc)BCH_getattr,
  .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc       = "BCH Encoder/Decoder",
  .tp_methods   = BCH_methods,
  .tp_members   = BCH_members,
  .tp_init      = (initproc)BCH_init,
  .tp_new       = PyType_GenericNew,
};

static PyMethodDef module_methods[] = {
    {NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	"BCH",
	"BCH Library",
	-1,
	module_methods,
	NULL,
	NULL,
	NULL,
	NULL,
};
#endif

static PyObject *
moduleinit(void)
{
	PyObject *m;

	if (PyType_Ready(&BCHType) < 0)
		return NULL;
#if PY_MAJOR_VERSION >= 3
	m = PyModule_Create(&moduledef);
#else
	m = Py_InitModule3("bchlib", module_methods, NULL);
#endif
	if (m == NULL)
		return NULL;

	Py_INCREF(&BCHType);
	PyModule_AddObject(m, "BCH", (PyObject *)&BCHType);

	return m;
}

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif
#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC
PyInit_bchlib(void)
{
	return moduleinit();
}
#else
PyMODINIT_FUNC
initbchlib(void)
{
	moduleinit();
}
#endif