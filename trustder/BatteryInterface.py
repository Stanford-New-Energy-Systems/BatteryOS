import json
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
    
    def serialize(self): 
        serialized = json.dumps({
            "voltage": self.voltage,
            "current": self.current, 
            "current_capacity": self.current_capacity, 
            "max_capacity": self.max_capacity,
            "max_discharging_current": self.max_discharging_current, 
            "max_charging_current": self.max_charging_current
        })
        return serialized
    
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
        pass
    def refresh(self): 
        """
        Refresh the state of the battery 

        this is the interface for physical battery, but it's required for virtual battery 
        because we couldn't distinguish between physical battery and virtual battery 
        i.e. we can use a virtual battery as a underlying battery of a BOS, 
        the BOS will keep calling this function to refresh the state 
        """
        pass
    def get_voltage(self): 
        """
        Return the voltage of the last refresh action
        """
        pass
    def get_current(self): 
        """
        Returns the current of the last refresh action
        """
        pass
    def get_maximum_capacity(self): 
        """
        Returns the maximum capacity of the last refresh action 
        """
        pass
    def get_current_capacity(self): 
        """
        Returns the capacity remaining of the last refresh action
        """
        pass
    def set_current(self, target_current):
        """
        Sets the current of the battery could pass through 
        could be positive or negative, 
        if positive, the battery is discharging, 
        if negative, the battery is charging 
        """ 
        pass
    def get_current_range(self): 
        """
        Returns (max_discharging_current, max_charging_current)
        e.g. (20, 20)
        """
        pass
    
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
        pass
