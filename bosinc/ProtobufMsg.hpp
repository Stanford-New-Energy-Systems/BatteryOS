#ifndef PROTOBUF_MSG_HPP 
#define PROTOBUF_MSG_HPP 

#include "admin_msg.pb.h"
#include "battery_msg.pb.h" 
#include "battery_status.pb.h"
#include "JBDBMS.hpp"
#include "TestBattery.hpp"
#include "NetworkBattery.hpp"
#include "AggregatorBattery.hpp"
#include "BALSplitter.hpp"
#include "SplittedBattery.hpp"
#include "RPC.hpp"
#include "Dynamic.hpp"
#include "BOS.hpp"
namespace protobufmsg {
int test_protobuf(); 


static inline int serialize(bosproto::CTimeStamp *msg, const CTimestamp &ts) {
    msg->set_secs_since_epoch(ts.secs_since_epoch); 
    msg->set_msec(ts.msec); 
    return 0; 
}
static inline int deserialize(CTimestamp &ts, const bosproto::CTimeStamp *msg) {
    ts.secs_since_epoch = msg->secs_since_epoch();
    ts.msec = msg->msec(); 
    return 0; 
}

static inline int serialize(bosproto::BatteryStatus *msg, const BatteryStatus &status) {
    bosproto::CTimeStamp *ts = msg->mutable_timestamp(); 
    serialize(ts, status.timestamp); 
    msg->set_voltage_mv(status.voltage_mV); 
    msg->set_current_ma(status.current_mA); 
    msg->set_remaining_charge_mah(status.capacity_mAh); 
    msg->set_full_charge_capacity_mah(status.max_capacity_mAh); 
    msg->set_max_charging_current_ma(status.max_charging_current_mA); 
    msg->set_max_discharging_current_ma(status.max_discharging_current_mA); 
    return 0; 
}
static inline int deserialize(BatteryStatus &status, const bosproto::BatteryStatus *msg) {
    deserialize(status.timestamp, &(msg->timestamp()));
    status.voltage_mV = msg->voltage_mv();
    status.current_mA = msg->current_ma(); 
    status.capacity_mAh = msg->remaining_charge_mah(); 
    status.max_capacity_mAh = msg->full_charge_capacity_mah(); 
    status.max_charging_current_mA = msg->max_charging_current_ma(); 
    status.max_discharging_current_mA = msg->max_discharging_current_ma(); 
    return 0; 
}

static inline int serialize(bosproto::Scale *msg, const Scale &scale) {
    msg->set_remaining_charge(scale.capacity); 
    msg->set_full_charge_capacity(scale.max_capacity); 
    msg->set_max_charge(scale.max_charge_rate); 
    msg->set_max_discharge(scale.max_discharge_rate); 
    return 0; 
}
static inline int deserialize(Scale &scale, const bosproto::Scale *msg) {
    scale.capacity = msg->remaining_charge(); 
    scale.max_capacity = msg->full_charge_capacity(); 
    scale.max_charge_rate = msg->max_charge(); 
    scale.max_capacity = msg->max_discharge(); 
    return 0; 
}

int handle_battery_msg(Battery *bat, bosproto::BatteryMsg *msg, bosproto::BatteryResp *resp); 
int handle_admin_msg(BatteryOS *bos, bosproto::AdminMsg *msg, bosproto::AdminResp *resp); 
}
#endif // ! PROTOBUF_MSG_HPP 
