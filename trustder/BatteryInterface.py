###############
# A collection of battery interfaces.
#
# This file defines two battery interfaces: BatteryStatus and Battery. Each is discussed in turn.
#
# BatteryStatus:
# This defines the battery status reported by a logical battery in BOS. This is a structure returned
# by the get_status() call of the BAL API.
#
# Battery:
# An abstract class representing a BOS logical battery, i.e., an object that conforms to the BAL
# API. For more info on the BAL API, see the BAL Design Document.
###############

import json
from Interface import Interface
from BOSNode import *
import BOS
import BOSErr
import util
import threading
import time
import enum

class BatteryStatus:
    VOLTAGE = 'voltage'
    CURRENT = 'current'
    SOC = 'state_of_charge'
    MAX_CAPACITY = 'max_capacity'
    MDC = 'max_discharging_current'
    MCC = 'max_charging_current'
    
    def __init__(self, 
        voltage, 
        current, 
        state_of_charge, 
        max_capacity, 
        max_discharging_current, 
        max_charging_current,
        ): 
        self.voltage = voltage
        self.current = current
        self.state_of_charge = state_of_charge
        self.max_capacity = max_capacity
        self.max_discharging_current = max_discharging_current
        self.max_charging_current = max_charging_current

    def __str__(self):
        return json.dumps(self.to_json())
    
    def serialize(self) -> dict: 
        return {
            BatteryStatus.VOLTAGE: self.voltage,
            BatteryStatus.CURRENT: self.current, 
            BatteryStatus.SOC: self.state_of_charge, 
            BatteryStatus.MAX_CAPACITY: self.max_capacity,
            BatteryStatus.MDC: self.max_discharging_current, 
            BatteryStatus.MCC: self.max_charging_current
        }
    
    def load_serialized(self, serialized_str: str):
        data = json.loads(serialized_str)
        # unpack the values
        self.voltage = data[BatteryStatus.VOLTAGE]
        self.current = data[BatteryStatus.CURRENT]
        self.state_of_charge = data[BatteryStatus.SOC]
        self.max_capacity = data[BatteryStatus.MAX_CAPACITY]
        self.max_discharging_current = data[BatteryStatus.MDC]
        self.max_charging_current = data[BatteryStatus.MCC] 
        return

    def to_json(self):
        return self.serialize()
    
    @staticmethod
    def from_json(json: dict):
        return BatteryStatus(
            json[BatteryStatus.VOLTAGE],
            json[BatteryStatus.CURRENT],
            json[BatteryStatus.SOC],
            json[BatteryStatus.MAX_CAPACITY],
            json[BatteryStatus.MDC],
            json[BatteryStatus.MCC],
        )

