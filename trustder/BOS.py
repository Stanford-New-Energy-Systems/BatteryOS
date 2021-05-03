import signal
import time
import json
from BatteryInterface import *
from BOSNode import *
from JBDBMS import JBDBMS
import Interface
from util import *
import BOSErr
from Policy import *

class NullBattery(Battery):
    def __init__(self, name, voltage):
        super().__init__(name)
        self._voltage = voltage

    def refresh(self): return

    def get_status(self):
        return BatteryStatus(self._voltage, 0, 0, 0, 0, 0)

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

class PseudoBattery(BALBattery):
    def __init__(self, name, iface, addr, status: BatteryStatus):
        super().__init__(name, iface, addr)
        self._status = status

    def __str__(self): return str(self.serialize())
        
    @staticmethod
    def type(): return "Pseudo"

    def get_status(self): return self._status

    def set_current(self, target_current):
        if not (target_current >= -self._status.max_charging_current and
                target_current <= self._status.max_discharging_current):
            raise BOSErr.CurrentOutOfRange
        self._status.current = target_current

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

        
class AggregatorBattery(Battery):
    def __init__(self, name, voltage, voltage_tolerance, sample_period, srcnames: [str], lookup):
        '''
        `voltage` is the reported voltage of the aggregated battery. 
        `voltage_tolerance` indicates how much the source voltages are allowed to differ from the 
        virtual voltage.
        `sample_period` indicates how frequently the battery should be updated.
        `srcnames` is a list of battery names (strings)
        `lookup` is a function that resolves a battery name into a battery object.
        '''
        super().__init__(name)
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
                raise BOSErr.VoltageMismatch

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
            s_cur_capacity += source.get_state_of_charge()
            (max_discharge_rate, max_charge_rate) = source.get_current_range()
            s_max_discharge_time = max(s_max_discharge_time,
                                       source.get_state_of_charge() / max_discharge_rate)
            s_max_charge_time = max(s_max_charge_time, (source.get_maximum_capacity() -
                                                        source.get_state_of_charge()) /
                                    max_charge_rate)
            s_current += source.get_current()

        s_max_discharge_rate = s_cur_capacity / s_max_discharge_time
        s_max_charge_rate = (s_max_capacity - s_cur_capacity) / s_max_charge_time

        return BatteryStatus(self._voltage,
                             s_current,
                             s_cur_capacity,
                             s_max_capacity,
                             s_max_discharge_rate,
                             s_max_charge_rate)

    def set_current(self, target_current):
        (max_discharge_rate, max_charge_rate) = self.get_current_range()
        if not (target_current >= -max_charge_rate and target_current <= max_discharge_rate):
            raise BOSErr.CurrentOutOfRange

        sources = [self._lookup(srcname) for srcname in self._srcnames]

        if target_current > 0: # discharging
            charge = self.get_state_of_charge()
            time = charge / target_current
            for source in sources:
                current = source.get_state_of_charge() / time
                source.set_current(current)
        elif target_current < 0: # charging
            charge = self.get_maximum_capacity() - self.get_state_of_charge()
            time = charge / -target_current
            for source in sources:
                current = (source.get_maximum_capacity() - source.get_state_of_charge()) / time
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
                                 

class SplitterBattery(Battery):
    '''
    The splitter battery delegates most of the work to its associated splitter policy.
    '''
    
    def __init__(self, name, sample_period, policyname, lookup):
        super().__init__(name)
        self._sample_period = sample_period
        self._policyname = policyname
        self._lookup = lookup

    @staticmethod
    def type(): return "Splitter"

    def _policy(self): return self._lookup(self._policy)
                     
    def refresh(self):
        self._policy().refresh()
        
    def get_status(self):
        return self._policy().get_status(self._name)

    def set_current(self):
        return self._policy().set_current(self._name)

    

