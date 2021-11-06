#include "JBDBMS.hpp"
#include "TestBattery.hpp"
#include "NetworkBattery.hpp"
#include "AggregatorBattery.hpp"
#include "ProportionalPolicy.hpp"
#include "SplittedBattery.hpp"
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
    using namespace std::chrono_literals;
    std::cout << std::endl << "---------------------- test events ---------------------------" << std::endl << std::endl;
    
    NullBattery nub("null_battery", 1200, 1s);  // 1200mV, 1s refresh time
    timepoint_t now;
    // std::cout << "starting background refresh" << std::endl;
    // nub.start_background_refresh();

    // now = get_system_time();
    // nub.schedule_set_current(1000, true, now+3s, now+5s);

    // std::this_thread::sleep_for(8s);
    
    // std::cout << "stopping background refresh" << std::endl;
    // nub.stop_background_refresh();
    // std::cout << "stopped" << std::endl;

    // std::this_thread::sleep_for(2s);
    // std::cout << "Manual refresh" << std::endl;
    // nub.get_status();
    // std::cout << "refreshed" << std::endl;

    // std::this_thread::sleep_for(3s);
    // std::cout << "now restart background refresh" << std::endl;
    // nub.start_background_refresh();

    // std::this_thread::sleep_for(5s);

    // std::cout << "---------- now test overlapping ----------" << std::endl;
    // std::cout << "---------- set 100mA after 3s, for 2s ----------" << std::endl;
    // now = get_system_time();
    // nub.schedule_set_current(100, false, now+3s, now+5s);
    // std::this_thread::sleep_for(1s);
    
    // std::cout << "---------- set 200mA after 1s, for 4s ----------" << std::endl;
    // // notice that this should cancel the previous set current event 
    // now = get_system_time();
    // nub.schedule_set_current(200, true, now+1s, now+5s);
    // std::cout << "---------- set !!! ----------" << std::endl;
    // std::this_thread::sleep_for(6s);
    
    // std::cout << "stop background refresh" << std::endl;
    // nub.stop_background_refresh();
    // std::this_thread::sleep_for(2s);

    nub.start_background_refresh();
    std::cout << std::endl << "---------- now test 4 cases of overlapping ----------" << std::endl;
    
    std::cout << "Case 1: " << std::endl;
    now = get_system_time();
    nub.schedule_set_current(200, true, now+1s, now+3s);
    nub.schedule_set_current(300, true, now+2s, now+4s);
    std::this_thread::sleep_for(5s);
    std::cout << "End of case 1" << std::endl << std::endl; 

    std::cout << "Case 2: " << std::endl;
    now = get_system_time();
    nub.schedule_set_current(200, true, now+1s, now+4s);
    nub.schedule_set_current(300, false, now+2s, now+3s);
    std::this_thread::sleep_for(5s);
    std::cout << "End of case 2" << std::endl << std::endl; 

    std::cout << "Case 3: " << std::endl;
    now = get_system_time();
    nub.schedule_set_current(200, false, now+2s, now+4s);
    nub.schedule_set_current(300, true, now+1s, now+3s);
    std::this_thread::sleep_for(5s);
    std::cout << "End of case 3" << std::endl << std::endl; 

    std::cout << "Case 4: " << std::endl;
    now = get_system_time();
    nub.schedule_set_current(200, true, now+1s, now+2s);
    nub.schedule_set_current(300, true, now+2s, now+3s);
    std::this_thread::sleep_for(5s);
    std::cout << "End of case 4" << std::endl << std::endl; 

    std::cout << "Case 5: " << std::endl;
    now = get_system_time();
    nub.schedule_set_current(100, true, now, now+5s);
    std::this_thread::sleep_for(2s);
    now = get_system_time();
    nub.schedule_set_current(200, true, now, now+2s);
    std::this_thread::sleep_for(4s);


    std::cout << "done" << std::endl;
}

int run() {
    LOG();
    // test_battery_status();
    // test_python_binding();
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
    // ERROR() << "Just to test abnormal return" << ", sys=" << sys << ", path=" << path;
    // std::cout << std::endl;
    return 0;
    
}


