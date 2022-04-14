#include "JBDBMS.hpp"
#include "TestBattery.hpp"
#include "NetworkBattery.hpp"
#include "AggregatorBattery.hpp"
#include "BALSplitter.hpp"
#include "SplittedBattery.hpp"
#include "BOS.hpp"
#include "sonnen.hpp"
#include "ProtobufMsg.hpp"
#include <iostream>
#include <signal.h> 
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
#ifndef NO_PYTHON 
void test_python_binding() {
    std::thread test_thread([](){
        std::string addr = "/dev/ttyUSB2";
        RD6006PowerSupply rd6006(addr);
        rd6006.enable();
        rd6006.set_current_Amps(123.456);
        printf("%f\n", rd6006.get_current_Amps());
        std::string addr2 = "/dev/ttyUSB2";
        RD6006PowerSupply rd60062(addr2);
        rd60062.enable();
        rd60062.set_current_Amps(456.123);
        return;
    });
    test_thread.join();
    return;
}
#endif 
// void test_JBDBMS() {
//     JBDBMS bms("jbd", "/dev/ttyUSB1", "current_regulator");
//     JBDBMS::State basic_info = bms.get_basic_info();
//     std::cout << basic_info << std::endl;
//     std::cout << bms.get_status() << std::endl;
// }

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
    nub.schedule_set_current(100, true, now+100ms, now+5s);
    std::this_thread::sleep_for(2s);
    now = get_system_time();
    nub.schedule_set_current(200, true, now+100ms, now+2s);
    std::this_thread::sleep_for(4s);


    std::cout << "done" << std::endl;
}
#if 0
void test_agg_management() {
    using namespace std::chrono_literals;
    BOS bos;
    bos.make_pseudo("ps1", BatteryStatus {
        .voltage_mV=3000, 
        .current_mA=0, 
        .capacity_mAh=60000, 
        .max_capacity_mAh=60000, 
        .max_charging_current_mA=40000, 
        .max_discharging_current_mA=40000,
        .timestamp = get_system_time_c()
    }, 1000);
    bos.make_pseudo("ps2", BatteryStatus {
        .voltage_mV=3000, 
        .current_mA=0, 
        .capacity_mAh=40000, 
        .max_capacity_mAh=40000, 
        .max_charging_current_mA=60000, 
        .max_discharging_current_mA=60000,
        .timestamp = get_system_time_c()
    }, 1000);
    bos.make_aggregator("agg1", 3000, 1000, {"ps1", "ps2"}, 1000);
    if (!bos.directory.name_exists("agg1")) {
        ERROR() << "agg1 not exists?";
    }
    std::cout << "OK" << std::endl;
    std::this_thread::sleep_for(1s);
    std::cout << "OK" << std::endl;
    // bos.start_background_refresh("ps1");
    // bos.start_background_refresh("ps2");
    bos.start_background_refresh("agg1");
    std::cout << "agg background started" << std::endl;
    timepoint_t now = get_system_time();
    bos.schedule_set_current("agg1", 1000, (now+1s), (now+3s));

    std::this_thread::sleep_for(5s);
    std::cout << "done" << std::endl;
    return;
}

void test_split_management(int policy = int(BALSplitterType::Proportional)) {
    using namespace std::chrono_literals;
    BOS bos;
    bos.make_pseudo("ps1", BatteryStatus{
        .voltage_mV = 3000,
        .current_mA = 0,
        .capacity_mAh = 200000,
        .max_capacity_mAh = 200000,
        .max_charging_current_mA = 80000,
        .max_discharging_current_mA = 100000,
    }, 500);

    bos.make_policy(
        "split_proportional", 
        "ps1",
        {"s1", "s2", "s3"}, 
        {Scale(0.5), Scale(0.3), Scale(0.2)}, 
        {500, 500, 500}, 
        policy, 
        1000
    );

    std::this_thread::sleep_for(1s);
    std::cout << "s1: " << bos.get_status("s1") << std::endl;
    std::this_thread::sleep_for(1s);
    std::cout << "s2: " << bos.get_status("s2") << std::endl;
    std::this_thread::sleep_for(1s);
    std::cout << "s3: " << bos.get_status("s3") << std::endl;
    std::this_thread::sleep_for(1s);


    std::cout << "---------- get ready for a current change! ------" << std::endl;;

    std::this_thread::sleep_for(1s);
    timepoint_t now = get_system_time();
    bos.schedule_set_current("s1", 1000, now+100ms, now+5s);

    std::this_thread::sleep_for(2s);
    std::cout << "s1: " << bos.get_status("s1") << std::endl;
    std::cout << "s2: " << bos.get_status("s2") << std::endl;
    std::cout << "s3: " << bos.get_status("s3") << std::endl;
    std::cout << "ps1: " << bos.get_status("ps1") << std::endl;
    std::this_thread::sleep_for(5s);

    std::cout << "done" << std::endl;
    return;
}

