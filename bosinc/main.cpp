#include "JBDBMS.hpp"
#include <iostream>

void test_uart() {
    printf("--------------------\n");
    UARTConnection connection("/dev/ttyUSB1");
    connection.connect();

    usleep(100000);
    connection.write({'W', 'a', 'k', 'e'});
    usleep(100000);
    connection.flush();
    usleep(100000);
    connection.write({0xdd, 0xa5, 0x03, 0x00, 0xff, 0xfd, 0x77});
    usleep(100000);
    std::vector<uint8_t> buf = connection.read(28);
    
    printf("Rx data: \n");
    Util::print_buffer(buf.data(), buf.size());
    printf("\nRx data end\n");
}

void test_python_binding() {
    std::string addr = "/dev/ttyUSB2";
    RD6006PowerSupply rd6006(addr);
    rd6006.enable();
    rd6006.set_current_Amps(123.456);

    printf("%f\n", rd6006.get_current_Amps());
    
    std::string addr2 = "/dev/ttyUSB2";
    RD6006PowerSupply rd60062(addr2);
    rd60062.enable();
    rd60062.set_current_Amps(456.123);
}

void test_JBDBMS() {
    JBDBMS bms("jbd", "/dev/ttyUSB1", "current_regulator");
    JBDBMS::State basic_info = bms.get_basic_info();
    JBDBMS::print_basic_state_to_stream(std::cout, basic_info);
    Util::print_battery_status_to_stream(std::cout, bms.get_status());
}

int run() {
    test_battery_status();
    test_python_binding();
    // test_uart();
    // test_JBDBMS(); 

    return 0;
}

int main() {
    Py_Initialize();

    // add the current path to the module search path!
    PyObject *sys = PyImport_ImportModule("sys");
    PyObject *path = PyObject_GetAttrString(sys, "path");
    PyList_Append(path, PyUnicode_FromString("./python"));
    Py_DECREF(path);
    Py_DECREF(sys);

    run();
    
    Py_FinalizeEx();

    return 0;
    
}