class BOSDirectory:
    def __init__(self):
        self._map = dict() # map battery names to (battery, children)

    def __contains__(self, name: str):
        return name in self._map

    def __getitem__(self, name):
        if name not in self:
            raise BOSErr.BadName
        return self._map[name]

    def __str__(self):
        strd = dict([(k, tuple(map(str, v))) for (k, v) in self._map.items()])
        return str(strd)

    def add_node(self, name: str, node: BOSNode, parents=set()):
        if name in self: raise BOSErr.NameTaken
        self._map[name] = (node, parents)

    def remove_node(self, name: str):
        raise NotImplementedError

    # def free_battery(self, name: str):
    #     '''
    #     Removes all children from the given battery, so that it is free for any use.
    #     This is the only battery removal mechanism in order to maintain consistency in splitter
    #     batteries; it is impossible to remove one splitter battery without removing all of them.
    #     '''
    #     self.ensure_name_taken(name)
    #     def rec(name: str):
    #         (_, children) = self._map[name]
    #         # free all children
    #         for child in children:
    #             rec(child)
    #         # remove all children
    #         for child in set(children):
    #             del self._map[child]
    #             for (_, children) in self._map.values():
    #                 if child in children:
    #                     children.remove(child)
    #     rec(name)

    # def rename_battery(self, oldname, newname)
    # NOTE: Currently not implemented due to design constraints / inconsistencies it may cause.
    # It'd be hard to rename a battery and update the references in all the battery objects.

    def replace_node(self, name: str, newnode: BOSNode):
        raise NotImplementedError
    # def replace_battery(self, name: str, newbattery: BALBattery):
    #     assert isinstance(newbattery, BALBattery)
    #     self.enure_name_taken(name)
    #     assert len(self._map[name][1]) == 0
    #     self._map[name][0] = newbattery

    def print_status(self):
        for src in self._map:
            status = self._map[src][0].get_status()
            print('{}:'.format(src))
            print('    current:         {}A'.format(status.current))
            print('    state of charge: {}Ah'.format(status.state_of_charge))
            print('    max capacity:    {}Ah'.format(status.max_capacity))
            print('    max discharge:   {}A'.format(status.max_discharging_current))
            print('    max charge:      {}A'.format(status.max_charging_current))
    
    def visualize(self):
        import networkx as nx
        import matplotlib.pyplot as plt
        g = nx.DiGraph()
        for src in self._map:
            for dst in self._map[src][1]:
                g.add_edge(dst, src)
        nx.draw_networkx(g)
        plt.show()

    
        
class BOS:
    battery_types = dict([(battery.type(), battery) for battery in
                         [NullBattery, PseudoBattery, AggregatorBattery, SplitterBattery,
                          JBDBMS]])
    
    def __init__(self):
        # Directory: map from battery names to battery objects
        self._directory = BOSDirectory()
        # self._ble = Interface.BLE()
        self._lookup = lambda name: self._directory[name][0]

    def __str__(self):
        return '{{directory = {}}}'.format(self._directory)

    def make_null(self, name: str, voltage) -> NullBattery:
        battery = NullBattery(name, voltage)
        self._directroy.add_null(name, battery)
        return battery
        
        
    def make_battery(self, name: str, kind: str, iface: Interface, addr: str, *args):
        if kind == JBDBMS.type():
            if iface != Interface.BLE:
                raise BOSErr.InvalidArgument
            battery = JBDBMS(name, addr, self._ble.connect(addr))
        else:
            battery_type = self.battery_types[kind]
            battery = battery_type(name, iface, addr, *args)
        self._directory.add_node(name, battery)
        return battery
    
    
    def make_aggregator(self, name: str, sources: [str], voltage, voltage_tolerance, sample_period):
        battery = AggregatorBattery(name, voltage, voltage_tolerance, sample_period, sources,
                                    self._lookup
                                    )
        self._directory.add_node(name, battery, set(sources))
        return battery

    def make_splitter_policy(self, policyname: str, policytype, parent: str, *policyargs) -> SplitterPolicy:
        if not issubclass(policytype, SplitterPolicy):
            raise BOSErr.InvalidArgument
        policy = policytype(*policyargs, self._lookup)
        self._directory.add_node(policyname, policy, {parent})
        return policy

    def make_splitter_battery(self, batteryname: str, policyname: str, sample_period, *policyargs):
        battery = SplitterBattery(batteryname, sample_period, policyname, self._lookup)
        self._directory.add_node(batteryname, battery, {policyname})
        policy = self._lookup(policyname)
        policy.add_child(batteryname, *policyargs)
        return battery
        

    def get_status(self, name: str) -> BatteryStatus:
        return self._directory[name].get_status()

    def free_battery(self, name: str): return self._directory.free_battery(name)
    
    def replace_battery(self, name: str, *args):
        newbattery = self.make_battery(name, *args)
        self._directory.replace_battery(name, newbattery)
        return battery

    def print_status(self):
        self._directory.print_status()
    
    def visualize(self):
        self._directory.visualize()
            
if __name__ == '__main__':
    bos = BOS()

    pseudo1 = bos.make_battery("pseudo1", "Pseudo", Interface.BLE, "pseudo1",
                              BatteryStatus(7000, 0, 500, 1000, 12, 12))
    pseudo2 = bos.make_battery("pseudo2", "Pseudo", Interface.BLE, "pseudo2",
                               BatteryStatus(7000, 0, 500, 1000, 12, 12))
    agg = bos.make_aggregator("agg", ["pseudo1", "pseudo2"], 7000, 500, 100)

    bos.print_status()
    
    agg.set_current(12)

    bos.print_status()
    
    # bos.visualize()
    