void test_sonnen_getstatus(int serial = std::stoi(std::getenv("SONNEN_SERIAL1")), int iterations = 1) {
    using namespace std::chrono_literals;
    Sonnen sonnen("s1", serial, 10000, 30000, 30000, std::chrono::milliseconds(1000));
    Sonnen sonnen2("s2", std::stoi(std::getenv("SONNEN_SERIAL2")), 10000, 30000, 30000, std::chrono::milliseconds(1000));
    Sonnen sonnen3("s3", std::stoi(std::getenv("SONNEN_SERIAL3")), 10000, 30000, 30000, std::chrono::milliseconds(1000));
    CSVOutput csv(
        "test_sonnen_get_status.csv", 
        {"Date & Time", "Unix_date1", "Power1",
         "Date & Time", "Unix_date2", "Power2",
         "Date & Time", "Unix_date3", "Power3"}
    );
    for (int i = 0; i < iterations; ++i) {
        std::this_thread::sleep_for(1s);
        BatteryStatus status = sonnen.get_status();
        BatteryStatus status2 = sonnen2.get_status();
        BatteryStatus status3 = sonnen3.get_status();
        output_status_to_csv(csv, {status, status2, status3});
        std::cout << status << status2 << status3 << std::endl;
    }
    return;
}

void test_sonnen() {
    using namespace std::chrono_literals;
    Sonnen sonnen("s1", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(5000));
    BatteryStatus status = sonnen.get_status();
    LOG() << "s1:\n" << status << std::endl;

    timepoint_t now = get_system_time();
    double watts = -3500;
    sonnen.schedule_set_current(round(watts / (status.voltage_mV/1000) * 1000), true, now+35s, now+5min+35s);
    // std::this_thread::sleep_for(6min);

    std::this_thread::sleep_for(35s);
    LOG() << "the power should be ramped up now, now is (secs since epoch)" << 
            std::chrono::duration_cast<std::chrono::seconds>(get_system_time().time_since_epoch()).count() << std::endl;

    std::this_thread::sleep_for(5min);
    LOG() << "the power should be ramped down now, now is (secs since epoch)" << 
            std::chrono::duration_cast<std::chrono::seconds>(get_system_time().time_since_epoch()).count() << std::endl;

    std::this_thread::sleep_for(10s);
}

void test_sonnen_split() {
    using namespace std::chrono_literals;
    BOS bos;
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "son1", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(5000))
        )
    );
    bos.make_policy(
        "split_proportional", 
        "son1",
        {"s1", "s2"}, 
        {Scale(0.6), Scale(0.4)}, 
        {500, 500}, 
        int(BALSplitterType::Proportional), 
        1000
    );
    double volt = 233;
    double w1 = 2000;
    double w2 = 2500;
    timepoint_t now = get_system_time();
    bos.schedule_set_current("s1", w1/volt*1000, now+1s, now+5min);
    bos.schedule_set_current("s2", w2/volt*1000, now+2min, now+5min);
    std::this_thread::sleep_for(6min);
}

void test_sonnen_aggregate() {
    using namespace std::chrono_literals;
    BOS bos; 
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(5000))
        )
    ); // 74% by the time testing it 
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "home1", std::stoi(std::getenv("SONNEN_SERIAL2")), 10000, 30000, 30000, std::chrono::milliseconds(1000))
        )
    ); // 100% by the time testing it 
    bos.make_aggregator("agg1", 235000, 10000, {"slac", "home1"}, 1000);  // 235 +- 10
    double volt = 239.0;
    double w1 = 4000.0;
    timepoint_t now = get_system_time();

    LOG() << "STATUS OF agg1: \n" << bos.get_status("agg1");
    bos.schedule_set_current("agg1", round(w1/volt*1000), now+35s, now+5min+35s);

    std::this_thread::sleep_for(35s);
    LOG() << "the power should be ramped up now, now is (secs since epoch)" << 
            std::chrono::duration_cast<std::chrono::seconds>(get_system_time().time_since_epoch()).count() << std::endl;

    std::this_thread::sleep_for(5min);
    LOG() << "the power should be ramped down now, now is (secs since epoch)" << 
            std::chrono::duration_cast<std::chrono::seconds>(get_system_time().time_since_epoch()).count() << std::endl;


    std::this_thread::sleep_for(10s);

}

