import signal
import time
import BatteryInterface
import JBDBMS
import VirtualBattery
import BOSControl
import Interface
from BOSControl import BOSError
from collections import defaultdict
from util import *
import BOSErr
from BatteryDAG import *

class NullBattery(BatteryInterface.Battery):
    def __init__(self, voltage):
        self._voltage = voltage

    def refresh(self): return

    def get_status(self):
        return BatteryInterface.BatteryStatus(self._voltage, 0, 0, 0, 0, 0)

    def set_current(self, current):
        if current != 0:
            raise BOSErr.CurrentOutOfRange

        
class AggregatorBattery(BatteryInterface.Battery):
    def __init__(self, voltage, voltage_tolerance, sample_period,
                 sources: [BatteryInterface.Battery]):
        self._sources = sources
        self._sample_period = sample_period

        if len(sources) == 0:
            raise BOSErr.NoBattery

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
            raise BOSErr.CurrentOutOfRange

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


class SplitterBattery(BatteryInterface.Battery):
    class Scale:
        def __init__(self, current_capacity, max_capacity, max_discharge_rate, max_charge_rate):
            for scale in [current_capacity, max_capacity, max_discharge_rate, max_charge_rate]:
                if not (scale >= 0.0 and scale <= 1.0):
                    raise BOSErr.InvalidArgument
            
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
        self._ble = Interface.BLE()

        
    def make_battery(self, name: str, kind: BatteryInterface.Battery,
                     iface: Interface, addr: str) -> VirtualBattery:

        if kind == JBDBMS:
            if iface != Interface.BLE:
                raise BOSErr.InvalidArgument
            return JBDBMS(self._ble.connect(addr))

        raise BOSErr.InvalidArgument

    
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


