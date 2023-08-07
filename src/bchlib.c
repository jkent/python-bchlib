/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Python C module for BCH encoding/decoding.
 *
 * Copyright (c) 2013, 2017-2018, 2021 Jeff Kent <jeff@jkent.net>
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


#if (defined(_WIN32) || defined(_WIN64)) && !defined(__WINDOWS__)
# define alloca(size) _alloca(size)
#endif

typedef struct {
    PyObject_HEAD
    struct bch_control *bch;
    uint8_t *ecc;
    unsigned int msg_len;
    unsigned int *errloc;
    int nerr;
} BCHObject;

static void
BCH_dealloc(BCHObject *self)
{
    if (self->bch) {
        bch_free(self->bch);
        self->bch = NULL;
    }

    if (self->ecc) {
        free(self->ecc);
        self->ecc = NULL;
    }

    if (self->errloc) {
        free(self->errloc);
        self->errloc = NULL;
    }

    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int
BCH_init(BCHObject *self, PyObject *args, PyObject *kwds)
{
    int t, m = -1;
    unsigned int poly = 0;
    bool swap_bits = false;

    static char *kwlist[] = {"t", "poly", "m", "swap_bits", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i|Iip", kwlist, &t, &poly,
            &m, &swap_bits)) {
        return -1;
    }

    if (m == -1 && poly == 0) {
        PyErr_SetString(PyExc_ValueError,
                "'m' and/or 'poly' must be provided");
        return -1;
    }

    if (m == -1) {
        unsigned int tmp = poly;
        m = 0;
        while (tmp >>= 1) {
            m++;
        }
    }

    self->bch = bch_init(m, t, poly, swap_bits);
    if (!self->bch) {
        PyErr_SetString(PyExc_RuntimeError,
                "unable to inititalize bch, invalid parameters?");
        return -1;
    }

    return 0;
}

static PyObject *
BCH_encode(BCHObject *self, PyObject *args, PyObject *kwds)
{
    Py_buffer data = {0};
    Py_buffer ecc = {0};

    static char *kwlist[] = {"data", "ecc", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*|y*", kwlist, &data,
            &ecc)) {
        return NULL;
    }

    if (!self->ecc) {
        self->ecc = malloc(self->bch->ecc_bytes);
        if (!self->ecc) {
            return NULL;
        }
    }

    if (ecc.buf) {
        if (ecc.len != self->bch->ecc_bytes) {
            PyErr_Format(PyExc_ValueError, "ecc length must be %d bytes",
                self->bch->ecc_bytes);
            return NULL;
        }
        memcpy(self->ecc, ecc.buf, self->bch->ecc_bytes);
    } else {
        memset(self->ecc, 0, self->bch->ecc_bytes);
    }

    bch_encode(self->bch, (uint8_t *) data.buf, (unsigned int) data.len,
            self->ecc);
    Py_RETURN_NONE;
}