void test_sonnen_aggregate_split(int policy = int(BALSplitterType::Proportional)) {
    using namespace std::chrono_literals;
    BOS bos; 
    Battery *slac = bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, 60s)
        )
    ); 
    

    Battery *home1 = bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "home1", std::stoi(std::getenv("SONNEN_SERIAL3")), 10000, 30000, 30000, 60s)
        )
    );
    // // hack, so that we can get the status anyways 
    // BatteryStatus slacstatus = slac->get_status();
    // BatteryStatus homestatus = home1->get_status();
    // slacstatus.current_mA = 0;
    // homestatus.current_mA = 0;
    // slac->set_status(slacstatus);
    // home1->set_status(homestatus);
    // // end of hack 

    // LOG() << slac->get_status() << home1->get_status();

    bos.make_aggregator("agg1", 235000, 10000, {"slac", "home1"}, 0);  // 235 +- 10

    bos.make_policy("sp1", "agg1", {"vb", "vc"}, {Scale(0.6), Scale(0.4)}, {0, 0}, policy, 0);

    BatteryStatus slac_status = slac->get_status();
    BatteryStatus home1_status = home1->get_status();
    std::cout << "---------- before the change ----------" << std::endl;
    LOG() << "slac:\n" << slac_status;
    LOG() << "home1:\n" << home1_status;
    LOG() << "vb:\n" << bos.get_status("vb");
    LOG() << "vc:\n" << bos.get_status("vc");
    LOG() << "agg1\n" << bos.get_status("agg1");
    
    // int64_t prev_cap = slac_status.capacity_mAh;

    slac_status.capacity_mAh = 0;
    slac->set_status(slac_status);
    std::this_thread::sleep_for(5s);
    std::cout << "---------- after changing slac to 0 ----------" << std::endl;
    LOG() << "slac:\n" << slac->get_status();
    LOG() << "home1:\n" << home1->get_status();
    LOG() << "vb:\n" << bos.get_status("vb");
    LOG() << "vc:\n" << bos.get_status("vc");
    LOG() << "agg1\n" << bos.get_status("agg1");

    
    slac_status.capacity_mAh = slac_status.max_capacity_mAh * 0.8;
    slac->set_status(slac_status);
    std::this_thread::sleep_for(5s);
    std::cout << "---------- after changing slac to max_cap * 0.8 ----------" << std::endl;
    LOG() << "slac:\n" << slac_status;
    LOG() << "home1:\n" << home1_status;
    LOG() << "vb:\n" << bos.get_status("vb");
    LOG() << "vc:\n" << bos.get_status("vc");
    LOG() << "agg1\n" << bos.get_status("agg1");

}

/////////////////////////////////////////////////////////// EXPERIMENTS //////////////////////////////////////////////////////////////
/**
 * watts > 0: discharge, ;
 * watts < 0: charge 
 */
void experiment1(double watts = 3500.0) {
    using namespace std::chrono_literals;
    BOS bos; 
    Battery *slac = bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, 500ms)
        )
    ); 

    BatteryStatus status = slac->get_status();
    LOG() << "initial slac status:\n" << status << std::endl;

    timepoint_t now = get_system_time();
    time_t now_ts = std::chrono::system_clock::to_time_t(now);
    time_t now_ts1 = std::chrono::system_clock::to_time_t(now+35s);
    time_t now_ts2 = std::chrono::system_clock::to_time_t(now+5min+35s);
    LOG() << "now is: (unix timestamp): " << now_ts << "\n"
        << "scheduled rampup time: (unix timestamp): " << now_ts1 << "\n"
        << "and scheduled rampdown time: (unix timestamp): " << now_ts2; 

    slac->schedule_set_current(round(watts / (status.voltage_mV/1000) * 1000), true, now+35s, now+5min+35s);
    CSVOutput csv(
        std::string("experiment1_")+std::to_string(watts)+std::string("W.csv"), 
        {"slac_date & time", "slac_unix_date", "slac_power"}
    );
    for (int i = 0; i < 335; ++i) {
        std::this_thread::sleep_for(1s);
        BatteryStatus status = slac->get_status();
        output_status_to_csv(csv, {status});
        std::cout << status;
    }
}

void experiment2(double watts = 4000.0) {
    using namespace std::chrono_literals;
    BOS bos; 
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(1000))
        )
    );
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "home1", std::stoi(std::getenv("SONNEN_SERIAL2")), 10000, 30000, 30000, std::chrono::milliseconds(1000))
        )
    ); 
    // if it says voltage out of range, change 235000 to something else, 
    // or change 10000 to something else 
    // current voltage is 235,000 +- 10,000 mV which is 235+-10 V
    bos.make_aggregator("agg1", 235000, 10000, {"slac", "home1"}, 1000);  // 235 +- 10
    double volt = 235.0;

    LOG() << "initital status of agg1: \n" << bos.get_status("agg1");

    timepoint_t now = get_system_time();
    time_t now_ts = std::chrono::system_clock::to_time_t(now);
    time_t now_ts1 = std::chrono::system_clock::to_time_t(now+35s);
    time_t now_ts2 = std::chrono::system_clock::to_time_t(now+5min+35s);

    LOG() << "now is: (unix timestamp): " << now_ts << "\n"
        << "scheduled rampup time: (unix timestamp): " << now_ts1 << "\n"
        << "and scheduled rampdown time: (unix timestamp): " << now_ts2; 
    
    bos.schedule_set_current("agg1", round(watts/volt*1000), now+35s, now+5min+35s);
    CSVOutput csv(
        std::string("experiment2_")+std::to_string(watts)+std::string("W.csv"), 
        {
            "slac_date & time", "slac_unix_date", "slac_power", 
            "home1_date & time", "home1_unix_date", "home1_power", 
            "agg1_date & time", "agg1_unix_date", "agg1_power",
        }
    );
    for (int i = 0; i < 335; ++i) {
        std::this_thread::sleep_for(1s);
        BatteryStatus slac_status = bos.get_status("slac");
        BatteryStatus home1_status = bos.get_status("home1");
        BatteryStatus agg1_status = bos.get_status("agg1");
        output_status_to_csv(csv, {slac_status, home1_status, agg1_status});
        LOG() << "slac:\n" << slac_status << "home1:\n" << home1_status << "agg1:\n" << agg1_status;
    }
}

