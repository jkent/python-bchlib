/* SPDX-License-Identifier: GPL-2.0-only */
/* Python C module for BCH encoding/decoding/correcting. */

#include <Python.h>
#include <structmember.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "bch.h"


#if (defined(_WIN32) || defined(_WIN64)) && !defined(__WINDOWS__)
# define alloca(size) _alloca(size)
# include <malloc.h>
#else
# include <alloca.h>
#endif


typedef struct {
    PyObject_HEAD
    struct bch_control *bch;
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
    unsigned int prim_poly = 0;
    int swap_bits = false;

    static char *kwlist[] = {"t", "prim_poly", "m", "swap_bits", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i|Iip", kwlist, &t,
            &prim_poly, &m, &swap_bits)) {
        return -1;
    }

    if (m == -1 && prim_poly == 0) {
        PyErr_SetString(PyExc_ValueError,
                "'m' and/or 'poly' must be provided");
        return -1;
    }

    if (m == -1) {
        unsigned int tmp = prim_poly;
        m = 0;
        while (tmp >>= 1) {
            m++;
        }
    }

    self->bch = bch_init(m, t, prim_poly, swap_bits);
    if (!self->bch) {
        PyErr_SetString(PyExc_RuntimeError,
                "unable to inititalize bch, invalid parameters?");
        return -1;
    }

    self->errloc = calloc(1, sizeof(unsigned int) * self->bch->t);
    if (!self->errloc) {
        bch_free(self->bch);
        self->bch = NULL;
        PyErr_SetString(PyExc_MemoryError, "unable to allocate errloc buffer");
        return -1;
    }

    memset(self->bch->syn, 0, sizeof(unsigned int) * 2*self->bch->t);

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

    if (!ecc.buf) {
        ecc.len = self->bch->ecc_bytes;
        ecc.buf = alloca(ecc.len);
        memset(ecc.buf, 0, ecc.len);
    } else if (ecc.len != self->bch->ecc_bytes) {
        PyErr_Format(PyExc_ValueError, "ecc length must be %d bytes",
            self->bch->ecc_bytes);
        return NULL;
    }

    bch_encode(self->bch, (uint8_t *) data.buf, (unsigned int) data.len,
            ecc.buf);

    return PyBytes_FromStringAndSize((const char *) ecc.buf,
            self->bch->ecc_bytes);
}

static PyObject *
BCH_decode(BCHObject *self, PyObject *args, PyObject *kwds)
{
    Py_buffer data = {0};
    Py_buffer recv_ecc = {0};
    Py_buffer calc_ecc = {0};
    PyObject *syn = NULL;

    static char *kwlist[] = {"data", "recv_ecc", "calc_ecc", "syn", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|y*y*y*O", kwlist, &data,
            &recv_ecc, &calc_ecc, &syn)) {
        return NULL;
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

        if (PySequence_Length(syn) != 2*self->bch->t) {
            PyErr_Format(PyExc_ValueError, "'syn' must have %d elements",
                    2*self->bch->t);
            Py_DECREF(syn);
            return NULL;
        }

        for (unsigned int i = 0; i < 2*self->bch->t; i++) {
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

    self->nerr = bch_decode(self->bch, data.buf, data.len, recv_ecc.buf,
            calc_ecc.buf, syn ? self->bch->syn : NULL, self->errloc);

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

    if (data.readonly) {
        PyErr_SetString(PyExc_ValueError, "data cannot be readonly");
        goto cleanup;
    }

    if (ecc.readonly) {
        PyErr_SetString(PyExc_ValueError, "ecc cannot be readonly");
        goto cleanup;
    }

    if (self->nerr < 0) {
        goto done;
    }

    for (int i = 0; i < self->nerr; i++) {
        unsigned int bitnum = self->errloc[i];
        if (bitnum >= (data.len + self->bch->ecc_bytes) * 8) {
            PyErr_SetString(PyExc_IndexError, "uncorrectable error");
            return NULL;
        }
        unsigned int byte = bitnum / 8;
        unsigned char bit = 1 << (bitnum & 7);

        if (byte < data.len) {
            if (data.buf && !data.readonly) {
                ((uint8_t *) data.buf)[byte] ^= bit;
            }
        } else {
            if (ecc.buf && !ecc.readonly) {
                ((uint8_t *) ecc.buf)[byte - data.len] ^= bit;
            }
        }
    }

done:
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
    unsigned int *result_syn = alloca(sizeof(unsigned int) * 2 * self->bch->t);

    static char *kwlist[] = {"syn", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &syn)) {
        return result;
    }

    if (!PySequence_Check(syn)) {
        PyErr_SetString(PyExc_TypeError, "'syn' must be a sequence type");
        return result;
    }

    if (PySequence_Length(syn) != 2*self->bch->t) {
        PyErr_Format(PyExc_ValueError, "'syn' must have %d elements",
                2*self->bch->t);
        return result;
    }

    for (unsigned int i = 0; i < 2*self->bch->t; i++) {
        PyObject *value = PySequence_GetItem(syn, i);
        long tmp = PyLong_AsLong(value);
        if (tmp == -1 && PyErr_Occurred()) {
            return result;
        }
        result_syn[i] = tmp;
    }

    bch_compute_even_syndromes(self->bch, result_syn);

    result = PyTuple_New(2*self->bch->t);
    for (unsigned int i = 0; i < 2*self->bch->t; i++) {
        PyObject *value = PyLong_FromLong(result_syn[i]);
        PyTuple_SetItem(result, i, value);
    }

    return result;
}