class Battery(BOSNode): 
    """
    The interface used for communication between BOS & physical batteries and BOS & virtual batteries
    """
    def __init__(self, name, sample_period=1000):
        self._name = name
        self._current = 0    # last measured current current
        self._meter = 0      # expected net charge of this battery. This must be initialized by BOS
        self._timestamp = util.bos_time()
        self._sample_period = sample_period
        self._lock = threading.RLock()
        self._background_refresh_thread = None
        self._should_background_refresh = False

    ## Below are the required inferfaces for the drivers to implement! 
    def set_current(self, target_current):
        """
        Sets the current of the battery could pass through 
        could be positive or negative, 
        if positive, the battery is discharging, 
        if negative, the battery is charging 
        """
        raise NotImplementedError

    def refresh(self):
        """
        Refresh the state of the battery and update the expected SOC (self._meter).
        For physical batteries, both tasks will be performed.
        For virtual batteries, only the latter will be performed (updating the expected SOC).

        DOC: doesn't propogate refreshes to source batteries; should be independent
        """
        raise NotImplementedError

    def get_status(self) -> BatteryStatus: 
        """
        Gets voltage, current, current capacity, max capacity, max_discharging_current, max_charging_current
        This method should return BatteryStatus
        """
        raise NotImplementedError

    @staticmethod
    def type() -> str:
        """
        Just the name of the driver
        """
        raise NotImplementedError


    ## Optional 
    def serialize(self):
        raise NotImplementedError

    @staticmethod
    def _deserialize_derived(d: dict):
        raise NotImplementedError

    ## End required functions


    def __str__(self):
        with self._lock:
            return '{{config = {}, status = {}}}'.format(self.serialize(), self.get_status())

    def name(self):
        with self._lock:
            return self._name

    def start_background_refresh(self):
        with self._lock:
            if self._sample_period < 0:
                return
            elif self._should_background_refresh or self._background_refresh_thread != None:
                raise BOSErr.InvalidArgument # already refreshing
            self._should_background_refresh = True
            self._background_refresh_thread = \
                threading.Thread(target=self._background_refresh, args=())
            self._background_refresh_thread.start()
    
    def stop_background_refresh(self):
        with self._lock:
            self._should_background_refresh = False
            self._background_refresh_thread = None

    def _background_refresh(self):
        while self._should_background_refresh:
            with self._lock:
                self.refresh()
                # print('{} status: {}'.format(self._name, self.get_status()))
            time.sleep(self._sample_period / 1000)
            
    def get_voltage(self): 
        """
        Return the voltage of the last refresh action
        """
        return self.get_status().voltage

    def get_current(self): 
        """
        Returns the current of the last refresh action
        """
        return self.get_status().current
        
    def get_maximum_capacity(self): 
        """
        Returns the maximum capacity of the last refresh action 
        """
        return self.get_status().max_capacity

    def get_state_of_charge(self): 
        """
        Returns the capacity remaining of the last refresh action
        """
        return self.get_status().state_of_charge 
    
    def get_current_range(self): 
        """
        Returns (max_discharging_current, max_charging_current)
        e.g. (20, 20)
        """
        s = self.get_status()
        return (s.max_discharging_current, s.max_charging_current)
    
    # def set_max_staleness(self, ms):
    #     """
    #     the maximum stale time of a value 
    #     the virtual battery should query information every time 
    #     """
    #     pass

    def update_meter(self, endcurrent, newcurrent):
        '''
        Approximate the total charge that has passed through the battery.
        `endcurrent` is the current that the battery has just been measured at (before modification).
        `newcurrent` is the current that the battery has just been set to (after modification).
        This function is general enough to work for background refresh() updates, which will call 
        this with `endcurrent == newcurrent`, as well as set_current() requests, which will generally
        call this with `endcurrent != newcurrent`.
        '''
        with self._lock:
            begincurrent = self._current
            endtimestamp = util.bos_time()
            duration = endtimestamp - self._timestamp
            
            # take the average of start and end currents for duration
            amp_hours = ((begincurrent + endcurrent) / 2) * (duration / 3600)
            self._meter -= amp_hours
    
            # update meter info
            self._current = newcurrent
            self._timestamp = endtimestamp

    def get_meter(self):
        with self._lock:
            return self._meter

    # NOTE: Should this be more protected?
    # This is public because it's needed by splitter policies which update expected SOC when
    # batteries are created or destroyed.
    def set_meter(self, newvalue):
        with self._lock:
            self._meter = newvalue

    def reset_meter(self):
        with self._lock:
            self._meter = self.get_status().state_of_charge
    
    _KEY_TYPE = "type"
    
    def _serialize_base(self, kvs):
        """
        Private method for serializing given derived class key-value pairs. 
        This is a protected method, i.e. should be called by derived classes when serializing.
        """
        kvs[self._KEY_TYPE] = self.type()
        return kvs

    @staticmethod
    def deserialize(d: dict, types: dict):
        if not isinstance(d, dict): 
            raise BOSErr.InvalidArgument
        tstr = d[Battery._KEY_TYPE]
        t = types[tstr]
        return t._deserialize_derived(d)

class PhysicalBattery(Battery):
    def __init__(self, name: str, iface: Interface, addr: str, sample_period=-1):
        super().__init__(name, sample_period)
        assert type(self) != PhysicalBattery # abstract class
        self._iface = iface
        self._addr = addr

    _KEY_IFACE = "iface"
    _KEY_ADDR = "addr"
    
    def _serialize_base(self, d: dict) -> str:
        d[self._KEY_IFACE] = self._iface.value
        d[self._KEY_ADDR] = self._addr
        return super()._serialize_base(d)

    def serialize(self) -> str:
        with self._lock:
            return self._serialize_base({})


# TODO: class Scale belongs in Policy.py, not here.
class Scale:
    def __init__(self, state_of_charge, max_capacity, max_discharge_rate,
                 max_charge_rate):
        for scale in [state_of_charge, max_capacity, max_discharge_rate, max_charge_rate]:
            if not (scale >= 0.0 and scale <= 1.0):
                raise BOSErr.InvalidArgument
            
        self._state_of_charge = state_of_charge
        self._max_capacity = max_capacity
        self._max_discharge_rate = max_discharge_rate
        self._max_charge_rate = max_charge_rate

    def __str__(self) -> str:
        return '{{soc = {}, max_capacity = {}, max_discharge = {}, max_charge = {}}}'.format(
            self._state_of_charge,
            self._max_capacity,
            self._max_discharge_rate,
            self._max_charge_rate,
            )

    @staticmethod
    def scale_all(scale): return BOS.SplitterBattery.Scale(scale, scale, scale, scale)
    
    def get_state_of_charge(self): return self._state_of_charge
    def get_max_capacity(self): return self._max_capacity
    def get_max_discharge_rate(self): return self._max_discharge_rate
    def get_max_charge_rate(self): return self._max_charge_rate
    
    def serialize(self):
        return {"state_of_charge": self._state_of_charge,
                "max_capacity": self._max_capacity,
                "max_discharge_rate": self._max_discharge_rate,
                "max_charge_rate": self._max_charge_rate}
    

    def __sub__(self, other):
        if self._state_of_charge >= other._state_of_charge and \
           self._max_capacity >= other._max_capacity and \
           self._max_discharge_rate >= other._max_discharge_rate and \
           self._max_charge_rate >= other._max_charge_rate:
            return Scale(self._state_of_charge - other._state_of_charge,
                         self._max_capacity - other._max_capacity,
                         self._max_discharge_rate - other._max_discharge_rate,
                         self._max_charge_rate - other._max_charge_rate,
                         )
        else:
            raise BOSErr.NoResources
        
        