void experiment3(double watt1 = 2000.0, double watt2 = 2500.0) {
    using namespace std::chrono_literals;
    BOS bos;
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(500))
        )
    );
    BatteryStatus slac_status = bos.get_status("slac");
    LOG() << "initial status for slac:" << slac_status;
    bos.make_policy(
        "split_proportional", 
        "slac",
        {"s1", "s2"}, 
        {Scale(0.6), Scale(0.4)}, 
        {500, 500}, 
        int(BALSplitterType::Proportional), 
        1000
    );
    double volt = slac_status.voltage_mV / 1000;

    timepoint_t now = get_system_time();
    time_t now_ts = std::chrono::system_clock::to_time_t(now);
    time_t now_ts1up = std::chrono::system_clock::to_time_t(now+35s);
    time_t now_ts1down = std::chrono::system_clock::to_time_t(now+5min+35s);
    time_t now_ts2up = std::chrono::system_clock::to_time_t(now+2min+35s);
    time_t now_ts2down = std::chrono::system_clock::to_time_t(now+4min+35s);
    
    LOG() << "now is: (unix timestamp): " << now_ts << "\n"
        << "s1 should ramp up at: (unix timestamp): " << now_ts1up << "\n"
        << "s1 should ramp down at: (unix timestamp): " << now_ts1down << "\n"
        << "s2 should ramp up at: (unix timestamp): " << now_ts2up << "\n"
        << "s2 should ramp down at: (unix timestamp): " << now_ts2down; 

    CSVOutput csv(
        std::string("experiment3_")+std::to_string(watt1)+"W_"+std::to_string(watt2)+std::string("W.csv"), 
        {
            "slac_date & time", "slac_unix_date", "slac_power", 
            "s1_date & time", "s1_unix_date", "s1_power", 
            "s2_date & time", "s2_unix_date", "s2_power",
        }
    );

    bos.schedule_set_current("s1", watt1/volt*1000, now+35s, now+5min+35s);
    bos.schedule_set_current("s2", watt2/volt*1000, now+2min+35s, now+4min+35s);
    
    for (int i = 0; i < 335; ++i) {
        std::this_thread::sleep_for(1s);
        BatteryStatus slac_status = bos.get_status("slac");
        BatteryStatus s1_status = bos.get_status("s1");
        BatteryStatus s2_status = bos.get_status("s2");
        output_status_to_csv(csv, {slac_status, s1_status, s2_status});
        LOG() << "slac:\n" << slac_status << "s1:\n" << s1_status << "s2:\n" << s2_status;
    }
}

void experiment4(double watt1 = 2000.0, double watt2 = -1500.0) {
    using namespace std::chrono_literals;
    BOS bos; 
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(500))
        )
    );
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "home1", std::stoi(std::getenv("SONNEN_SERIAL2")), 10000, 30000, 30000, std::chrono::milliseconds(500))
        )
    ); 
    bos.make_aggregator("agg1", 235000, 10000, {"slac", "home1"}, 500);  // 235 +- 10
    double volt = 235.0;

    LOG() << "initital status of agg1: \n" << bos.get_status("agg1");

    bos.make_policy(
        "split_reservation", 
        "agg1",
        {"s1", "s2"}, 
        {Scale(0.6), Scale(0.4)}, 
        {500, 500}, 
        int(BALSplitterType::Reservation), 
        1000
    );

    CSVOutput csv(
        std::string("experiment4_")+std::to_string(watt1)+"W_"+std::to_string(watt2)+std::string("W.csv"), 
        {
            "slac_date & time", "slac_unix_date", "slac_power", 
            "home1_date & time", "home1_unix_date", "home1_power", 
            "agg1_date & time", "agg1_unix_date", "agg1_power",
            "s1_date & time", "s1_unix_date", "s1_power", 
            "s2_date & time", "s2_unix_date", "s2_power",
        }
    );

    
    timepoint_t now = get_system_time();
    time_t now_ts = std::chrono::system_clock::to_time_t(now);
    time_t now_ts1up = std::chrono::system_clock::to_time_t(now+35s);
    time_t now_ts1down = std::chrono::system_clock::to_time_t(now+4min+35s);
    time_t now_ts2up = std::chrono::system_clock::to_time_t(now+2min+35s);
    time_t now_ts2down = std::chrono::system_clock::to_time_t(now+5min+35s);
    
    LOG() << "now is: (unix timestamp): " << now_ts << "\n"
        << "s1 should ramp up at: (unix timestamp): " << now_ts1up << "\n"
        << "s1 should ramp down at: (unix timestamp): " << now_ts1down << "\n"
        << "s2 should ramp up at: (unix timestamp): " << now_ts2up << "\n"
        << "s2 should ramp down at: (unix timestamp): " << now_ts2down; 

    bos.schedule_set_current("s1", watt1/volt*1000, now+35s, now+4min+35s);
    bos.schedule_set_current("s2", watt2/volt*1000, now+2min+35s, now+5min+35s);


    for (int i = 0; i < 335; ++i) {
        std::this_thread::sleep_for(1s);
        BatteryStatus slac_status = bos.get_status("slac");
        BatteryStatus home1_status = bos.get_status("home1");
        BatteryStatus agg1_status = bos.get_status("agg1");
        BatteryStatus s1_status = bos.get_status("s1");
        BatteryStatus s2_status = bos.get_status("s2");
        output_status_to_csv(csv, {slac_status, home1_status, agg1_status, s1_status, s2_status});
        LOG() << "slac:\n" << slac_status 
            << "home1:\n" << home1_status 
            << "agg1:\n" << agg1_status 
            << "s1:\n" << s1_status 
            << "s2:\n" << s2_status;
    }
}

