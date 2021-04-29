import signal
import time
import json
import BatteryInterface
from JBDBMS import JBDBMS
import Interface
from util import *
import BOSErr

class NullBattery(BatteryInterface.Battery):
    def __init__(self, voltage):
        self._voltage = voltage

    def refresh(self): return

    def get_status(self):
        return BatteryInterface.BatteryStatus(self._voltage, 0, 0, 0, 0, 0)

    def set_current(self, current):
        if current != 0:
            raise BOSErr.CurrentOutOfRange
        
    @staticmethod
    def type(): return "Null"

    _KEY_VOLTAGE = "voltage"
    
    def serialize(self):
        return self._serialize_base({self._KEY_VOLTAGE: self._voltage})

    @staticmethod
    def _deserialize_derived(d: dict):
        return NullBattery(d[self._KEY_VOLTAGE])

class PseudoBattery(BatteryInterface.BALBattery):
    def __init__(self, iface, addr, status: BatteryInterface.BatteryStatus):
        super().__init__(iface, addr)
        self._status = status

    def __str__(self): return str(self.serialize())
        
    @staticmethod
    def type(): return "Pseudo"

    def get_status(self): return self._status

    def set_status(self, status):
        self._status = status

    _KEY_IFACE = "iface"
    _KEY_ADDR = "addr"
    _KEY_STATUS = "status"

    def serialize(self):
        return self._serialize_base({self._KEY_STATUS: self._status.serialize()})

    @staticmethod
    def _deserialize_derived(d):
        return PseudoBattery(d[self._KEY_IFACE], d[self._KEY_ADDR], d[self._KEY_STATUS])

'''
class ForwardBattery(BatteryInterface.Battery):
    def __init__(self, source):
        self._source = source

    def refresh(self): self._source.refresh()
    def get_status(self): return self._source.get_status()
    def set_current(self, current): self._source.set_current(current)
'''
        
class AggregatorBattery(BatteryInterface.Battery):
    def __init__(self, voltage, voltage_tolerance, sample_period, srcnames: [str], lookup):
        '''
        `voltage` is the reported voltage of the aggregated battery. 
        `voltage_tolerance` indicates how much the source voltages are allowed to differ from the 
        virtual voltage.
        `sample_period` indicates how frequently the battery should be updated.
        `srcnames` is a list of battery names (strings)
        `lookup` is a function that resolves a battery name into a battery object.
        '''
        self._lookup = lookup
        self._srcnames = srcnames
        self._sample_period = sample_period
        self._voltage = voltage
        self._voltage_tolerance = voltage_tolerance

        if len(srcnames) == 0:
            raise BOSErr.NoBattery

        # check voltages within given tolerance
        for srcname in srcnames:
            source = self._lookup(srcname)
            v = source.get_voltage()
            if not (v >= voltage - voltage_tolerance and v <= voltage + voltage_tolerance):
                raise BOS_VoltageMismatch

    def refresh(self):
        for srcname in self._srcnames:
            self._lookup(srcname).refresh()
        # TODO: Check to make sure voltages still in range.

    def get_status(self):
        s_max_capacity = 0
        s_cur_capacity = 0
        s_max_discharge_time = 0
        s_max_charge_time = 0
        s_current = 0

        for srcname in self._srcnames:
            source = self._lookup(srcname)
            s_max_capacity += source.get_maximum_capacity()
            s_cur_capacity += source.get_current_capacity()
            (max_discharge_rate, max_charge_rate) = source.get_current_range()
            s_max_discharge_time = max(s_max_discharge_time,
                                       source.get_current_capacity() / max_discharge_rate)
            s_max_charge_time = max(s_max_charge_time, (source.get_maximum_capacity() -
                                                        source.get_current_capacity()) /
                                    max_charge_rate)
            s_current += source.get_current()

        s_max_discharge_rate = s_cur_capacity / s_max_discharge_time
        s_max_charge_rate = (s_max_capacity - s_cur_capacity) / s_max_charge_time

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

        sources = [self._lookup(srcname) for srcname in self._srcnames]

        if target_current > 0: # discharging
            charge = self.get_current_capacity()
            time = charge / target_current
            for source in sources:
                current = source.get_current_capacity() / time
                source.set_current(current)
        elif target_current < 0: # charging
            charge = self.get_maximum_capacity() - self.get_current_capacity()
            time = charge / -target_current
            for source in sources:
                current = (source.get_maximum_capacity() - source.get_current_capacity()) / time
                source.set_current(current)
        else: # none
            for source in sources:
                source.set_current(0)

    @staticmethod
    def type(): return "Aggregator"

    _KEY_SOURCES = "sources"
    _KEY_VOLTAGE = "voltage"
    _KEY_VOLTAGE_TOLERANCE = "voltage_tolerance"
    
    def serialize(self):
        return self._serialize_base({self._KEY_SOURCES: self._srcnames,
                                     self._KEY_VOLTAGE: self._voltage,
                                     self._KEY_VOLTAGE_TOLERANCE: self._voltage_tolerance})

    @staticmethod
    def _deserialize_derived(d: dict, sample_period, lookup):
        return AggregatorBattery(d[self._KEY_VOLTAGE],
                                 d[self._KEY_VOLTAGE_TOLERANCE],
                                 sample_period,
                                 d[self._KEY_SOURCES],
                                 lookup)
                                 

