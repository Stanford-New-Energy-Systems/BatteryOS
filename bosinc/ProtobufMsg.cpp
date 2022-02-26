#include "ProtobufMsg.hpp" 

namespace protobufmsg {
int test_protobuf() {
    GOOGLE_PROTOBUF_VERIFY_VERSION; 
    bosproto::BatteryStatus battery_status_msg; 
    bosproto::CTimeStamp *ts = battery_status_msg.mutable_timestamp(); 
    ts->set_msec(100); 
    ts->set_secs_since_epoch(2147483647); 
    std::string sbuff = battery_status_msg.SerializeAsString();
    BatteryStatus bts; 
    bosproto::BatteryStatus parsed; 
    parsed.ParseFromString(sbuff); 
    deserialize(bts, &parsed); 
    CTimestamp cts = bts.timestamp; 
    // deserialize(cts, ts); 
    LOG() << cts.secs_since_epoch << " " << cts.msec;
    LOG() << "battery_status_msg.IsInitialized() = " << battery_status_msg.IsInitialized(); 
    return 0; 
}


int handle_battery_msg(Battery *bat, bosproto::BatteryMsg *msg, bosproto::BatteryResp *resp) {
    switch (msg->func_call_case()) {
        case bosproto::BatteryMsg::FuncCallCase::kGetInfo:
        case bosproto::BatteryMsg::FuncCallCase::kGetStatus: {
            BatteryStatus status = bat->get_status();
            bosproto::BatteryStatus *respstat = resp->mutable_status();
            serialize(respstat, status); 
            resp->set_retcode(0);  
            break;
        }
        case bosproto::BatteryMsg::FuncCallCase::kScheduleSetCurrent: {
            const bosproto::ScheduleSetCurrent &args = msg->schedule_set_current();
            CTimestamp when2set; 
            deserialize(when2set, &(args.when_to_set())); 
            CTimestamp untilwhen; 
            deserialize(untilwhen, &(args.until_when())); 
            uint32_t ret = bat->schedule_set_current(
                args.target_current_ma(), 
                false, 
                c_time_to_timepoint(when2set), 
                c_time_to_timepoint(untilwhen)
            ); 
            resp->set_set_current_response(ret); 
            resp->set_retcode(0); 
            break;
        }
        case bosproto::BatteryMsg::FuncCallCase::FUNC_CALL_NOT_SET:
            resp->set_retcode(-1); 
            resp->set_failreason("Function type not set!"); 
            break; 
    }
    return 0; 
}
int handle_admin_msg(BatteryOS *bos, bosproto::AdminMsg *msg, bosproto::AdminResp *resp) {
    switch (msg->func_call_case()) {
        case bosproto::AdminMsg::FuncCallCase::kMakeNull: {
            const bosproto::MakeNull &args = msg->make_null(); 
            Battery *bat = bos->get_manager().make_null(args.name(), args.voltage_mv(), args.max_staleness_ms()); 
            if (!bat) {
                resp->set_retcode(-1); 
                resp->set_failreason("failed to create battery");
            } else {
                resp->set_retcode(0); 
                resp->set_success(1); 
            }
            break; 
        }
        case bosproto::AdminMsg::FuncCallCase::kMakePseudo: {
            const bosproto::MakePseudo &args = msg->make_pseudo(); 
            BatteryStatus status; 
            deserialize(status, &(args.status())); 
            Battery *bat = bos->get_manager().make_pseudo(args.name(), status, args.max_staleness_ms()); 
            if (!bat) {
                resp->set_retcode(-1); 
                resp->set_failreason("failed to create battery");
            } else {
                resp->set_retcode(0); 
                resp->set_success(1); 
            }
            break;
        }
        case bosproto::AdminMsg::FuncCallCase::kMakeJBDBMS: {
            const bosproto::MakeJBDBMS &args = msg->make_jbdbms(); 
            Battery *bat = bos->get_manager().make_JBDBMS(args.name(), args.device_address(), args.current_regulator_address(), args.max_staleness_ms()); 
            if (!bat) {
                resp->set_retcode(-1); 
                resp->set_failreason("failed to create battery");
            } else {
                resp->set_retcode(0); 
                resp->set_success(1); 
            }
            break; 
        }
        case bosproto::AdminMsg::FuncCallCase::kMakeNetworked: {
            const bosproto::MakeNetworked &args = msg->make_networked(); 
            Battery *bat = bos->get_manager().make_networked_battery(args.name(), args.remote_name(), args.address(), args.port(), args.max_staleness_ms()); 
            if (!bat) {
                resp->set_retcode(-1); 
                resp->set_failreason("failed to create battery");
            } else {
                resp->set_retcode(0); 
                resp->set_success(1); 
            }
            break; 
        }
        case bosproto::AdminMsg::FuncCallCase::kMakeAggregator: {
            const bosproto::MakeAggregator &args = msg->make_aggregator(); 
            int num_src = args.src_names_size(); 
            std::vector<std::string> src_names; 
            for (int i = 0; i < num_src; ++i) {
                src_names.push_back(args.src_names(i)); 
            }
            Battery *bat = bos->get_manager().make_aggregator(args.name(), args.voltage_mv(), args.voltage_tolerance_mv(), src_names, args.max_staleness_ms()); 
            if (!bat) {
                resp->set_retcode(-1); 
                resp->set_failreason("failed to create battery");
            } else {
                resp->set_retcode(0); 
                resp->set_success(1); 
            }
            break; 
        }
        case bosproto::AdminMsg::FuncCallCase::kMakeSplitter: {
            const bosproto::MakeSplitter &args = msg->make_splitter(); 
            int num_cnames = args.child_names_size(); 
            int num_scales = args.child_scales_size(); 
            int num_max_staleness = args.child_max_staleness_ms_size(); 
            if (!(num_cnames == num_scales && num_scales == num_max_staleness)) {
                resp->set_retcode(-2); 
                resp->set_failreason("number of children is not equal to the number of scales or the number of max_staleness"); 
            } else {
                std::vector<std::string> child_names; 
                std::vector<Scale> child_scales; 
                std::vector<int64_t> child_staleness; 
                for (int i = 0; i < num_cnames; ++i) {
                    child_names.push_back(args.child_names(i)); 
                    Scale sc; 
                    deserialize(sc, &(args.child_scales(i))); 
                    child_scales.push_back(sc); 
                    child_staleness.push_back(args.child_max_staleness_ms(i)); 
                }
                std::vector<Battery*> bat = bos->get_manager().make_splitter(
                    args.name(), 
                    args.src_name(), 
                    child_names, 
                    child_scales, 
                    child_staleness, 
                    args.policy_type(), 
                    args.max_staleness_ms() 
                ); 
                if (bat.size() == 0) {
                    resp->set_retcode(-1); 
                    resp->set_failreason("failed to create partitioner"); 
                } else {
                    resp->set_retcode(0); 
                    resp->set_success(1); 
                }
            }

            break; 
        }
        case bosproto::AdminMsg::FuncCallCase::kMakeDynamic: {
            const bosproto::MakeDynamic &args = msg->make_dynamic(); 
            std::string delay_func_name = ""; 
            if (args.has_get_delay_func_name()) {
                delay_func_name = args.get_delay_func_name(); 
            }
            Battery *bat = bos->get_manager().make_dynamic(
                args.name(), 
                args.dynamic_lib_path(), 
                args.max_staleness_ms(), 
                args.init_argument().c_str(), 
                args.init_func_name(), 
                args.destruct_func_name(), 
                args.get_status_func_name(), 
                args.set_current_func_name(), 
                delay_func_name
            ); 
            if (!bat) {
                resp->set_retcode(-1); 
                resp->set_failreason("failed to create battery");
            } else {
                resp->set_retcode(0); 
                resp->set_success(1); 
            }
            break; 
        }
        case bosproto::AdminMsg::FuncCallCase::kRemoveBattery: {
            int retcode = bos->get_manager().remove_battery(msg->remove_battery().name()); 
            resp->set_retcode(retcode);
            resp->set_success(1); 
            break; 
        }
        case bosproto::AdminMsg::FuncCallCase::kListBattery: 
        case bosproto::AdminMsg::FuncCallCase::kGetBatteryInfo: 
            break; 
        case bosproto::AdminMsg::FuncCallCase::FUNC_CALL_NOT_SET: 
            resp->set_retcode(-1); 
            resp->set_failreason("Function type not set!"); 
            break; 
    }
    return 0; 
}



}

