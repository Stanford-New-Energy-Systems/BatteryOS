import json
from Interface import Interface

class BatteryStatus: 
    def __init__(self, 
        voltage, 
        current, 
        current_capacity, 
        max_capacity, 
        max_discharging_current, 
        max_charging_current,
        ): 
        self.voltage = voltage
        self.current = current
        self.current_capacity = current_capacity
        self.max_capacity = max_capacity
        self.max_discharging_current = max_discharging_current
        self.max_charging_current = max_charging_current

    def __str__(self):
        return '{{voltage = {}, current = {}, current_capacity = {}, max_capacity = {}, '\
            'max_discharging_current = {}, max_charging_current = {}}}'\
            .format(self.voltage, self.current, self.current_capacity, self.max_capacity,
                    self.max_discharging_current, self.max_charging_current)
    
    def serialize(self): 
        return {
            "voltage": self.voltage,
            "current": self.current, 
            "current_capacity": self.current_capacity, 
            "max_capacity": self.max_capacity,
            "max_discharging_current": self.max_discharging_current, 
            "max_charging_current": self.max_charging_current
        }
    
    def load_serialized(self, serialized):
        data = json.loads(serialized)
        self.voltage = data['voltage']
        self.current = data['current']
        self.current_capacity = data['current_capacity']
        self.max_capacity = data['max_capacity']
        self.max_discharging_current = data['max_discharging_current']
        self.max_charging_current = data['max_charging_current'] 
        return

class Battery: 
    """
    The interface used for communication between BOS & physical batteries and BOS & virtual batteries
    """
    def __init__(self): 
        raise NotImplementedError

    def __str__(self):
        return '{{config = {}, status = {}}}'.format(self.serialize(), self.get_status())

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

    def get_current_capacity(self): 
        """
        Returns the capacity remaining of the last refresh action
        """
        return self.get_status().current_capacity

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
    def __init__(self, iface: Interface, addr: str):
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
