#include "JBDBMS.hpp"
#include "TestBattery.hpp"
#include "NetworkBattery.hpp"
#include "AggregatorBattery.hpp"
#include "SplitterPolicy.hpp"
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
    print_buffer(buf.data(), (size_t)buf.size());
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
    std::cout << basic_info << std::endl;
    std::cout << bms.get_status() << std::endl;
}

void test_events() {
    std::cout << std::endl << "---------------------- test events ---------------------------" << std::endl << std::endl;
    NullBattery nub("null_battery", 1200, std::chrono::milliseconds(1000));
    std::cout << "starting background refresh" << std::endl;
    nub.start_background_refresh();
    nub.schedule_set_current(1000, true, get_system_time()+std::chrono::seconds(3), get_system_time()+std::chrono::seconds(5));
    std::this_thread::sleep_for(std::chrono::seconds(8));
    
    std::cout << "stopping background refresh" << std::endl;
    nub.stop_background_refresh();
    std::cout << "stopped" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Manual refresh" << std::endl;
    nub.get_status();
    std::cout << "refreshed" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "now restart background refresh" << std::endl;
    nub.start_background_refresh();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "---------- now test overlapping ----------" << std::endl;
    nub.schedule_set_current(100, false, get_system_time()+std::chrono::seconds(3), get_system_time()+std::chrono::seconds(5));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "---------- set 200mA after 1 second ----------" << std::endl;
    // notice that this should cancel the previous set current event 
    nub.schedule_set_current(200, true, get_system_time()+std::chrono::seconds(1), get_system_time()+std::chrono::seconds(5));
    std::cout << "---------- set !!! ----------" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(6));
    std::cout << "done" << std::endl;
}

int run() {
    test_battery_status();
    test_python_binding();
    // test_uart();
    // test_JBDBMS(); 

    test_events();

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
    // error("Just to test abnormal return", ", sys=", sys, ", path=", path);
    return 0;
    
}


