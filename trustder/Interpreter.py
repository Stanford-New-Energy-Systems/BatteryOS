import sys
import argparse
import traceback
import time
import threading
import util

import BOS
import util
import BOSErr
from BatteryInterface import BatteryStatus, Battery
import Interface
import Policy
import Server

class Interpreter:
    def __init__(self):
        # util.bos_time = util.DummyTime(0)
        self._bos = BOS.BOS()
        self._commands = {"make": self._make,
                          "mk": self._make,
                          "tick": self._tick,
                          "time": self._time,
                          "stat": self._stat,
                          "list": self._list,
                          "ls":   self._list,
                          "load": self._load,
                          "alias": self._alias,
                          "unalias": self._unalias,
                          "remove": self._remove,
                          "rm": self._remove,
                          "modify": self._modify,
                          "mod": self._modify,
                          "current": self._current,
                          "visualize": self._visualize,
                          "help": self._help,
                          "clock": self._clock,
                          "refresh": self._refresh,
                          "sleep": self._sleep,
                          "verbose": self._verbose,
                          "echo": self._echo,
                          "server": self._server,
                          }
        self._aliases = dict()
        self._server = None

    def load(self, path):
        with open(path) as f:
            lines = f.readlines()
            for line in lines:
                if line[0] != '#':
                    self.execute(line)
        
        
    def run(self):
        while True:
            try:
                line = input('> ')
            except EOFError:
                print('')
                return
            try:
                self.execute(line)
            except Exception as e:
                print(traceback.format_exc())

    def execute(self, line: str):
        args = line.split()
        if len(args) == 0:
            return
        cmd = args[0]
        args = args[1:]
        if cmd not in self._commands:
            if cmd in self._aliases:
                args = self._aliases[cmd] + args
                cmd = args[0]
                args = args[1:]
            else:
                print("Unrecognized command '{}'.".format(cmd))
        self._commands[cmd](args)

    def _help(self, args):
        for cmd in sorted(self._commands.keys()):
            print(cmd)
            
                
    def _make(self, args):
        subcmds = {
            "null": self._make_null,
            "battery": self._make_battery,
            "aggregator": self._make_aggregator,
            "splitter": self._make_splitter,
            "pseudo": self._make_pseudo,
            "network": self._make_network,
        }
        if len(args) < 1:
            print(f'Usage: make {"|".join(subcmds.keys())} [<arg>...]')
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
    
        
    def _make_battery(self, args):
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

    def _make_network(self, args):
        if len(args) != 5:
            print("Usage: make network <name> <remote_name> <iface> <addr> <port>")
            return
        name = args[0]
        remote_name = args[1]
        try: 
            iface = Interface.Interface(args[2])
        except:
            print(f'invalid interface {args[2]}')
            return
        addr = args[3]
        port = int(args[4])
        self._bos.make_network(name, remote_name, iface, addr, port)

    def _parse_status(self, args, status=None):
        if status is None:
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
                raise BOSErr.InvalidArgument("unrecognized key '{}'".format(key))
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
        self._bos.make_aggregator(name, sources, voltage, voltage_tolerance)

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
        parser = argparse.ArgumentParser(description='list mode parser')
        parser.add_argument('-l', dest='long', action='store_const', const=True, default=False,
                            help='long mode')
        flags = parser.parse_args(args)

        for name in self._bos.list():
            if flags.long:
                node = self._bos.lookup(name)
                # get type
                tl = [(Battery, 'b'),
                      (Policy.Policy, 'p')]
                t = '?'

                # get usage status
                if len(self._bos.get_children(name)) == 0:
                    usage = '-' # free
                else:
                    usage = 'X' # used
                
                for (ty, ch) in tl:
                    if isinstance(node, ty):
                        t = ch
                        break
                
                print('{} {} {}'.format(t, usage, name))
            else:
                print(name)
        

    def _tick(self, args):
        if len(args) != 1:
            print("Usage: tick <hours>")
            return
        hours = float(args[0])
        if hours < 0:
            print("You can't go back in time!")
            return
        util.bos_time.tick(hours * 3600)

    def _time(self, args):
        if len(args) != 0:
            print("'time' accepts no arguments")
            return
        print(util.bos_time())

    def _stat(self, args):
        if len(args) < 1:
            print("Usage: stat <battery>")
            return
        for battery in args:
            try:
                status = self._bos.get_status(battery)
            except BOSErr.BOSErr as e:
                print('Error: {}'.format(e))
                return
            print('{{status = {}, meter = {}}}'.format(status,
                                                       self._bos.lookup(battery).get_meter()))

    def _load(self, args):
        if len(args) == 0:
            print("Usage: load <cmdfile>...")
            return
        for arg in args:
            self.load(arg)

    def _alias(self, args):
        if len(args) < 2:
            print("Usage: alias <name> <cmd> [<arg>...]")
            return
        self._aliases[args[0]] = args[1:]

    def _unalias(self, args):
        if len(args) != 1:
            print("Usage: unalias <name>")
            return
        del self._aliases[args[0]]

    def _remove(self, args):
        raise NotImplementedError

    def _modify(self, args):
        if len(args) < 1:
            print("Usage: modify pseudo [<arg>...]")
            return
        cmds = {
            "pseudo": self._modify_pseudo,
        }
        cmd = args[0]
        if cmd not in cmds:
            print("Unrecognized subcommand '{}'".format(cmd))
            return
        cmds[cmd](args[1:])

    def _modify_pseudo(self, args):
        if len(args) < 2:
            print("Usage: modify pseudo <name> <status_specs>")
            return
        name = args[0]
        status_specs = args[1]
        pseudo = self._bos.lookup(name)
        newstatus = self._parse_status(status_specs, pseudo.get_status())
        pseudo.set_status(newstatus)
        assert pseudo.get_status() == newstatus

    def _current(self, args):
        if len(args) != 2:
            print("Usage: current <name> <value>\n"
                  "Negative <value> means charging; positive <value> means discharging"
                  )
            return
        name = args[0]
        current = float(args[1])
        self._bos.set_current(name, current)

    def _visualize(self, args):
        self._bos.visualize()

    def _clock(self, args):
        if len(args) < 1:
            print('Usage: clock get|set ...')
            return
        subcmd = args[0]
        if subcmd == 'get':
            if len(args) != 1:
                print('clock get: accepts no arguments')
                return 
            print('{} {}'.format(type(util.bos_time).__name__, util.bos_time()))
        elif subcmd == 'set':
            if len(args) < 2:
                print('Usage: clock set DummyTime|SystemTime <arg>...')
                return
            typename = args[1]
            if typename == 'DummyTime':
                util.bos_time = util.DummyTime(*args[2:])
            elif typename == 'SystemTime':
                util.bos_time = util.SystemTime(*args[2:])
            else:
                print('clock set: invalid clock type {}'.format(args[1]))
                return

    def _refresh(self, args):
        if len(args) != 2:
            print('Usage: refresh enable|disable|once <name>')
            return
        subcmd = args[0]
        name = args[1]
        d = {
            'enable': self._bos.start_background_refresh,
            'disable': self._bos.stop_background_refresh,
            'once': self._bos.refresh,
        }
        if subcmd not in d:
            print(f'Unrecognized subcommand "{subcmd}"')
            return
        d[subcmd](name)

    def _sleep(self, args):
        if len(args) != 1:
            print('Usage: sleep <secs>')
            return
        time.sleep(int(args[0]))

    def _verbose(self, args):
        if len(args) != 0:
            print('Usage: verbose')
            return
        util.verbose = not util.verbose

    def _echo(self, args):
        print(*args)

    def _server(self, args):
        if len(args) != 1:
            print('Usage: server start|stop|check')
            return
        cmd = args[0]
        if cmd == 'start':
            server = Server.BOSServer(self._bos)
            self._server = threading.Thread(target=server.loop, args=())
            self._server.start()
        elif cmd == 'stop':
            self._server = None
        elif cmd == 'check':
            if self._server is not None:
                print('running')
            else:
                print('not started')
        else:
            print(f'bad subcommand {cmd}')


if __name__ == '__main__':
    interp = Interpreter()
    for arg in sys.argv[1:]:
        interp.load(arg)
    interp.run()