void experiment5(double watt1 = -2000.0, double watt2 = 2500.0) {
    using namespace std::chrono_literals;
    BOS bos; 
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(500))
        )
    );
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "home1", std::stoi(std::getenv("SONNEN_SERIAL2")), 10000, 30000, 30000, std::chrono::milliseconds(500))
        )
    ); 
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "home2", std::stoi(std::getenv("SONNEN_SERIAL3")), 10000, 30000, 30000, std::chrono::milliseconds(500))
        )
    ); 
    bos.make_aggregator("agg1", 235000, 10000, {"slac", "home1"}, 500);  // 235 +- 10
    double volt = 235.0;

    LOG() << "initital status of agg1: \n" << bos.get_status("agg1");

    bos.make_policy(
        "split_tranche", 
        "agg1",
        {"s1", "s2"}, 
        {Scale(0.6), Scale(0.4)}, 
        {500, 500}, 
        int(BALSplitterType::Tranche), 
        1000
    );

    bos.make_aggregator("agg2", 235000, 10000, {"s2", "home2"}, 500);

    LOG() << "initital status of agg2: \n" << bos.get_status("agg2");

    CSVOutput csv(
        std::string("experiment5_")+std::to_string(watt1)+"W_"+std::to_string(watt2)+std::string("W.csv"), 
        {
            "slac_date & time", "slac_unix_date", "slac_power", 
            "home1_date & time", "home1_unix_date", "home1_power", 
            "agg1_date & time", "agg1_unix_date", "agg1_power",
            "s1_date & time", "s1_unix_date", "s1_power", 
            "s2_date & time", "s2_unix_date", "s2_power",
            "home2_date & time", "home2_unix_date", "home2_power", 
            "agg2_date & time", "agg2_unix_date", "agg2_power",
        }
    );

    
    timepoint_t now = get_system_time();
    time_t now_ts = std::chrono::system_clock::to_time_t(now);
    time_t now_ts1up = std::chrono::system_clock::to_time_t(now+35s);
    time_t now_ts1down = std::chrono::system_clock::to_time_t(now+5min+35s);
    time_t now_ts2up = std::chrono::system_clock::to_time_t(now+2min+35s);
    time_t now_ts2down = std::chrono::system_clock::to_time_t(now+5min+35s);
    
    LOG() << "now is: (unix timestamp): " << now_ts << "\n"
        << "s1 should ramp up at: (unix timestamp): " << now_ts1up << "\n"
        << "s1 should ramp down at: (unix timestamp): " << now_ts1down << "\n"
        << "agg2 should ramp up at: (unix timestamp): " << now_ts2up << "\n"
        << "agg2 should ramp down at: (unix timestamp): " << now_ts2down; 

    bos.schedule_set_current("s1", watt1/volt*1000, now+35s, now+5min+35s);
    bos.schedule_set_current("agg2", watt2/volt*1000, now+2min+35s, now+5min+35s);

    for (int i = 0; i < 335; ++i) {
        std::this_thread::sleep_for(1s);
        BatteryStatus slac_status = bos.get_status("slac");
        BatteryStatus home1_status = bos.get_status("home1");
        BatteryStatus agg1_status = bos.get_status("agg1");
        BatteryStatus s1_status = bos.get_status("s1");
        BatteryStatus s2_status = bos.get_status("s2");
        BatteryStatus home2_status = bos.get_status("home2");
        BatteryStatus agg2_status = bos.get_status("agg2");
        output_status_to_csv(csv, {slac_status, home1_status, agg1_status, s1_status, s2_status, home2_status, agg2_status});
        LOG() << "slac:\n" << slac_status 
            << "home1:\n" << home1_status 
            << "agg1:\n" << agg1_status 
            << "s1:\n" << s1_status 
            << "s2:\n" << s2_status
            << "home2:\n" << home2_status
            << "agg2:\n" << agg2_status;
    }
}
/////////////////////////////////////////////////////////// EXPERIMENTS //////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////// DRYRUNS //////////////////////////////////////////////////////////////

void experiment1_dryrun(double watts = 3500.0) {
    using namespace std::chrono_literals;
    BOS bos; 
    Battery *slac = bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, 1000ms)
        )
    ); 

    BatteryStatus status = slac->get_status();
    LOG() << "initial slac status:\n" << status << std::endl;

    timepoint_t now = get_system_time();
    time_t now_ts = std::chrono::system_clock::to_time_t(now);
    time_t now_ts1 = std::chrono::system_clock::to_time_t(now+35s);
    time_t now_ts2 = std::chrono::system_clock::to_time_t(now+5min+35s);
    LOG() << "now is: (unix timestamp): " << now_ts << "\n"
        << "scheduled rampup time: (unix timestamp): " << now_ts1 << "\n"
        << "and scheduled rampdown time: (unix timestamp): " << now_ts2; 

    // slac->schedule_set_current(round(watts / (status.voltage_mV/1000) * 1000), true, now+35s, now+5min+35s);
    CSVOutput csv(
        std::string("experiment1_")+std::to_string(watts)+std::string("W.csv"), 
        {"slac_date & time", "slac_unix_date", "slac_power"}
    );
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(1s);
        BatteryStatus status = slac->get_status();
        output_status_to_csv(csv, {status});
        std::cout << status;
    }
}