class SplitterBattery(BatteryInterface.Battery):
    class Scale:
        def __init__(self, current_capacity, max_capacity, max_discharge_rate,
                     max_charge_rate):
            for scale in [current_capacity, max_capacity, max_discharge_rate, max_charge_rate]:
                if not (scale >= 0.0 and scale <= 1.0):
                    raise BOSErr.InvalidArgument
            
            self._current_capacity = current_capacity
            self._max_capacity = max_capacity
            self._max_discharge_rate = max_discharge_rate
            self._max_charge_rate = max_charge_rate

        @staticmethod
        def scale_all(scale): return SplitterBattery.Scale(scale, scale, scale, scale)
            
        def get_current_capacity(self): return self._current_capacity
        def get_max_capacity(self): return self._max_capacity
        def get_max_discharge_rate(self): return self._max_discharge_rate
        def get_max_charge_rate(self): return self._max_charge_rate

        def serialize(self):
            return {"current_capacity": self._current_capacity,
                    "max_capacity": self._max_capacity,
                    "max_discharge_rate": self._max_discharge_rate,
                    "max_charge_rate": self._max_charge_rate}
                               
        
    
    def __init__(self, sample_period, srcname: str, scale: Scale, lookup):
        self._sample_period = sample_period
        self._srcname = srcname
        self._lookup = lookup
        source = self._source()

        # TODO: Refresh first?
        self._source_status = source.get_status()
        s = self._source_status

        self._current_capacity = s.current_capacity * scale.get_current_capacity()
        self._max_capacity = s.max_capacity * scale.get_max_capacity()
        self._max_discharge_rate = s.max_discharging_current * scale.get_max_discharge_rate()
        self._max_charge_rate = s.max_charging_current * scale.get_max_charge_rate()

        # TODO: Should current be a parameter? User can just call `set_current()` afterwards.
        self._current = 0;

    @staticmethod
    def type(): return "Splitter"

    def scale(self):
        s = self._source_status
        return SplitterBattery.Scale(self._current_capacity / s.current_capacity,
                                     self._max_capacity / s.max_capacity,
                                     self._max_discharge_rate / s.max_discharging_current,
                                     self._max_charge_rate / s.max_charging_current)
                     

    def _source(self) -> BatteryInterface.Battery: return self._lookup(self._srcname)

    def refresh(self):
        self._source().refresh()

    def _update(self, snew: BatteryInterface.BatteryStatus):
        # update measurements given new status
        sold = self._source_status

        newloc = lambda oldloc, oldsrc, newsrc: (oldloc / oldsrc) * newsrc

        if self._current >= 0:
            self._current = newloc(self._current, sold.max_discharging_current,
                                   snew.max_discharging_current)
        else:
            self._current = newloc(self._current, sold.max_charging_current,
                                   snew.max_charging_current)
        self._current_capacity = newloc(self._current_capacity,
                                        sold.current_capacity,
                                        snew.current_capacity)
        self._max_capacity = newloc(self._max_capacity, sold.max_capacity, snew.max_capacity)
        self._max_discharge_rate = newloc(self._max_discharge_rate,
                                          sold.max_discharging_current,
                                          snew.max_discharging_current)
        self._max_charge_rate = newloc(self._max_charge_rate,
                                       sold.max_charging_current,
                                       snew.max_charging_current)

        self._source_status = snew

        
    def get_status(self):
        self._update(self._source().get_status())
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
        
        self._source().set_current(new_source_current)

    _KEY_SOURCE = "source"
    _KEY_SCALE = "scale"
        
    def serialize(self):
        return self._serialize_base({self._KEY_SOURCE: self._srcname,
                                     self._KEY_SCALE: self.scale().serialize()})

    @staticmethod
    def _deserialize_derived(d: dict, sample_period, lookup):
        return SplitterBattery(sample_period,
                               d[self._KEY_SOURCE],
                               Scale.deserialize(d[self._KEY_SCALE]),
                               lookup)
                               
    

