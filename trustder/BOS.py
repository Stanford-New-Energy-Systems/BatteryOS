import signal
import time
import BatteryInterface
import JBDBMS
import VirtualBattery
import BOSControl
from BOSControl import BOSError
from collections import defaultdict
from util import *
from BOSErr import *
from BatteryDAG import *

class VirtualBattery(BatteryInterface.Battery):
    """
    This should be the BOS-local implementation of VirtualBattery 
    There should also be a user-local implementation 
    """
    def __init__(self, bos, reserve_capacity, current_capacity, max_discharging_current,
                 max_charging_current, sample_period): 
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

    def get_status(self):
        return BatteryInterface.BatteryStatus(\
            self._voltage, 
            self.actual_current, 
            self.remaining_capacity, 
            self.reserved_capacity, 
            self.current_range[0], 
            self.current_range[1])

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

    # split off battery w/ maximum capacity `maximum_capacity`
    # and current capacity `current_capacity`
    # returns (False, _) if either parameter exceeds what's available
    def split(self, maximum_capacity, current_capacity) -> VirtualBattery:
        if maximum_capacity > self.reserved_capacity:
            raise BOSError.InsufficientResources('maximum_capacity')
        elif current_capacity > self.current_capacity:
            raise BOSError.InsufficientResources('current_capacity')
        new_vb = VirtualBattery(self.bos, maximum_capacity, current_capacity,
                                self.max_discharging_current,
                                self.max_charging_current, self.sample_period)
        self.reserved_capacity -= maximum_capacity
        self.current_capacity -= current_capacity
        return new_vb

    def merge(self, other: VirtualBattery):
        self.reserved_capacity += other.reserved_capacity
        other.reserved_capacity = 0
        self.current_capacity += other.current_capacity
        other.current_capacity = 0


class AggregatorBattery: BatteryInterface.Battery:
    def __init__(self, voltage, voltage_tolerance, sample_period,
                 sources: [BatteryInterface.Battery]):
        self._sources = sources
        self._sample_period = sample_period

        if len(sources) == 0:
            raise BOSErr_NoBattery

        # check voltages within given tolerance
        for source in sources:
            v = source.get_voltage()
            if not (v >= voltage - voltage_tolerance and v <= voltage + voltage_tolerance):
                raise BOS_VoltageMismatch

    def refresh(self):
        for source in self._sources:
            source.refresh()
        # TODO: Check to make sure voltages still in range.

    def get_status(self):
        s_max_capacity = 0
        s_cur_capacity = 0
        s_max_discharge_time = 0
        s_max_charge_time = 0
        s_current = 0

        for source in self._sources:
            s_max_capacity += source.get_maximum_capacity()
            s_cur_capacity += source.get_current_capacity()
            (max_discharge_rate, max_charge_rate) = source.get_current_range()
            s_max_discharge_time = max(s_max_discharge_time,
                                       source.get_current_capacity() / max_discharge_rate)
            s_max_charge_time = max(s_max_charge_time, (source.get_maximum_capacity() -
                                                        source.get_current_capacity()) /
                                    max_charge_rate)
            s_current += source.get_current()
            
        s_max_discharge_rate = s_max_capacity / s_max_discharge_time
        s_max_charge_rate = s_max_capacity / s_max_charge_time

        return BatteryInterface.BatteryStatus(self._voltage,
                                              s_current,
                                              s_cur_capacity,
                                              s_max_capacity,
                                              s_max_discharge_rate,
                                              s_max_charge_rate)

    def set_current(self, target_current):
        (max_discharge_rate, max_charge_rate) = self.get_current_range()
        if target_current not in range(-max_discharge_rate, max_charge_rate):
            raise Exception('invalid current')

        if target_current > 0: # discharging
            charge = self.get_current_capacity()
            time = charge / target_current
            for source in self._sources:
                current = source.get_current_capacity() / time
                source.set_current(current)
        elif target_current < 0: # charging
            charge = self.get_maximum_capacity() - self.get_current_capacity()
            time = charge / -target_current
            for source in self._sources:
                current = (source.get_maximum_capacity() - source.get_current_capacity()) / time
                source.set_current(current)
        else: # none
            for source in self._sources:
                source.set_current(0)