static PyObject *
BCH_decode(BCHObject *self, PyObject *args, PyObject *kwds)
{
    Py_buffer data = {0};
    Py_buffer recv_ecc = {0};
    Py_buffer calc_ecc = {0};
    PyObject *syn = NULL;
    int msg_len = 0;

    static char *kwlist[] = {"data", "recv_ecc", "calc_ecc", "syn", "msg_len",
            NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|y*y*y*Oi", kwlist, &data,
            &recv_ecc, &calc_ecc, &syn, &msg_len)) {
        return NULL;
    }

    if (msg_len > 0) {
        self->msg_len = msg_len;
    } else {
        self->msg_len += data.len;
    }

    if (recv_ecc.buf && recv_ecc.len != self->bch->ecc_bytes) {
        PyErr_Format(PyExc_ValueError, "recv_ecc length should be %d bytes",
                self->bch->ecc_bytes);
        return NULL;
    }

    if (calc_ecc.buf && calc_ecc.len != self->bch->ecc_bytes) {
        PyErr_Format(PyExc_ValueError, "calc_ecc length should be %d bytes",
            self->bch->ecc_bytes);
        return NULL;
    }

    if (syn) {
        Py_INCREF(syn);

        if (!PySequence_Check(syn)) {
            PyErr_SetString(PyExc_TypeError, "'syn' must be a sequence type");
            Py_DECREF(syn);
            return NULL;
        }

        if (PySequence_Length(syn) != self->bch->t * 2) {
            PyErr_Format(PyExc_ValueError, "'syn' must have %d elements",
                    self->bch->t * 2);
            Py_DECREF(syn);
            return NULL;
        }

        for (unsigned int i = 0; i < self->bch->t * 2; i++) {
            PyObject *value = PySequence_GetItem(syn, i);
            Py_INCREF(value);
            long ltmp = PyLong_AsLong(value);
            if (ltmp == -1 && PyErr_Occurred()) {
                Py_DECREF(value);
                Py_DECREF(syn);
                return NULL;
            }
            self->bch->syn[i] = ltmp;
            Py_DECREF(value);
        }

        Py_DECREF(syn);
    }

    if (!self->errloc) {
        self->errloc = malloc(sizeof(unsigned int) * self->bch->t);
        if (!self->errloc) {
            return NULL;
        }
    }

    self->nerr = bch_decode(self->bch, data.buf,
            (unsigned int) msg_len ? msg_len : data.len,
            recv_ecc.buf, calc_ecc.buf, syn ? self->bch->syn : NULL,
            self->errloc);

    if (self->nerr < 0) {
        if (self->nerr == -EINVAL) {
            PyErr_SetString(PyExc_ValueError, "invalid parameters");
            return NULL;
        } else if (self->nerr == -EBADMSG) {
            self->nerr = -1;
        } else {
            return NULL;
        }
    }

    return PyLong_FromLong(self->nerr);
}

static PyObject *
BCH_correct(BCHObject *self, PyObject *args, PyObject *kwds)
{
    Py_buffer data = {0};
    Py_buffer ecc = {0};
    PyObject *result = NULL;

    static char *kwlist[] = {"data", "ecc", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "y*|y*", kwlist, &data,
            &ecc)) {
        goto cleanup;
    }

    if (self->nerr < 0) {
        goto cleanup;
    }

    if (data.readonly) {
        PyErr_SetString(PyExc_ValueError, "data cannot be readonly");
        goto cleanup;
    }

    if (ecc.readonly) {
        PyErr_SetString(PyExc_ValueError, "ecc cannot be readonly");
        goto cleanup;
    }

    for (int i = 0; i < self->nerr; i++) {
        unsigned int bitnum = self->errloc[i];
        if (bitnum >= (self->msg_len + self->bch->ecc_bytes) * 8) {
            PyErr_SetString(PyExc_IndexError, "uncorrectable error");
            return NULL;
        }
        unsigned int byte = bitnum / 8;
        unsigned char bit = 1 << (bitnum & 7);
        if (byte < self->msg_len) {
            ((uint8_t *) data.buf)[byte] ^= bit;
        } else if (ecc.buf && !ecc.readonly) {
            ((uint8_t *) ecc.buf)[byte - self->msg_len] ^= bit;
        }
    }

    result = Py_None;
    Py_IncRef(Py_None);

cleanup:
    PyBuffer_Release(&data);
    PyBuffer_Release(&ecc);
    return result;
}

static PyObject *
BCH_compute_even_syn(BCHObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *syn;
    PyObject *result = NULL;

    static char *kwlist[] = {"syn", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &syn)) {
        return result;
    }

    Py_INCREF(syn);

    if (!PySequence_Check(syn)) {
        PyErr_SetString(PyExc_TypeError, "'syn' must be a sequence type");
        Py_DECREF(syn);
        return result;
    }

    if (PySequence_Length(syn) != self->bch->t * 2) {
        PyErr_Format(PyExc_ValueError, "'syn' must have %d elements",
                self->bch->t * 2);
        Py_DECREF(syn);
        return result;
    }

    for (unsigned int i = 0; i < self->bch->t * 2; i++) {
        PyObject *value = PySequence_GetItem(syn, i);
        Py_INCREF(value);
        long ltmp = PyLong_AsLong(value);
        if (ltmp == -1 && PyErr_Occurred()) {
            Py_DECREF(value);
            Py_DECREF(syn);
            return result;
        }
        self->bch->syn[i] = ltmp;
        Py_DECREF(value);
    }

    Py_DECREF(syn);

    bch_compute_even_syndromes(self->bch, self->bch->syn);

    result = PyTuple_New(self->bch->t * 2);
    for (unsigned int i = 0; i < self->bch->t * 2; i++) {
        PyObject *value = PyLong_FromLong(self->bch->syn[i]);
        PyTuple_SetItem(result, i, value);
    }

    return result;
}

