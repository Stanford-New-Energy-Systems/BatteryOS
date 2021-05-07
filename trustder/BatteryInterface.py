import json
from Interface import Interface
from BOSNode import *
import util

class BatteryStatus: 
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
        return '{{voltage = {}, current = {}, state_of_charge = {}, max_capacity = {}, '\
            'max_discharging_current = {}, max_charging_current = {}}}'\
            .format(self.voltage, self.current, self.state_of_charge, self.max_capacity,
                    self.max_discharging_current, self.max_charging_current)
    
    def serialize(self): 
        return {
            "voltage": self.voltage,
            "current": self.current, 
            "state_of_charge": self.state_of_charge, 
            "max_capacity": self.max_capacity,
            "max_discharging_current": self.max_discharging_current, 
            "max_charging_current": self.max_charging_current
        }
    
    def load_serialized(self, serialized):
        data = json.loads(serialized)
        self.voltage = data['voltage']
        self.current = data['current']
        self.state_of_charge = data['state_of_charge']
        self.max_capacity = data['max_capacity']
        self.max_discharging_current = data['max_discharging_current']
        self.max_charging_current = data['max_charging_current'] 
        return

class Battery(BOSNode): 
    """
    The interface used for communication between BOS & physical batteries and BOS & virtual batteries
    """
    def __init__(self, name):
        self._name = name
        self._current = 0    # last measured current current
        self._meter = 0 # expected net charge of this battery. This must be initialized by BOS
        self._timestamp = util.bos_time()

    def __str__(self):
        return '{{config = {}, status = {}}}'.format(self.serialize(), self.get_status())

    def name(self):
        return self._name

    def refresh(self): 
        """
        Refresh the state of the battery 

        this is the interface for physical battery, but it's required for virtual battery 
        because we couldn't distinguish between physical battery and virtual battery 
        i.e. we can use a virtual battery as a underlying battery of a BOS, 
        the BOS will keep calling this function to refresh the state 
        """
        raise NotImplementedError
    
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

    def set_current(self, target_current):
        """
        Sets the current of the battery could pass through 
        could be positive or negative, 
        if positive, the battery is discharging, 
        if negative, the battery is charging 
        """
        raise NotImplementedError
    
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

    def get_status(self): 
        """
        Gets voltage, current, current capacity, max capacity, max_discharging_current, max_charging_current
        """
        raise NotImplementedError

    def update_meter(self, endcurrent, newcurrent):
        '''
        Approximate the total charge that has passed through the battery.
        `endcurrent` is the current that the battery has just been measured at (before modification).
        `newcurrent` is the current that the battery has just been set to (after modification).
        This function is general enough to work for background refresh() updates, which will call 
        this with `endcurrent == newcurrent`, as well as set_current() requests, which will generally
        call this with `endcurrent != newcurrent`.
        '''
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
        return self._meter

    # NOTE: Should this be more protected?
    # This is public because it's needed by splitter policies which update expected SOC when
    # batteries are created or destroyed.
    def set_meter(self, newvalue):
        self._meter = newvalue

    def reset_meter(self):
        self._meter = self.get_status().state_of_charge
        

    _KEY_TYPE = "type"
    
    @staticmethod
    def type() -> str:
        raise NotImplementedError

    def _serialize_base(self, kvs):
        """
        Private method for serializing given derived class key-value pairs. 
        This is a protected method, i.e. should be called by derived classes when serializing.
        """
        kvs[self._KEY_TYPE] = self.type()
        return kvs
    
    def serialize(self):
        raise NotImplementedError

    @staticmethod
    def _deserialize_derived(d: dict):
        raise NotImplementedError

    @staticmethod
    def deserialize(d: dict, types: dict):
        if not isinstance(d, dict): raise BOSErr.InvalidArgument
        tstr = d[self._KEY_TYPE]
        t = types[tstr]
        return t._deserialize_derived(d)

class BALBattery(Battery):
    def __init__(self, name: str, iface: Interface, addr: str):
        super().__init__(name)
        assert type(self) != BALBattery # abstract class
        self._iface = iface
        self._addr = addr

    _KEY_IFACE = "iface"
    _KEY_ADDR = "addr"
    
    def _serialize_base(self, d: dict) -> str:
        d[self._KEY_IFACE] = self._iface.value
        d[self._KEY_ADDR] = self._addr
        return super()._serialize_base(d)

    def serialize(self) -> str:
        return self._serialize_base({})



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
    def scale_all(scale): return SplitterBattery.Scale(scale, scale, scale, scale)
    
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
        
        
