#ifndef PY_SSIZE_T_CLEAN
#define PY_SSIZE_T_CLEAN
#endif 
#include <Python.h>
#include <string>
class RD6006PowerSupply {
private: 
    PyObject *p_device;
    PyObject *pf_create;
    PyObject *pf_enable;
    PyObject *pf_disable;
    PyObject *pf_set_current;
    PyObject *pf_get_current;
public: 
    RD6006PowerSupply(const std::string &address) {
        PyObject *p_name = PyUnicode_DecodeFSDefault("rd6006_c");
        if (p_name == 0) {
            fprintf(stderr, "ERROR! p_name == 0\n");
        }
        PyObject *p_module = PyImport_Import(p_name);
        Py_DECREF(p_name);
        if (p_module == 0) {
            fprintf(stderr, "ERROR! p_module == 0\n");
        }
        printf("p_module = %p\n", (p_module));
        Py_DECREF(p_module);

        pf_create = PyObject_GetAttrString(p_module, "rd6006_create");
        pf_enable = PyObject_GetAttrString(p_module, "rd6006_enable");
        pf_disable = PyObject_GetAttrString(p_module, "rd6006_disable");
        pf_set_current = PyObject_GetAttrString(p_module, "rd6006_set_current");
        pf_get_current = PyObject_GetAttrString(p_module, "rd6006_get_current");

        PyObject *p_address = PyUnicode_FromString(address.c_str());
        // p_device = PyObject_CallOneArg(pf_create, p_address);
        p_device = PyObject_CallFunctionObjArgs(pf_create, p_address, NULL);
        if (p_device == 0) {
            fprintf(stderr, "ERROR! p_device == 0\n");
        }
        Py_DECREF(p_address);
    }
    ~RD6006PowerSupply() {
        if (p_device) {
            close();
        }
    }
    void close() {
        disable();
        Py_DECREF(p_device);
        Py_DECREF(pf_create);
        Py_DECREF(pf_enable);
        Py_DECREF(pf_disable);
        Py_DECREF(pf_set_current);
        Py_DECREF(pf_get_current);
        p_device = nullptr;
    }
    void enable() {
        // NOTE: do not decref the True False None etc literals!
        // PyObject_CallOneArg(pf_enable, p_device);
        PyObject_CallFunctionObjArgs(pf_enable, p_device, NULL);
    }
    void disable() {
        // PyObject_CallOneArg(pf_disable, p_device);
        PyObject_CallFunctionObjArgs(pf_disable, p_device, NULL);
    }
    void set_current_Amps(double current_A) {
        PyObject *p_current = PyFloat_FromDouble(current_A);
        PyObject_CallFunctionObjArgs(pf_set_current, p_device, p_current, NULL);
        Py_DECREF(p_current);
    }
    double get_current_Amps() {
        // PyObject *p_current = PyObject_CallOneArg(pf_get_current, p_device);
        PyObject *p_current = PyObject_CallFunctionObjArgs(pf_get_current, p_device, NULL);
        double current = PyFloat_AsDouble(p_current);
        Py_DECREF(p_current);
        return current;
    }
};