static PyObject *
BCH_getattr(BCHObject *self, PyObject *name)
{
    PyObject *value;
    PyObject *result = NULL;

    if (!PyUnicode_Check(name)) {
        PyErr_Format(PyExc_TypeError,
                "attribute name must be a string, not '%.200s'",
                Py_TYPE(name)->tp_name);
        return NULL;
    }
    const char *cname = PyUnicode_AsUTF8(name);

    if (strcmp(cname, "ecc_bits") == 0) {
        result = PyLong_FromLong(self->bch->ecc_bits);
    } else if (strcmp(cname, "ecc_bytes") == 0) {
        result = PyLong_FromLong(self->bch->ecc_bytes);
    } else if (strcmp(cname, "errloc") == 0) {
        result = PyTuple_New(self->nerr <= 0 ? 0 : self->nerr);
        for (int i = 0; i < self->nerr; i++) {
            value = PyLong_FromLong(self->errloc[i]);
            PyTuple_SetItem(result, i, value);
        }
    } else if (strcmp(cname, "m") == 0) {
        result = PyLong_FromLong(self->bch->m);
    } else if (strcmp(cname, "n") == 0) {
        result = PyLong_FromLong(self->bch->n);
    } else if (strcmp(cname, "prim_poly") == 0) {
        result = PyLong_FromLong(self->bch->prim_poly);
    } else if (strcmp(cname, "syn") == 0) {
        if (self->bch->syn) {
            result = PyTuple_New(2*self->bch->t);
            for (unsigned int i = 0; i < 2*self->bch->t; i++) {
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

    return result;
}

static PyMemberDef BCH_members[] = {
    {"ecc_bits", -1, 0, READONLY|RESTRICTED,
            "Readonly; number of ecc bits."},
    {"ecc_bytes", -1, 0, READONLY|RESTRICTED,
            "Readonly; number of ecc bytes."},
    {"errloc", -1, 0, READONLY|RESTRICTED,
            "Readonly; tuple of error bit locations."},
    {"m", -1, 0, READONLY|RESTRICTED,
            "Readonly; Galois field order."},
    {"n", -1, 0, READONLY|RESTRICTED,
            "Readonly; maximum codeword size in bits."},
    {"prim_poly", -1, 0, READONLY|RESTRICTED,
            "Readonly; primitive polynomial for bch operations."},
    {"syn", -1, 0, READONLY|RESTRICTED,
            "Readonly; a tuple of syndromes after performing a decode()."},
    {"t", -1, 0, READONLY|RESTRICTED,
            "Readonly; the number of bit errors that can be corrected."},
    {NULL}
};

static PyMethodDef BCH_methods[] = {
    {"encode", (PyCFunction) BCH_encode, METH_VARARGS | METH_KEYWORDS, "\b\b\b\b"
            "encode(data[, ecc]) → ecc\n"
                "Encodes 'data' with an optional starting 'ecc'.  Returns the\n"
                "calculated ecc."},
    {"decode", (PyCFunction) BCH_decode, METH_VARARGS | METH_KEYWORDS, "\b\b\b\b"
            "decode(data=None, recv_ecc=None, calc_ecc=None, syn=None) → nerr\n"
                "Calculates error locations and returns the number of errors found\n"
                "or negative if decoding failed.\n\n"

                "There are four ways that 'decode' can function by providing\n"
                "different input parameters:\n\n"

                "    'data' and 'recv_ecc'\n"
                "    'recv_ecc' and 'calc_ecc'\n"
                "    'calc_ecc' (as recv_ecc XOR calc_ecc)\n"
                "    'syn' (a sequence of 2*t values)"},
    {"correct", (PyCFunction) BCH_correct, METH_VARARGS | METH_KEYWORDS, "\b\b\b\b"
            "correct(data=None, ecc=None) → None\n"
                "Corrects 'data' and 'ecc' if provided.  Buffers must not be\n"
                "readonly."},
    {"compute_even_syn", (PyCFunction) BCH_compute_even_syn,
            METH_VARARGS | METH_KEYWORDS, "\b\b\b\b"
            "compute_even_syn(syn) → syn\n"
                "Computes even syndromes from odd ones.  Takes a sequence of\n"
                "2*t values and returns a tuple of 2*t elements."},
    {NULL}
};

static PyTypeObject BCHType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "bchlib.BCH",
    .tp_basicsize = sizeof(BCHObject),
    .tp_dealloc   = (destructor) BCH_dealloc,
    .tp_getattro  = (getattrofunc) BCH_getattr,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       =
        "BCH Encoder/Decoder\n\n"

        "__init__(t, poly=None, m=None, swap_bits=False) → bch\n"
        "    Constructor creates a BCH object with given 't' bit strength.  At\n"
        "    least one of 'poly' and/or 'm' must be provided.  If 'poly' is\n"
        "    provided but 'm' (Galois field order) is not, 'm' will be\n"
        "    calculated automatically.  If 'm' between 5 and 15 inclusive is'\n"
        "    provided, 'polywill be selected automatically.  The 'swap_bits'\n"
        "    parameter will reverse the bit order within data and syndrome\n"
        "    bytes.",
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
