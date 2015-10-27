/*
 * Python C module for BCH encoding/correction.
 *
 * Copyright © 2013 Jeff Kent <jeff@jkent.net>
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
	PyObject_HEAD;
	struct bch_control *bch;
	bool reversed;
} BchObject;

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
Bch_dealloc(BchObject *self)
{
	if (self->bch)
		free_bch(self->bch);

	self->ob_type->tp_free((PyObject *)self);
}

static int
Bch_init(BchObject *self, PyObject *args, PyObject *kwds)
{
	unsigned int prim_poly;
	int t;
	PyObject *reversed = NULL;
	static char *kwlist[] = {"prim_poly", "t", "reverse", NULL};
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
Bch_encode(BchObject *self, PyObject *args, PyObject *kwds)
{
	const uint8_t *rdata;
	unsigned int data_len;
	const uint8_t *previous_ecc = NULL;
	unsigned int previous_ecc_len;
	static char *kwlist[] = {"data", "ecc", NULL};
	uint8_t *data;
	uint8_t ecc[self->bch->ecc_bytes];

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s#|z#", kwlist,
			&rdata, &data_len, &previous_ecc, &previous_ecc_len));

	if (previous_ecc) {
		if (previous_ecc_len != self->bch->ecc_bytes) {
			PyErr_Format(PyExc_ValueError,
				"ECC length should be %d bytes",
				self->bch->ecc_bytes);
			return NULL;
		}
		memcpy(ecc, previous_ecc, self->bch->ecc_bytes);
	} else {
		memset(ecc, 0, self->bch->ecc_bytes);
	}

	if (self->reversed) {
		data = malloc(data_len);
		reverse_bytes(data, rdata, data_len);
		encode_bch(self->bch, data, data_len, ecc);
		free(data);
	}
	else {
		encode_bch(self->bch, rdata, data_len, ecc);
	}

	return PyString_FromStringAndSize((char *)ecc, self->bch->ecc_bytes);
}

static PyObject *
Bch_correct(BchObject *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {"data", "ecc", "syn", NULL};
	const uint8_t *rdata;
	const uint8_t *ecc = NULL;
	unsigned int data_len, ecc_len;
	PyObject *po_syn = NULL;
	unsigned int errloc[self->bch->t];
	int nerr = -1;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s#|z#O", kwlist,
			&rdata, &data_len, &ecc, &ecc_len, &po_syn))
		return NULL;

	if ((ecc && po_syn) || (!ecc && !po_syn)) {
		PyErr_SetString(PyExc_TypeError,
			"only one of 'ecc' or 'syn' permitted");
		return NULL;
	}

	if (po_syn) {
		if (!PySequence_Check(po_syn)) {
			PyErr_SetString(PyExc_TypeError,
				"'syn' must be a sequence type");
			return NULL;
		}

		if (PySequence_Length(po_syn) != self->bch->t*2) {
			PyErr_Format(PyExc_ValueError,
				"'syn' must have %d elements",
				self->bch->t*2);
			return NULL;
		}

		Py_INCREF(po_syn);
		unsigned int syn[self->bch->t];
		for (int i = 0; i < self->bch->t*2; i++) {
			PyObject *value = PySequence_GetItem(po_syn, i);
			Py_INCREF(value);
			long ltmp = PyInt_AsLong(value);
			if (ltmp == -1 && PyErr_Occurred()) {
				Py_DECREF(value);
				return NULL;
			}
			syn[i] = ltmp;
			Py_DECREF(value);
		}

		nerr = decode_bch(self->bch, NULL, data_len, NULL, NULL, syn,
				errloc);
		Py_DECREF(po_syn);
	}

	PyObject *result = NULL;
	uint8_t *data = malloc(data_len);

	if (self->reversed)
		reverse_bytes(data, rdata, data_len);
	else
		memcpy(data, rdata, data_len);

	if (ecc)
		nerr = decode_bch(self->bch, data, data_len, ecc, NULL,
				NULL, errloc);
	
	if (nerr < 0) {
		if (nerr == -EINVAL)
			PyErr_SetString(PyExc_ValueError,
				"invalid parameters");
		if (nerr == -EBADMSG)
			PyErr_SetString(PyExc_OverflowError,
				"too many errors or bad message");
		goto cleanup;
	}

	while (nerr--) {
		unsigned int bitnum = errloc[nerr];
		if (bitnum >= data_len*8 + ecc_len*8) {
			PyErr_SetString(PyExc_IndexError,
				"uncorrectable error");
			goto cleanup;
		}
		if (bitnum < data_len*8)
			data[bitnum/8] ^= 1 << (bitnum & 7);
		//else
		//	ecc[bitnum/8 - data_len] ^= 1 << (bitnum & 7);
	}

	if (self->reversed)
		reverse_bytes(data, data, data_len);

	result = PyString_FromStringAndSize((char *)data, data_len);

cleanup:
	free(data);
	return result;
}

static PyObject *
Bch_calc_even_syndrome(BchObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *po_syn;
	static char *kwlist[] = {"syn", NULL};
	PyObject *result = NULL;
	PyObject *value = NULL;
	unsigned int *syn = NULL;
	long tmp;
	int i;


	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist,
			&po_syn))
		return NULL;

	Py_INCREF(po_syn);

	if (!PySequence_Check(po_syn)) {
		PyErr_SetString(PyExc_TypeError,
			"'syn' must be a sequence type");
		goto cleanup;
	}

	if (PySequence_Length(po_syn) != self->bch->t*2) {
		PyErr_Format(PyExc_ValueError,
			"'syn' should have length of %d",
			self->bch->t*2);
		goto cleanup;
	}

	syn = malloc(self->bch->t*2*sizeof(*syn));
	for (i = 0; i < self->bch->t*2; i++) {
		value = PySequence_GetItem(po_syn, i);
		Py_INCREF(value);
		tmp = PyInt_AsLong(value);
		if (tmp == -1 && PyErr_Occurred()) {
			Py_DECREF(value);
			goto cleanup;
		}
		syn[i] = tmp;
		Py_DECREF(value);
	}

	compute_even_syndromes(self->bch, syn);

	result = PyTuple_New(self->bch->t*2);
	for (i = 0; i < self->bch->t*2; i++) {
		value = PyInt_FromLong(syn[i]);
		PyTuple_SetItem(result, i, value);
	}

cleanup:
	if (syn)
		free(syn);
	Py_DECREF(po_syn);
	return result;	
}

static PyObject *
Bch_getattr(BchObject *self, PyObject *name)
{
	PyObject *value;
	PyObject *result = NULL;
	int i;

	Py_INCREF(name);
	const char *cname = PyString_AsString(name);

	if (strcmp(cname, "syndrome") == 0) {
		if (self->bch->syn) {
			result = PyTuple_New(self->bch->t*2);
			for (i = 0; i < self->bch->t*2; i++) {
				value = PyInt_FromLong(self->bch->syn[i]);
				PyTuple_SetItem(result, i, value);
			}
		}
		else {
			result = Py_None;
			Py_INCREF(result);
		}
	}
	else {
		result = PyObject_GenericGetAttr((PyObject *)self, name);
	}

	Py_DECREF(name);
	return result;
}

static PyMemberDef Bch_members[] = {
	{"syndrome", -1, 0, RO|RESTRICTED, NULL},
	{NULL}
};

static PyMethodDef Bch_methods[] = {
	{"encode", (PyCFunction)Bch_encode, METH_KEYWORDS, NULL},
	{"correct", (PyCFunction)Bch_correct, METH_KEYWORDS, NULL},
	{"calc_even_syndrome", (PyCFunction)Bch_calc_even_syndrome, METH_KEYWORDS, NULL},
	{NULL}
};

static PyTypeObject BchType = {
  PyObject_HEAD_INIT(NULL)
  .tp_name      = "bch.bch",
  .tp_basicsize = sizeof(BchObject),
  .tp_dealloc   = (destructor)Bch_dealloc,
  .tp_getattro  = (getattrofunc)Bch_getattr,
  .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc       = "BCH Encoder/Decoder",
  .tp_methods   = Bch_methods,
  .tp_members   = Bch_members,
  .tp_init      = (initproc)Bch_init,
  .tp_new	= PyType_GenericNew,
};

static PyMethodDef module_methods[] = {
    {NULL}
};

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initbchlib(void)
{
	PyObject *m;

	if (PyType_Ready(&BchType) < 0)
		return;

	m = Py_InitModule3("bchlib", module_methods, NULL);
	if (m == NULL)
		return;

	Py_INCREF(&BchType);
	PyModule_AddObject(m, "bch", (PyObject *)&BchType);
}

