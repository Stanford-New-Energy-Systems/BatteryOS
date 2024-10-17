#ifndef SONNEN_BATTERY_HPP
#define SONNEN_BATTERY_HPP

#include "BatteryInterface.hpp"
#include <curl/curl.h>
#include <Python.h>
class SonnenBattery : public PhysicalBattery {
    // int serial;
    int64_t max_capacity_Wh;
    int64_t max_discharging_current_mA; 
    int64_t max_charging_current_mA; 
    PyObject *pf_new_sonnen;
    PyObject *pf_quit_sonnen;
    PyObject *pf_sonnen_get_status;
    PyObject *pf_sonnen_set_current;

    PyObject *sonnen;
    
public: 
    SonnenBattery(
        const std::string &name, 
        int serial, 
        int64_t max_capacity_Wh, 
        int64_t max_discharging_current_mA, 
        int64_t max_charging_current_mA, 
        std::chrono::milliseconds max_staleness
    ) : 
        PhysicalBattery(name, max_staleness), 
        // serial(serial),
        max_capacity_Wh(max_capacity_Wh), 
        max_discharging_current_mA(max_discharging_current_mA), 
        max_charging_current_mA(max_charging_current_mA)
    {
        PyGILState_STATE gstate; 
        gstate = PyGILState_Ensure();
        PyObject *pname = PyUnicode_DecodeFSDefault("Sonnen");
        if (pname == nullptr) {
            ERROR() << "failed to decode string";
        }
        PyObject *pmodule = PyImport_Import(pname);
        Py_DECREF(pname);
        if (pmodule == nullptr) {
            ERROR() << "Sonnen.py not found";
        }
        Py_DECREF(pmodule);

        pf_new_sonnen = PyObject_GetAttrString(pmodule, "new_sonnen");
        pf_quit_sonnen = PyObject_GetAttrString(pmodule, "quit_sonnen");
        pf_sonnen_get_status = PyObject_GetAttrString(pmodule, "sonnen_get_status");
        pf_sonnen_set_current = PyObject_GetAttrString(pmodule, "sonnen_set_current");

        PyObject *py_serial_num = PyLong_FromLong(serial);

        this->sonnen = PyObject_CallFunctionObjArgs(pf_new_sonnen, py_serial_num, NULL);
        Py_DECREF(py_serial_num);
        if (!this->sonnen) {
            ERROR() << "Sonnen failed to initialize";
        }
        PyGILState_Release(gstate);
    }

    ~SonnenBattery() {
        PyGILState_STATE gstate;
        gstate = PyGILState_Ensure();
        if (this->sonnen) {
            // set sonnen back to self-consumption mode 
            // PyObject_CallFunctionObjArgs(this->pf_quit_sonnen, this->sonnen, NULL);
        }
        Py_DECREF(this->sonnen);
        Py_DECREF(this->pf_new_sonnen);
        Py_DECREF(this->pf_quit_sonnen);
        Py_DECREF(this->pf_sonnen_get_status);
        Py_DECREF(this->pf_sonnen_set_current);
        PyGILState_Release(gstate);
    }

    std::string get_type_string() override {
        return "SonnenBattery";
    }

    BatteryStatus refresh() override {
        PyGILState_STATE gstate; 
        gstate = PyGILState_Ensure();
        PyObject *wh = PyLong_FromLong(this->max_capacity_Wh);
        PyObject *mcc = PyLong_FromLong(this->max_charging_current_mA);
        PyObject *mdc = PyLong_FromLong(this->max_discharging_current_mA);
        PyObject *tup = PyObject_CallFunctionObjArgs(pf_sonnen_get_status, this->sonnen, wh, mcc, mdc, NULL);
        Py_DECREF(wh);
        Py_DECREF(mcc);
        Py_DECREF(mdc); 

        PyObject *volt = PyTuple_GetItem(tup, 0);
        PyObject *curr = PyTuple_GetItem(tup, 1);
        PyObject *cap = PyTuple_GetItem(tup, 2);
        PyObject *max_cap = PyTuple_GetItem(tup, 3);
        PyObject *mcc2 = PyTuple_GetItem(tup, 4);
        PyObject *mdc2 = PyTuple_GetItem(tup, 5);
        PyObject *ts_s = PyTuple_GetItem(tup, 6);
        PyObject *ts_ms = PyTuple_GetItem(tup, 7);

        BatteryStatus status; 
        status.voltage_mV = PyLong_AsLong(volt);
        status.current_mA = PyLong_AsLong(curr);
        status.capacity_mAh = PyLong_AsLong(cap);
        status.max_capacity_mAh = PyLong_AsLong(max_cap);
        status.max_charging_current_mA = PyLong_AsLong(mcc2);
        status.max_discharging_current_mA = PyLong_AsLong(mdc2);
        status.timestamp.secs_since_epoch = PyLong_AsLong(ts_s);
        status.timestamp.msec = PyLong_AsLong(ts_ms);
        
        this->status = status;
        Py_DECREF(volt);
        Py_DECREF(curr);
        Py_DECREF(cap);
        Py_DECREF(max_cap);
        Py_DECREF(mcc2);
        Py_DECREF(mdc2);
        Py_DECREF(ts_s);
        Py_DECREF(ts_ms);
        Py_DECREF(tup);
        PyGILState_Release(gstate);
        std::cout << "release" << PyGILState_Check() << std::endl;
        return this->status;
    }

    uint32_t set_current(int64_t target_current_mA, bool is_greater_than_target, void *other_data) override {
        std::cout << "acquiring" << PyGILState_Check() << std::endl;
        PyGILState_STATE gstate;
        gstate = PyGILState_Ensure();
        std::cout << "acquired" << PyGILState_Check() << std::endl;
        std::cout << "set_current: " << target_current_mA << "mA" << std::endl;
        PyObject *target_mA = PyLong_FromLong((long)target_current_mA);
        PyObject *success = PyObject_CallFunctionObjArgs(pf_sonnen_set_current, this->sonnen, target_mA, NULL);
        long succ = PyLong_AsLong(success);
        Py_DECREF(target_mA);
        Py_DECREF(success);
        PyGILState_Release(gstate);
        std::cout << "release" << PyGILState_Check() << std::endl;
        return succ;
    }
};

#endif // ! SONNEN_BATTERY_HPP