static PyObject *
BCH_getattr(BCHObject *self, PyObject *name)
{
    PyObject *value;
    PyObject *result = NULL;

    Py_INCREF(name);
    const char *cname = PyUnicode_AsUTF8(name);

    if (strcmp(cname, "bits") == 0) {
        result = PyLong_FromLong(self->bch->ecc_bits);
    } else if (strcmp(cname, "bytes") == 0) {
        result = PyLong_FromLong(self->bch->ecc_bytes);
    } else if (strcmp(cname, "ecc") == 0) {
        result = PyBytes_FromStringAndSize((const char *) self->ecc,
                self->bch->ecc_bytes);
    } else if (strcmp(cname, "errloc") == 0) {
        result = PyTuple_New(self->bch->t);
        for (int i = 0; i < self->nerr; i++) {
            value = PyLong_FromLong(self->errloc[i]);
            PyTuple_SetItem(result, i, value);
        }
    } else if (strcmp(cname, "m") == 0) {
        result = PyLong_FromLong(self->bch->m);
    } else if (strcmp(cname, "n") == 0) {
        result = PyLong_FromLong(self->bch->n);
    } else if (strcmp(cname, "syn") == 0) {
        if (self->bch->syn) {
            result = PyTuple_New(self->bch->t * 2);
            for (unsigned int i = 0; i < self->bch->t * 2; i++) {
                value = PyLong_FromLong(self->bch->syn[i]);
                PyTuple_SetItem(result, i, value);
            }
        }
        else {
            result = Py_None;
            Py_INCREF(result);
        }
    } else if (strcmp(cname, "t") == 0) {
        result = PyLong_FromLong(self->bch->t);
    } else {
        result = PyObject_GenericGetAttr((PyObject *)self, name);
    }

    Py_DECREF(name);
    return result;
}

static PyMemberDef BCH_members[] = {
    {"bits", -1, 0, READONLY|RESTRICTED},
    {"bytes", -1, 0, READONLY|RESTRICTED},
    {"ecc", -1, 0, READONLY|RESTRICTED},
    {"errloc", -1, 0, READONLY|RESTRICTED},
    {"m", -1, 0, READONLY|RESTRICTED},
    {"msg_len", T_UINT, offsetof(BCHObject, msg_len), 0},
    {"n", -1, 0, READONLY|RESTRICTED},
    {"nerr", T_INT, offsetof(BCHObject, nerr), READONLY},
    {"syn", -1, 0, READONLY|RESTRICTED},
    {"t", -1, 0, READONLY|RESTRICTED},
    {NULL}
};

static PyMethodDef BCH_methods[] = {
    {"encode", (PyCFunction) BCH_encode, METH_VARARGS | METH_KEYWORDS, NULL},
    {"decode", (PyCFunction) BCH_decode, METH_VARARGS | METH_KEYWORDS, NULL},
    {"correct", (PyCFunction) BCH_correct, METH_VARARGS | METH_KEYWORDS, NULL},
    {"compute_even_syn", (PyCFunction) BCH_compute_even_syn,
            METH_VARARGS | METH_KEYWORDS, NULL},
    {NULL}
};

static PyTypeObject BCHType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "bchlib.BCH",
    .tp_basicsize = sizeof(BCHObject),
    .tp_dealloc   = (destructor) BCH_dealloc,
    .tp_getattro  = (getattrofunc) BCH_getattr,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "BCH Encoder/Decoder",
    .tp_methods   = BCH_methods,
    .tp_members   = BCH_members,
    .tp_init      = (initproc) BCH_init,
    .tp_new       = PyType_GenericNew,
};

static PyMethodDef module_methods[] = {
    {NULL}
};

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

PyMODINIT_FUNC
PyInit_bchlib(void)
{
    PyObject *m;

    if (PyType_Ready(&BCHType) < 0)
        return NULL;
    m = PyModule_Create(&moduledef);
    if (m == NULL)
        return NULL;

    Py_INCREF(&BCHType);
    PyModule_AddObject(m, "BCH", (PyObject *)&BCHType);

    return m;
}