class SplitterBattery: BatteryInterface.Battery:
    class Scale:
        def __init__(self, current_capacity, max_capacity, max_discharge_rate, max_charge_rate):
            for scale in [current_capacity, max_capacity, max_discharge_rate, max_charge_rate]:
                if not (scale >= 0.0 and scale <= 1.0):
                    raise BOSErr_InvalidArgument
            
            self._current_capacity = current_capacity
            self._max_capacity = max_capacity
            self._max_discharge_rate = max_discharge_rate
            self._max_charge_rate = max_charge_rate

        def get_current_capacity(self): return self._current_capacity
        def get_max_capacity(self): return self._max_capacity
        def get_max_discharge_rate(self): return self._max_discharge_rate
        def get_max_charge_rate(self): return self._max_charge_rate
        
    
    def __init__(self, sample_period, source, scale: Scale):
        self._sample_period = sample_period
        self._source = source

        # TODO: Refresh first?
        self._source_status = source.get_status()
        s = self._source_status

        self._current_capacity = s.current_capacity * scale.get_current_capacity()
        self._max_capacity = s.max_capacity * scale.get_max_capacity()
        self._max_discharge_rate = s.max_discharging_current * scale.get_max_discharge_rate()
        self._max_charge_rate = s.max_charging_current * scale.get_max_charge_rate()

        # TODO: Should current be a parameter? User can just call `set_current()` afterwards.
        self._current = 0;

    def refresh(self):
        self._source.refresh()

    def _update(self, snew: BatteryInterface.BatteryStatus):
        # update measurements given new status
        sold = self._source_status

        newloc = lambda oldloc, oldsrc, newsrc: (oldloc / oldsrc) * newsrc

        self._current = newloc(self._current, sold.current, snew.current)
        self._current_capacity = newloc(self._current_capacity,
                                        sold.current_capacity,
                                        snew.current_capacity)
        self._max_capacity = newloc(self._max_capacity, sold.max_capacity, snew.max_capacity)
        self._max_discharge_rate = newloc(self._max_discharge_rate,
                                          sold.max_discharging_current,
                                          snew.max_discharging_current)

        self._source_status = snew

        
    def get_status(self):
        self._update(source.get_status())
        s = self._source_status
        return BatteryInterface.BatteryStatus(s.voltage,
                                              self._current,
                                              self._current_capacity,
                                              self._max_capacity,
                                              self._max_discharge_rate,
                                              self._max_charge_rate)

    def set_current(self, target_current):
        s = self.get_status()
        min_current = -s.max_charging_current
        max_current = s.max_discharging_current

        # TODO: This should throw a "bad request" error, or something of the sort.
        assert target_current >= min_current and target_current <= max_current

        base = self._source_status.current
        diff = target_current - s.current

        new_source_current = base + diff
        assert new_source_current >= -self._source_status.max_charging_current and \
            new_source_current <= self._source_status.max_discharging_current
        
        source.set_current(new_source_current)
        
class BOS:
    def __init__(self):
        # Directory: map from battery names to battery objects
        self._dag = BatteryDAG()

        
    def make_battery(self, name: str, kind: BatteryInterface.Battery,
                     iface: Interface, addr: str) -> VirtualBattery:
        raise NotImplementedError

    
    def make_aggregator(self, name: str, sources: [str], voltage, voltage_tolerance, sample_period):
        # look up sources
        src_batteries = self._dag.get_battery(src) for src in sources
        ab = AggregatorBattery(voltage, voltage_tolerance, sample_period, src_batteries)
        self.add_parents(name, sources, ab)

        
    def make_splitter(self, parts, source: str, sample_period):
        # parts: dictionary from name to scale
        src_battery = self._dag.get_battery(source)
        
        batteries = dict()
        for name in parts:
            scale = parts[name]
            sb = SplitterBattery(sample_period, src_battery, scale)
            batteries[name] = sb

        self._dag.add_children(batteries, source)