void experiment2_dryrun(double watts = 4000.0) {
    using namespace std::chrono_literals;
    BOS bos; 
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(1000))
        )
    );
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "home1", std::stoi(std::getenv("SONNEN_SERIAL3")), 10000, 30000, 30000, std::chrono::milliseconds(1000))
        )
    ); 
    // if it says voltage out of range, change 235000 to something else, 
    // or change 10000 to something else 
    // current voltage is 235,000 +- 10,000 mV which is 235+-10 V
    bos.make_aggregator("agg1", 235000, 10000, {"slac", "home1"}, 1000);  // 235 +- 10
    double volt = 235.0;

    LOG() << "initital status of agg1: \n" << bos.get_status("agg1");

    timepoint_t now = get_system_time();
    time_t now_ts = std::chrono::system_clock::to_time_t(now);
    time_t now_ts1 = std::chrono::system_clock::to_time_t(now+35s);
    time_t now_ts2 = std::chrono::system_clock::to_time_t(now+5min+35s);

    LOG() << "now is: (unix timestamp): " << now_ts << "\n"
        << "scheduled rampup time: (unix timestamp): " << now_ts1 << "\n"
        << "and scheduled rampdown time: (unix timestamp): " << now_ts2; 
    
    // bos.schedule_set_current("agg1", round(watts/volt*1000), now+35s, now+5min+35s);
    CSVOutput csv(
        std::string("experiment2_")+std::to_string(watts)+std::string("W.csv"), 
        {
            "slac_date & time", "slac_unix_date", "slac_power", 
            "home1_date & time", "home1_unix_date", "home1_power", 
            "agg1_date & time", "agg1_unix_date", "agg1_power",
        }
    );
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(1s);
        BatteryStatus slac_status = bos.get_status("slac");
        BatteryStatus home1_status = bos.get_status("home1");
        BatteryStatus agg1_status = bos.get_status("agg1");
        output_status_to_csv(csv, {slac_status, home1_status, agg1_status});
        LOG() << "slac:\n" << slac_status << "home1:\n" << home1_status << "agg1:\n" << agg1_status;
    }
}

void experiment3_dryrun(double watt1 = 2000.0, double watt2 = 2500.0) {
    using namespace std::chrono_literals;
    BOS bos;
    bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(500))
        )
    );
    BatteryStatus slac_status = bos.get_status("slac");
    LOG() << "initial status for slac:" << slac_status;
    bos.make_policy(
        "split_proportional", 
        "slac",
        {"s1", "s2"}, 
        {Scale(0.6), Scale(0.4)}, 
        {500, 500}, 
        int(BALSplitterType::Tranche), 
        1000
    );
    double volt = slac_status.voltage_mV / 1000;

    timepoint_t now = get_system_time();
    time_t now_ts = std::chrono::system_clock::to_time_t(now);
    time_t now_ts1up = std::chrono::system_clock::to_time_t(now+35s);
    time_t now_ts1down = std::chrono::system_clock::to_time_t(now+5min+35s);
    time_t now_ts2up = std::chrono::system_clock::to_time_t(now+2min+35s);
    time_t now_ts2down = std::chrono::system_clock::to_time_t(now+4min+35s);
    
    LOG() << "now is: (unix timestamp): " << now_ts << "\n"
        << "s1 should ramp up at: (unix timestamp): " << now_ts1up << "\n"
        << "s1 should ramp down at: (unix timestamp): " << now_ts1down << "\n"
        << "s2 should ramp up at: (unix timestamp): " << now_ts2up << "\n"
        << "s2 should ramp down at: (unix timestamp): " << now_ts2down; 

    CSVOutput csv(
        std::string("experiment3_")+std::to_string(watt1)+"W_"+std::to_string(watt2)+std::string("W.csv"), 
        {
            "slac_date & time", "slac_unix_date", "slac_power", 
            "s1_date & time", "s1_unix_date", "s1_power", 
            "s2_date & time", "s2_unix_date", "s2_power",
        }
    );

    // bos.schedule_set_current("s1", watt1/volt*1000, now+35s, now+5min+35s);
    // bos.schedule_set_current("s2", watt2/volt*1000, now+2min+35s, now+4min+35s);
    
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(1s);
        BatteryStatus slac_status = bos.get_status("slac");
        BatteryStatus s1_status = bos.get_status("s1");
        BatteryStatus s2_status = bos.get_status("s2");
        output_status_to_csv(csv, {slac_status, s1_status, s2_status});
        LOG() << "slac:\n" << slac_status << "s1:\n" << s1_status << "s2:\n" << s2_status;
    }
}
/////////////////////////////////////////////////////////// DRYRUNS //////////////////////////////////////////////////////////////