class BOSDirectory:
    def __init__(self):
        self._map = dict() # map battery names to (battery, children)

    def __contains__(self, name: str):
        return name in self._map

    def __getitem__(self, name):
        return self._map[name]

    def __str__(self):
        strd = dict([(k, tuple(map(str, v))) for (k, v) in self._map.items()])
        return str(strd)

    def ensure_name_free(self, name: str):
        if name in self: raise BOSErr.NameTaken

    def ensure_names_free(self, names: [str]):
        return all([name not in self for name in names])

    def ensure_name_taken(self, name: str):
        if name not in self: raise BOSErr.BadName

    def ensure_names_taken(self, names: [str]):
        return all([name in self for name in names])

    def add_battery(self, name: str, battery: BatteryInterface.Battery, children: set):
        self.ensure_name_free(name)
        self._map[name] = (battery, children)

    def free_battery(self, name: str):
        '''
        Removes all children from the given battery, so that it is free for any use.
        This is the only battery removal mechanism in order to maintain consistency in splitter
        batteries; it is impossible to remove one splitter battery without removing all of them.
        '''
        self.ensure_name_taken(name)
        def rec(name: str):
            (_, children) = self._map[name]
            # free all children
            for child in children:
                rec(child)
            # remove all children
            for child in set(children):
                del self._map[child]
                for (_, children) in self._map.values():
                    if child in children:
                        children.remove(child)
        rec(name)

    # def rename_battery(self, oldname, newname)
    # NOTE: Currently not implemented due to design constraints / inconsistencies it may cause.
    # It'd be hard to rename a battery and update the references in all the battery objects.

    def replace_battery(self, name: str, newbattery: BatteryInterface.BALBattery):
        assert isinstance(newbattery, BALBattery)
        self.enure_name_taken(name)
        assert len(self._map[name][1]) == 0
        self._map[name][0] = newbattery
        
        
class BOS:
    battery_types = dict([(battery.type(), battery) for battery in
                         [NullBattery, PseudoBattery, AggregatorBattery, SplitterBattery,
                          JBDBMS]])
    
    def __init__(self):
        # Directory: map from battery names to battery objects
        # self._dag = BatteryDAG()
        self._directory = BOSDirectory()
        self._ble = Interface.BLE()

    def __str__(self):
        return '{{directory = {}}}'.format(self._directory)

    def make_null(self, name: str, voltage) -> NullBattery:
        battery = NullBattery(voltage)
        self._directory.add_battery(name, battery, [])
        return battery
        
        
    def make_battery(self, name: str, kind: str, iface: Interface, addr: str, *args):
        battery = None
        if kind == JBDBMS.type():
            if iface != Interface.Interface.BLE:
                raise BOSErr.InvalidArgument
            battery = JBDBMS(addr, self._ble.connect(addr))
        else:
            battery_type = self.battery_types[kind]
            battery = battery_type(iface, addr, *args)
            self._directory.add_battery(name, battery, [])
        
        return battery
    
    
    def make_aggregator(self, name: str, sources: [str], voltage, voltage_tolerance, sample_period):
        battery = AggregatorBattery(voltage, voltage_tolerance, sample_period, sources,
                                    lambda name: self._directory[name][0]
                                    )
        self._directory.add_battery(name, battery, set(sources))
        return battery
        
    def make_splitter(self, parts, source: str, sample_period):
        # ensure all splitter names are available
        for (name, _) in parts:
            self._directory.ensure_names_free(name)

        # create splitters
        batteries = []
        for (name, scale) in parts:
            battery = SplitterBattery(sample_period, source, scale,
                                      lambda name: self._directory[name][0]
                                      )
            self._directory.add_battery(name, battery, {source})
            batteries.append(battery)

        return batteries

    def get_status(self, name: str) -> BatteryInterface.BatteryStatus:
        return self._directory[name].get_status()

    def free_battery(self, name: str): return self._directory.free_battery(name)
    
    def replace_battery(self, name: str, *args):
        newbattery = self.make_battery(name, *args)
        self._directory.replace_battery(name, newbattery)
        return battery

if __name__ == '__main__':
    bos = BOS()

    pseudo = bos.make_battery("pseudo1", "Pseudo", Interface.Interface.BLE, "pseudo1",
                              BatteryInterface.BatteryStatus(7000, 0, 500, 1000, 12, 12))
    splits = bos.make_splitter([("pseudo1a", SplitterBattery.Scale.scale_all(0.5)),
                                ("pseudo1b", SplitterBattery.Scale.scale_all(0.5))],
                               "pseudo1",
                               100)
    map(lambda s: s.refresh(), splits)
    
    print(bos)
    print(splits[0].get_status())

    pseudo.set_status(BatteryInterface.BatteryStatus(6000, 4, 600, 800, 14, 14))
    
    print(splits[0].get_status())
    print(splits[1].get_status())

    aggregator = bos.make_aggregator("agg", ["pseudo1a", "pseudo1b"], 6000, 500, 100)

    print(aggregator.get_status())

    # bos.free_battery("agg")

    print(bos)
