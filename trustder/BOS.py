import signal
import time
import BatteryInterface
import JBDBMS
import VirtualBattery

class VirtualBattery(BatteryInterface.Battery):
    """
    This should be the BOS-local implementation of VirtualBattery 
    There should also be a user-local implementation 
    """
    def __init__(self, bos, reserve_capacity, max_discharging_current, max_charging_current, sample_period): 
        self.bos = bos
        self.chargeable = False
        self.dischargeable = True
        
        # voltage 
        self._voltage = bos.get_voltage()

        self._current_range = range(-max_charging_current, max_discharging_current)
        self.current_range = (max_discharging_current, max_charging_current)

        self.actual_current = 0
        self.claimed_current = 0
        
        self.reserved_capacity = reserve_capacity
        self.remaining_capacity = reserve_capacity

        self.sample_period = sample_period

        # how does the actual drawing vs the claimed drawing compare
        # if you claimed to draw 1A, but you drawn 2A, your credit will be -1Ah after an hour! 
        self.credit = 0
    
    def refresh(self): 
        # self._bos.refresh_all()
        # refresh the virtual batteries 
        # and update the capacity according to the measured current values 
        # what TODO  
        # the underlying BOS will handle the refresh of the physical batteries 

        # perhaps refresh the associated current meter? 
        # this should be from a current meter, we just assume that we have it 
        # but we don't have it right now, we just assume we have it 
        self.actual_current = self.read_current_meter()  # TODO
        # also estimate the remaining capacity 
        self._voltage = self.bos.get_voltage()
        self.remaining_capacity -= (self.claimed_current * self._voltage) * self.sample_period
        self.credit = (self.claimed_current - self.actual_current) * self._voltage * self.sample_period
        # perhaps use a more accurate approach... 

        if self.get_current_capacity <= 0: 
            self._set_dischargeable(False)
        else: 
            self._set_dischargeable(True)
        if self.get_current_capacity >= self.get_maximum_capacity(): 
            self._set_chargeable(False)
        else: 
            self._set_chargeable(True)
        pass

    def read_current_meter(self): 
        """
        TODO 
        Let the BOS know the user-side current meter reading 
        """
        self.actual_current = 0
        return self.actual_current
    
    def get_voltage(self): 
        """
        the voltage is fixed 
        """
        return self._voltage
    
    def get_current(self): 
        """
        The actual current the you are drawing 
        """
        return self.actual_current
    
    def get_maximum_capacity(self): 
        """
        Just the reserved capacity 
        """
        return self.reserved_capacity
    
    def get_current_capacity(self): 
        """
        if you keep running at your claimed current, what's your remaining charge... 
        """
        return self.remaining_capacity

    def set_current(self, target_current): 
        """
        Now submit a current contract 
        it's a contract that the user is drawing target_current in the future  
        if charging, please set it to negative current 
        otherwise it's positive 

        return value: fail / succeed 
        """
        if (target_current not in self._current_range): 
            return False
        if (target_current > 0 and self.get_current_capacity() <= 0): 
            # self._set_dischargeable(False)
            return False
        if (target_current < 0 and self.get_current_capacity() >= self.get_maximum_capacity()): 
            # self._set_chargeable(False)
            return False 
        
        self.claimed_current = target_current
        # # BOS side don't need to tell the BOS
        # self.bos.currrent_change()
        return True

    def get_current_range(self): 
        """
        Returns (max_discharging_current, max_charging_current)
        e.g. (20, 20)
        """
        return self.current_range

    def get_credit(self): 
        """
        how much do I owe? 
        """
        return self.credit

    def _set_chargeable(self, is_chargeable): 
        """
        should be able to shutdown the virtual battery 
        """
        self.chargeable = is_chargeable
        # TODO is there a way to regulate this? 
    
    def _set_dischargeable(self, is_dischargeable): 
        """
        should be able to shutdown the virtual battery 
        """
        self.dischargeable = is_dischargeable
        # TODO is there a way to regulate this? 





class BOS: 
    def __init__(self, voltage, bms_list, sample_period):
        self.bms_list = bms_list 
        self.virtual_battery_list = []
        self._voltage = voltage
        self.sample_period = sample_period
        self.net_current = 0

    def new_virtual_battery(self, reserve_capacity, max_discharging_current, max_charging_current): 
        total_capacity = 0
        capacity_remaining = 0
        max_charging_current = 0
        max_discharging_current = 0 
        net_claimed_current = 0
        for vb in self.virtual_battery_list: 
            total_capacity += vb.get_maximum_capacity()
            capacity_remaining += vb.get_current_capacity()
            current_range = vb.get_current_range()
            max_discharging_current += current_range[0]
            max_charging_current += current_range[1]
            net_claimed_current += vb.claimed_current

        # check capacity
        if capacity_remaining <= reserve_capacity: 
            return None
        # check max current 
        if (net_claimed_current < 0 and (-net_claimed_current) >= max_charging_current) or \
            (net_claimed_current > 0 and net_claimed_current >= max_discharging_current): 
            return None

        v_battery = VirtualBattery(\
            self, reserve_capacity, max_discharging_current, max_charging_current, self.sample_period)
        self.virtual_battery_list.append(v_battery)
        return v_battery

    def refresh_all(self): 
        """
        refresh the states of physical batteries 
        and get the actual currents of the virtual batteries 
        also distribute the currents accordingly to the physical batteries 
        """
        # also refresh the bus voltage! 
        # should get the real time voltage from the bus 
        # but we don't have it yet 
        self._voltage = 3
        # refresh end-point currents of the virtual batteries 
        actual_net_current = 0
        for vb in self.virtual_battery_list: 
            vb.refresh()
            actual_net_current += vb.read_current_meter()
        # refresh the physical batteries  
        for bms in self.bms_list: 
            bms.refresh()
        # distribute the currents to the batteries 
        # now we just fairly distribute the currents 
        # huge assumption! 
        # should fix 
        bms_current = actual_net_current / len(self.bms_list)
        for bms in self.bms_list: 
            bms.set_current(bms_current)
        return
    
    def get_voltage(self): 
        return self._voltage

    def get_maximum_capacity(self): 
        max_capacity = 0
        for bms in self.bms_list: 
            max_capacity += bms.get_maximum_capacity()
        return max_capacity

    def get_current_capacity(self): 
        current_capacity = 0
        for bms in self.bms_list: 
            current_capacity += bms.get_current_capacity()
        return current_capacity

    # def currrent_change(self): 
    #     self.refresh_all()
    #     virtual_battery_currents = 0
    #     for vb in self.virtual_battery_list:
    #         virtual_battery_currents += vb.current
        

    def main_loop(self): 
        while 1: 
            # handle inputs 
            # polling from connections 
            # refresh 
            self.refresh_all()
            time.sleep(self.sample_period)
            # handle output requests 
            # for each request, send back a reply 
            # something else to do 
            pass



