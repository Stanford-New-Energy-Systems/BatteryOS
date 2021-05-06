import BOS
import util
import BOSErr
from BatteryInterface import BatteryStatus
import Interface
import Policy

class Interpreter:
    def __init__(self):
        util.bos_time = util.DummyTime(0)
        self._bos = BOS.BOS()
        self._commands = {"make": self._make,
                          "mk": self._make,
                          "tick": self._tick,
                          "time": self._time,
                          "stat": self._stat,
                          "list": self._list,
                          "ls":   self._list,
                          }
        
    def run(self):
        while True:
            try:
                line = input('> ')
            except EOFError:
                print('')
                return
            args = line.split()
            if len(args) == 0:
                continue
            cmd = args[0]
            args = args[1:]
            if cmd not in self._commands:
                print("Unrecognized command '{}'.".format(cmd))
            try:
                self._commands[cmd](args)
            except Exception as e:
                print('Error: {}'.format(e))
        
    def _make(self, args):
        subcmds = {
            "null": self._make_null,
            "battery": self._make_battery,
            "aggregator": self._make_aggregator,
            "splitter": self._make_splitter,
            "pseudo": self._make_pseudo,
        }
        if len(args) < 1:
            print("Usage: make null|battery|aggregator|splitter|pseudo [<arg>...]")
            return
        if args[0] not in subcmds:
            print("Unrecognized battery kind '{}'".format(args[0]))
            return
        subcmds[args[0]](args[1:])

    def _make_null(self, args):
        if len(args) != 2:
            print("Usage: make null <name> <voltage>")
            return
        name = args[0]
        voltage = float(args[1])
        self._bos.make_null(name, voltage)
    
        
    def _make_battery(self, name, args):
        if len(args) < 4:
            print("Usage: make battery <name> <kind> <iface> <addr>")
            return
        name = args[0]
        kind = args[1]
        try:
            iface = Interface.Interface(args[2])
        except:
            print("Invalid interface '{}'".format(args[2]))
            return
        addr = args[3]
        self._bos.make_battery(name, kind, iface, addr)

    def _parse_status(self, args):
        status = BatteryStatus(0, 0, 0, 0, 0, 0)
        for arg in args.split(','):
            (key, value) = arg.split('=')
            value = float(value)
            def pos():
                if value < 0:
                    raise("key '{}' requires nonnegative value".format(key))
                return value
            if key in {'voltage', 'v'}: status.voltage = pos()
            elif key in {'current', 'c'}: status.current = value
            elif key in {'state_of_charge', 'soc', 's'}: status.state_of_charge = pos()
            elif key in {'max_capacity', 'mc', 'cap'}: status.max_capacity = pos()
            elif key in {'max_discharging_current', 'mdc', 'discharge'}:
                status.max_discharging_current = pos()
            elif key in {'max_charging_current', 'mcc', 'charge'}:
                status.max_charging_current = pos()
            else:
                raise InvalidArgument("unrecognized key '{}'".format(key))
        return status
        
    def _make_pseudo(self, args):
        if len(args) != 2:
            print("Usage: make pseudo <name> <status_arg>[,<status_arg>]...\n"
                  "<status_arg> := max_capacity=<float>\n"
                  "              | current=<float>\n"
                  "              | state_of_charge=<float>\n"
                  "              | max_discharging_current=<float>\n"
                  "              | max_charging_current=<float>"
                  )
            return
        name = args[0]
        status = self._parse_status(args[1])
        self._bos.make_battery(name, "Pseudo", Interface.Interface.PSEUDO, "pseudo", status)
        
    def _make_aggregator(self, args):
        if len(args) != 4:
            print('Usage: make aggregator <name> <source>[,<source>]... <voltage>'\
                  ' <voltage_tolerance>')
            return
        name = args[0]
        sources = args[1].split(',')
        voltage = float(args[2])
        voltage_tolerance = float(args[3])
        self._bos.make_aggregator(name, sources, voltage, voltage_tolerance, 0)

    def _make_splitter(self, args):
        if len(args) < 1:
            print("Usage: make splitter policy|battery <arg>...")
            return
        cmds = {"policy": self._make_splitter_policy,
                "battery": self._make_splitter_battery,
                }
        if args[0] not in cmds:
            print("Unrecognized splitter subcommand '{}'".format(args[0]))
            return
        cmds[args[0]](args[1:])

    def _make_splitter_policy(self, args):
        if len(args) != 4:
            print("Usage: make splitter policy <name> proportional|tranche <source> <initname>")
            return
        policyname = args[0]
        policytypestr = args[1]
        parent = args[2]
        initname = args[3]
        policytypedict = {"proportional": Policy.ProportionalPolicy,
                          "tranche": Policy.TranchePolicy,
                          }
        if policytypestr not in policytypedict:
            print("Unrecognized policy type '{}'".format(policytypestr))
            return
        policytype = policytypedict[policytypestr]
        self._bos.make_splitter_policy(policyname, policytype, parent, initname)

        
    def _make_splitter_battery(self, args):
        if len(args) < 3:
            print("Usage: make splitter battery <batteryname> <policyname> <status> [<arg>...]")
            return

        batteryname = args[0]
        policyname = args[1]
        status = self._parse_status(args[2])
        args = args[3:]
        self._bos.make_splitter_battery(batteryname, policyname, 0, status, *args)

        
    def _list(self, args):
        if len(args) != 0:
            print("'list' accepts no arguments")
            return
        for node in self._bos.list():
            print(node)
        

    def _tick(self, args):
        if len(args) != 1:
            print("Usage: tick <hours>")
            return
        hours = float(args[0])
        if hours < 0:
            print("You can't go back in time!")
            return
        util.bos_time.tick(hours)

    def _time(self, args):
        if len(args) != 0:
            print("'time' accepts no arguments")
            return
        print(util.bos_time())

    def _stat(self, args):
        if len(args) != 1:
            print("Usage: stat <battery>")
            return
        battery = args[0]
        try:
            status = self._bos.get_status(battery)
        except BOSErr.BOSErr as e:
            print('Error: {}'.format(e))
            return
        print(status)

if __name__ == '__main__':
    interp = Interpreter()
    interp.run()
