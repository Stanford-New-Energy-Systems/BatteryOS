#include "admin_msg.pb.h"
#include "battery_msg.pb.h" 
#include "battery_status.pb.h"
#include <unistd.h> 
#include <fcntl.h>
uint8_t read_buffer[4096]; 

void ask_shutdown() {
    bosproto::AdminMsg amsg; 
    amsg.mutable_shutdown(); 
    int ifd = open("bosdir/admin_input", O_WRONLY | O_NONBLOCK); 
    if (ifd < 0) {
        std::cerr << "failed to open? errno = " << errno << std::endl; 
        return; 
    }
    std::cout << "ready to serialize" << std::endl; 
    bool serialize_success = amsg.SerializeToFileDescriptor(ifd); 
    std::cout << "serialized " << serialize_success << std::endl; 
    // fsync(ifd); 
    // must close, otherwise EOF not sent! 
    close(ifd); 

    int ofd = open("bosdir/admin_output", O_RDONLY); 
    bosproto::BatteryResp resp; 
    bosproto::AdminResp aresp; 
    std::cout << "parsing" << std::endl; 
    aresp.ParseFromFileDescriptor(ofd); 
    std::cout << "parsed" << std::endl; 
    close(ofd); 
    return; 
}

void get_status(const std::string &name) {
    bosproto::BatteryMsg msg; 
    msg.mutable_get_status(); 
    std::string input_name = "bosdir/";
    input_name = input_name + name + "_input"; 
    std::string output_name = "bosdir/"; 
    output_name = output_name + name + "_output"; 
    int ifd = open(input_name.c_str(), O_WRONLY); 
    if (ifd < 0) {
        std::cerr << "failed to open? errno = " << errno << std::endl; 
        return; 
    }
    std::cout << "ready to serialize" << std::endl; 
    bool serialize_success = msg.SerializeToFileDescriptor(ifd); 
    std::cout << "serialized " << serialize_success << std::endl; 
    close(ifd); 

    int ofd = open(output_name.c_str(), O_RDONLY); 
    bosproto::BatteryResp resp; 
    std::cout << "parsing" << std::endl; 
    resp.ParseFromFileDescriptor(ofd); 
    std::cout << "parsed" << std::endl; 
    

    const bosproto::BatteryStatus &stat = resp.status(); 
    std::cout << "voltage_mV = " << stat.voltage_mv() 
        << "\ncurrent_mA = " << stat.current_ma() 
        << "\nremaining_charge = " << stat.remaining_charge_mah() 
        << "\nfull_charge = " << stat.full_charge_capacity_mah() 
        << "\nmax_charging_current = " << stat.max_charging_current_ma()
        << "\nmax_discharging_current = " << stat.max_discharging_current_ma() 
        << "\ntime_sec_since_epoch = " << stat.timestamp().secs_since_epoch() 
        << "\ntime_ms = " << stat.timestamp().msec() 
        << std::endl;
    close(ofd); 
    std::cout << "get_status done" << std::endl;
    return; 
}

void make_pseudo() {
    std::cout << "make pseudo" << std::endl; 
    bosproto::AdminMsg msg; 
    bosproto::MakePseudo *args = msg.mutable_make_pseudo(); 
    args->set_name("pseudo"); 
    args->set_max_staleness_ms(3000); 
    bosproto::BatteryStatus *status = args->mutable_status(); 
    status->set_voltage_mv(1234); 
    status->set_current_ma(0); 
    status->set_remaining_charge_mah(123); 
    status->set_full_charge_capacity_mah(321); 
    status->set_max_charging_current_ma(10); 
    status->set_max_discharging_current_ma(12); 
    bosproto::CTimeStamp *ts = status->mutable_timestamp(); 
    ts->set_secs_since_epoch(123456); 
    ts->set_msec(998); 
    int ifd = open("bosdir/admin_input", O_WRONLY); 
    if (ifd < 0) {
        std::cerr << "failed to open? errno = " << errno << std::endl; 
        return; 
    }
    msg.SerializeToFileDescriptor(ifd); 
    close(ifd); 
    int ofd = open("bosdir/admin_output", O_RDONLY); 
    bosproto::BatteryResp resp; 
    bosproto::AdminResp aresp; 
    std::cout << "parsing" << std::endl; 
    aresp.ParseFromFileDescriptor(ofd); 
    std::cout << "parsed" << std::endl; 
    close(ofd); 
    return; 
}

void make_dynamic() {
    // make_dynamic(
    //     "dyn", "../example/build/libdynamicnull.dylib", 1000, 
    //     (void*)a, "init", "destroy", "get_status", "set_current"); 
    bosproto::AdminMsg msg; 
    bosproto::MakeDynamic *args = msg.mutable_make_dynamic(); 
    args->set_name("dyn"); 
    args->set_max_staleness_ms(123); 
    args->set_dynamic_lib_path("../example/build/libdynamicnull.dylib"); 
    std::string *init_arg = args->mutable_init_argument(); 
    (*init_arg) = "aaa"; 
    args->set_init_func_name("init"); 
    args->set_destruct_func_name("destroy"); 
    args->set_get_status_func_name("get_status"); 
    args->set_set_current_func_name("set_current"); 

    int ifd = open("bosdir/admin_input", O_WRONLY); 
    if (ifd < 0) {
        std::cerr << "failed to open? errno = " << errno << std::endl; 
        return; 
    }
    msg.SerializeToFileDescriptor(ifd); 
    close(ifd); 
    int ofd = open("bosdir/admin_output", O_RDONLY); 
    bosproto::AdminResp aresp; 
    std::cout << "parsing" << std::endl; 
    aresp.ParseFromFileDescriptor(ofd); 
    std::cout << "parsed" << std::endl; 
    close(ofd); 
    return; 
}

int main() {
    get_status("nullbat"); 
    std::cout << "------------------------------------" << std::endl;
    sleep(3); 
    make_pseudo(); 
    std::cout << "------------------------------------" << std::endl;
    sleep(3); 
    get_status("pseudo"); 
    std::cout << "------------------------------------" << std::endl;
    sleep(3); 
    make_dynamic(); 
    std::cout << "------------------------------------" << std::endl;
    sleep(3); 
    get_status("dyn"); 
    std::cout << "------------------------------------" << std::endl;
    sleep(3); 
    ask_shutdown(); 
    std::cout << "------------------------------------" << std::endl;
    return 0; 
}











