int test_network(int port) {
    using namespace std::chrono_literals;
    BOS bos;
    Battery *bat = bos.directory.add_battery(
        std::unique_ptr<Battery>(
            new Sonnen(
                "slac", std::stoi(std::getenv("SONNEN_SERIAL1")), 10000, 30000, 30000, std::chrono::milliseconds(500))
        )
    );
    Sonnen *sonnen = dynamic_cast<Sonnen*>(bat);
    BatteryStatus slac_status = bos.get_status("slac");
    LOG() << slac_status;
    
    bos.simple_remote_connection_server(port);
    return 0;
}

int test_network2(int port) {
    using namespace std::chrono_literals;
    std::unique_ptr<Connection> pconn(new TCPConnection("127.0.0.1", port));
    if (!pconn->connect()) {
        WARNING() << "Connection failed!";
        return 1;
    }
    NetworkBattery netbat("remote_slac", "slac", std::move(pconn), 0ms);

    BatteryStatus status = netbat.get_status();
    LOG() << "netbattery status: " << status;

    double watts = 2500;
    timepoint_t now = get_system_time();
    time_t now_ts = std::chrono::system_clock::to_time_t(now);
    time_t now_ts1 = std::chrono::system_clock::to_time_t(now+35s);
    time_t now_ts2 = std::chrono::system_clock::to_time_t(now+5min+35s);
    LOG() << "now is: (unix timestamp): " << now_ts << "\n"
        << "scheduled rampup time: (unix timestamp): " << now_ts1 << "\n"
        << "and scheduled rampdown time: (unix timestamp): " << now_ts2; 
    
    netbat.schedule_set_current(round(watts / (status.voltage_mV/1000) * 1000), true, now+35s, now+5min+35s);

    std::this_thread::sleep_for(5s);
    return 0;
}
#endif 
BatteryOS *bosptr = nullptr; 
void test_bos_fifo() {
    BatteryOS bos; 
    bosptr = &bos; 
    bos.get_manager().make_null("nullbat", 10000, 1000); 
    // int retval = bos.admin_fifo_init(); 
    // bool failed = false; 
    // if (retval < 0) {
    //     WARNING() << "Failed to initialize admin fifo"; 
    //     failed = true; 
    // }
    // retval = bos.battery_fifo_init(); 
    // if (retval < 0) {
    //     WARNING() << "Failed to initialize battery fifo"; 
    //     failed = true; 
    // }
#ifdef __linux__
    bos.bootup_fifo("/tmp/bosdir"); 
#else 
    bos.bootup_fifo(); 
#endif 
    // bos will shutdown 
    // bos.notify_should_quit(); 
}

void test_bos_remote() {
    BatteryOS bos; 
    bosptr = &bos; 
    // bos.get_manager().make_null("remote", 1000, 1000); 
    // bos.get_manager().make_null("remote2", 1000, 1000); 
    int limit = 1000; 
    std::vector<std::string> names; 
    for (int i = 0; i < limit; ++i) {
        bos.get_manager().make_pseudo("ps"+std::to_string(i), BatteryStatus {
            .voltage_mV=3000, 
            .current_mA=0, 
            .capacity_mAh=20000, 
            .max_capacity_mAh=40000, 
            .max_charging_current_mA=60000, 
            .max_discharging_current_mA=60000,
            .timestamp = get_system_time_c()
        }, 1000);
        names.push_back("ps"+std::to_string(i)); 
    }
    bos.get_manager().make_aggregator("agg1", 3000, 10, names, 1000); 
    bos.bootup_tcp_socket(1234); 
}

void test_dynamic() {
    BatteryOS bos; 
    const char *a = "123"; 
    Battery *dyn = bos.get_manager().make_dynamic(
        "dyn", "../example/build/libdynamicnull.dylib", 1000, 
        (void*)a, "init", "destroy", "get_status", "set_current"); 
    LOG() << dyn->get_status(); 
}

int run() {
    LOG();
    // protobufmsg::test_protobuf(); 
    // test_bos_fifo(); 
    test_bos_remote(); 
    // test_dynamic(); 
    // test_battery_status();
    // test_python_binding();
    // test_uart();
    // test_JBDBMS(); 
    // test_events();
    // test_agg_management();  // seems ok! 
    // test_split_management(int(BALSplitterType::Reservation));
    // std::cout << std::chrono::duration_cast<std::chrono::seconds>(get_system_time().time_since_epoch()).count() << std::endl;
    // test_sonnen();
    // test_sonnen_split();
    // test_sonnen_getstatus(std::stoi(std::getenv("SONNEN_SERIAL1")), 10);
    // test_sonnen_aggregate();
    // test_sonnen_aggregate_split(int(BALSplitterType::Proportional));

/////////////////////////////////////////////////////////// LOOK AT THIS //////////////////////////////////////////////////////////////
    {
        ////// the experiments: 
        // experiment1(3500);
        // experiment2(4000);
        // experiment3(2000, 2500);
        // experiment4(2000, -1500);
        // experiment5(-2000, 2500);
        ////// the dryruns
        // experiment1_dryrun();
        // experiment2_dryrun();
        // experiment3_dryrun();
    }
/////////////////////////////////////////////////////////// HERE //////////////////////////////////////////////////////////////

    // test_network(1234);
    // test_network2(1234);
    return 0;
}

void single_remote_pseduobat(int port) {
    BatteryOS bos; 
    bosptr = &bos; 
    bos.get_manager().make_pseudo("remote"+std::to_string(port), BatteryStatus {
        .voltage_mV=3000, 
        .current_mA=0, 
        .capacity_mAh=20000, 
        .max_capacity_mAh=40000, 
        .max_charging_current_mA=60000, 
        .max_discharging_current_mA=60000,
        .timestamp = get_system_time_c()
    }, 0);
    bos.bootup_tcp_socket(port); 
}

void aggregate_remote_pseudobat(const char *const addr, int portmin, int portmax) {
    BatteryOS bos; 
    bosptr = &bos; 
    std::vector<std::string> rbat; 
    for (int port = portmin; port <= portmax; ++port) {
        bos.get_manager().make_networked_battery(
            "remote"+std::to_string(port), 
            "remote"+std::to_string(port), 
            addr, 
            port, 
            0
        ); 
        rbat.push_back("remote"+std::to_string(port));
    }
    Battery *agg = bos.get_manager().make_aggregator("agg", 3000, 1000, rbat, 0); 
    if (!agg) {
        WARNING() << "agg is NULL? what's wrong?";
        return; 
    }
    BatteryStatus status;
    std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now(); 
    /// time it!!! 
    status = agg->manual_refresh(); 
    std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now(); 
    std::cout << status << std::endl;
    LOG() << "The time difference is: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us";
    std::ofstream logger;
    logger.open("./aggdelay.csv", std::ios::app); 
    logger << (portmax-portmin+1) << ", " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;
    logger.close(); 
    return; 
}
void chain_remote(const char *const addr, int remoteport, int myport) {
    BatteryOS bos; 
    bosptr = &bos; 
    bos.get_manager().make_networked_battery(
        "remote"+std::to_string(myport), 
        "remote"+std::to_string(remoteport),
        addr, 
        remoteport,
        0
    );
    bos.bootup_tcp_socket(myport); 
}
void chain_remote_end(const char *const addr, int remoteport) {
    BatteryOS bos; 
    bosptr = &bos; 
    Battery *bat = bos.get_manager().make_networked_battery(
        "endpoint",
        "remote"+std::to_string(remoteport),
        addr, 
        remoteport,
        0
    );
    BatteryStatus status;
    std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now(); 
    /// time it!!! 
    status = bat->manual_refresh(); 
    std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now(); 
    std::cout << status << std::endl;
    LOG() << "The time difference is: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "us";
    std::ofstream logger;
    logger.open("./chaindelay.csv", std::ios::app); 
    logger << (remoteport-1200+1) << ", " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << std::endl;
    logger.close(); 
}

void sigint_handler(int sig) {
    if (bosptr) { bosptr->notify_should_quit(); }
    // exit(0); 
}
int main(int argc, const char *const argv[]) {
#ifndef NO_PYTHON
    Py_Initialize();
    // PyEval_InitThreads();
    // add the current path to the module search path!
    PyObject *sys = PyImport_ImportModule("sys");
    PyObject *path = PyObject_GetAttrString(sys, "path");
    PyList_Append(path, PyUnicode_FromString("./python"));
    Py_DECREF(path);
    Py_DECREF(sys);
    Py_BEGIN_ALLOW_THREADS
#endif 
    signal(SIGINT, sigint_handler); 
    // ./bos client port
    // ./bos server port_min port_max
    // ./bos chain myport remoteport
    // ./bos chainend remoteport
    if (argc < 3) {
        run();
    } else {
        if (strcmp("client", argv[1]) == 0) {
            // client 
            int port = atoi(argv[2]); 
            if (port <= 0) {
                WARNING() << "port conversion failed!"; 
            } else {
                single_remote_pseduobat(port);
            }
        } 
        else if (strcmp("server", argv[1]) == 0) {
            // server 
            int portmin = atoi(argv[2]);
            int portmax = atoi(argv[3]); 
            if (portmin <= 0 || portmax <= 0) {
                WARNING() << "port conversion failed!"; 
            } else if (portmin > portmax) {
                WARNING() << "portmin > portmax"; 
            } else {
                /// 
                aggregate_remote_pseudobat("127.0.0.1", portmin, portmax); 
            }
        }
        else if (strcmp("chain", argv[1]) == 0) {
            int myport = atoi(argv[2]);
            int remoteport = atoi(argv[3]); 
            if (myport <= 0 || remoteport <= 0) {
                WARNING() << "port conversion failed!";
            } else {
                chain_remote("127.0.0.1", remoteport, myport); 
            }
        }
        else if (strcmp("chainend", argv[1]) == 0) {
            int port = atoi(argv[2]);
            if (port <= 0) {
                WARNING() << "port conversion failed!";
            } else {
                chain_remote_end("127.0.0.1", port); 
            }
        }
    }
    google::protobuf::ShutdownProtobufLibrary(); 
#ifndef NO_PYTHON
    Py_END_ALLOW_THREADS
    Py_FinalizeEx();
#endif 
    // ERROR() << "Just to test abnormal return" << ", sys=" << sys << ", path=" << path;
    // std::cout << std::endl;
    return 0;
    
}


