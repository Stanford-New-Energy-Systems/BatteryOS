import pygatt
import threading
from binascii import hexlify
import binascii
import time
import sys
import typing as T
from BatteryInterface import BALBattery, BatteryStatus
from Interface import Interface, Connection
import BOSErr
# import uuid
import util
# import threading
DEBUG = False

class JBDBMS(BALBattery): 
    dev_info_str = b'\xdd\xa5\x03\x00\xff\xfd\x77'
    bat_info_str = b'\xdd\xa5\x04\x00\xff\xfc\x77'
    ver_info_str = b'\xdd\xa5\x05\x00\xff\xfb\x77'

    enter_factory_mode_register = b'\x00'
    exit_factory_mode_register = b'\x01'
    enter_factory_mode_command = b'\x56\x78'
    exit_and_save_factory_mode_command = b'\x28\x28'
    exit_factory_mode_command = b'\x00\x00'

    def __init__(self, name, iface, addr, staleness=100):
        super().__init__(name, iface, addr, staleness)
        self.connection = Connection.create(iface, addr)

        if DEBUG: print("connecting")
        self.connection.connect()

        if self.connection.get_iface() == Interface.UART:
            # send a byte to wake up the lazy BMS
            self.connection.write(bytes('Junk', 'ASCII'))
        
        if not self.connection.is_connected(): 
            print("JBDBMS: connection failed!", file=sys.stderr)
            raise BOSErr.NoBattery

        self.state = self.get_basic_info()
        
        self.current_range = (120, 120)
        self._current_range = range(-120, 120)
        self.staleness = staleness
        self.last_refresh = (time.time() * 1000)  # in ms

    @staticmethod
    def type() -> str: return "JBDBMS"

    def _read_force(self, count: int, timeout: float):
        res = self.connection.read(count, timeout)
        if len(res) != count:
            raise BOSErr.DriverError
        return res
    
    def query_info(self, to_send) -> bytes:
        max_tries = 3
        err = None
        for _ in range(0, max_tries):
            try:
                self.connection.write(to_send)
                timeout = 1 # wait 0.25 seconds
                hdr = self._read_force(4, timeout)
                if hdr[0] != 0xdd:
                    raise BOSErr.DriverError
                data = self._read_force(hdr[3], timeout)
                end = self._read_force(3, timeout)
                if end[-1] != 0x77:
                    raise BOSErr.DriverError
                cksum = end[1]
                
                if hdr[1] != to_send[2]:
                    raise BOSErr.DriverError
        
                resp = hdr + data + end
        
                # validate checksum
                self._check_resp_cksum(resp)

                return resp
            
            except BOSErr.DriverError as err_:
                err = err_
                pass

        raise err


    def write_without_receive(self, to_send): 
        self.connection.write_without_receive(to_send)
        time.sleep(0.1)
    
    def close(self): 
        self.connection.close()

    @staticmethod
    def get_read_register_command(register_address) -> bytes: 
        starter = b'\xdd'
        read_indicator = b'\xa5'
        write_indicator = b'\x5a'
        ending = b'\x77'
        twobytes_mask = int('0xffff', base=16)

        reg_addr = register_address
        data_length = b'\x00'
        checksum = (
            ~((int.from_bytes(reg_addr, byteorder='big', signed=False) + 
                int.from_bytes(data_length, byteorder='big', signed=False)) & twobytes_mask)
        ) + 1 
        checksum = (checksum & twobytes_mask).to_bytes(2, 'big', signed=False)
        command = starter + read_indicator + reg_addr + data_length + checksum + ending
        return command

    @staticmethod
    def get_write_register_command(register_address, bytes_to_write) -> bytes: 
        starter = b'\xdd'
        read_indicator = b'\xa5'
        write_indicator = b'\x5a'
        ending = b'\x77'
        twobytes_mask = int('0xffff', base=16)

        reg_addr = register_address
        data_length = len(bytes_to_write).to_bytes(1, byteorder='big', signed=False)
        sum_of_databytes = (sum(bytes_to_write))
        checksum = (
            ~((int.from_bytes(reg_addr, byteorder='big', signed=False) + 
                int.from_bytes(data_length, byteorder='big', signed=False) + 
                sum_of_databytes
            ) & twobytes_mask)
        ) + 1 
        checksum = (checksum & twobytes_mask).to_bytes(2, 'big', signed=False)
        command = starter + write_indicator + reg_addr + data_length + bytes_to_write + checksum + ending
        return command

    @staticmethod
    def extract_received_data(info) -> T.Tuple[bool, int, bytes]: 
        """
        info = 0xdd <register_addr> <OK:0x00 / Error:0x80> <length> <data> <checksum> 0x77
        """
        start = info[0]
        end = info[-1]
        checksum = info[-3:-1]
        reg_addr = info[1]
        success = (info[2] == 0) 
        length = info[3]
        data = info[4:-3]
        return (success, length, data)

    def _check_resp_cksum(self, resp):
        assert len(resp) >= 4
        cksum = 0
        for byte in resp[4:-3]:
            cksum += byte
        cksum += resp[2] # status code 
        cksum += resp[3] # opcode
        cksum = 0xff - cksum
        cksum += 1
        cksum %= 256
        if cksum != resp[-2]:
            print(resp)
            raise BOSErr.DriverError('bad checksum: computed {:02x} vs sent {:02x}'
                                     .format(cksum, resp[-2]))

        
    def get_basic_info(self) -> T.Dict[str, T.Union[int, bytes]]: 
        info = self.query_info(self.get_read_register_command(b'\x03'))
        byte_order = "big"
        basic_info = {
            "voltage": int.from_bytes(info[4:6], byte_order) / 100, # originally in decivolts
            "current": int.from_bytes(info[6:8], byte_order),  # units ???
            "remaining charge": int.from_bytes(info[8:10], byte_order), # units ???
            "maximum capacity": int.from_bytes(info[10:12], byte_order),
            "cycles": int.from_bytes(info[12:14], byte_order), 
            "manufacture": int.from_bytes(info[14:16], byte_order), 
            "balancing bit 1": info[16:18], 
            "balancing bit 2": info[18:20], 
            "protection state": info[20:22], 
            "version": info[22], 
            "remaining percentage": info[23], 
            "MOSFET status": info[24], 
            "num batteries": info[25], 
            "num temperature sensors": info[26], 
            "temperature information": info[27:-3], 
        }
        return basic_info

    def get_battery_voltages(self):
        voltages = []
        info = self.query_info(self.get_read_register_command(b'\x04'))
        byte_order = "big"
        assert (info[3] % 2 == 0)
        num_batteries = info[3] // 2
        print("#Batteries {}".format(num_batteries))
        for i in range(num_batteries): 
            voltages.append(int.from_bytes(info[4+2*i:4+2*i+2], byte_order))
            # print("Battery {} voltage: {}mV".format(i+1, int.from_bytes(info[4+2*i:4+2*i+2], byte_order)))
        return voltages
    
    def set_max_staleness(self, ms):
        """
        the maximum stale time of a value 
        """
        if ms >= 0: 
            self.staleness = ms

    def check_staleness(self):
        now = (time.time() * 1000)
        if now - self.last_refresh > self.staleness: 
            self.refresh()
        return

    def refresh(self):
        with self._lock:
            self.state = self.get_basic_info()
            if self.state['remaining charge'] <= 0: 
                self._set_dischargeable(False)
            else: 
                self._set_dischargeable(True)
            if self.state['remaining charge'] >= self.state['maximum capacity']:
                self._set_chargeable(False)
            else: 
                self._set_chargeable(True)
            newcurrent = self.state['current']
            self.last_refresh = (time.time() * 1000)
            self.update_meter(newcurrent, newcurrent)

    def get_voltage(self):
        with self._lock:
            self.check_staleness()
            return self.get_status().voltage

    def get_current(self):
        with self._lock:
            self.check_staleness()
            return self.get_status().current
    
    def get_maximum_capacity(self):
        with self._lock:
            self.check_staleness()
            return self.get_status().max_capacity

    def get_current_capacity(self):
        with self._lock:
            self.check_staleness()
            return self.get_status().state_of_charge

    def _set_chargeable(self, is_chargeable):
        # self.check_staleness()
        mosfet_status = self.state['MOSFET status']
        assert mosfet_status <= 3
        if is_chargeable: 
            mosfet_status = (mosfet_status & 2)
        else: 
            mosfet_status = (mosfet_status | 1)
        try:
            self.query_info(self.get_write_register_command(b'\xe1', mosfet_status.to_bytes(1, byteorder='big')))
        except BOSErr.DriverError as err:
            print(err)
            
    def _set_dischargeable(self, is_dischargeable): 
        # self.check_staleness()
        mosfet_status = self.state['MOSFET status']
        assert mosfet_status <= 3
        if is_dischargeable: 
            mosfet_status = (mosfet_status & 1)
        else: 
            mosfet_status = (mosfet_status | 2)
        try: 
            self.query_info(self.get_write_register_command(b'\xe1', mosfet_status.to_bytes(1, byteorder='big')))
        except BOSErr.DriverError as err:
            print(err)
        

    # def get_mosfet_status_data(self, chargeable, dischargeable): 
    #     mosfet_status = self.state['MOSFET status']
    #     assert mosfet_status <= 3
    #     if chargeable: 
    #         mosfet_status = (mosfet_status & 2)
    #     else: 
    #         mosfet_status = (mosfet_status | 1)
    #     if dischargeable: 
    #         mosfet_status = (mosfet_status & 1)
    #     else: 
    #         mosfet_status = (mosfet_status | 2)
    #     return mosfet_status

    def set_current(self, target_current):
        if util.verbose:
            print(f'{self.name()}: set current {target_current}')
        
        with self._lock:
            if (target_current not in self._current_range): 
                return False
            if (target_current > 0 and self.get_current_capacity() <= 0): 
                # self._set_dischargeable(False)
                return False
            if (target_current < 0 and self.get_current_capacity() >= self.get_maximum_capacity()): 
                # self._set_chargeable(False)
                return False 
            # if target_current > 0: 
            #     # self._set_chargeable(False)
            #     self._set_dischargeable(True)
            # elif target_current < 0: 
            #     self._set_chargeable(True)
            #     # self._set_dischargeable(False)
            # else: 
            #     self._set_chargeable(False)
            #     self._set_dischargeable(False)
            # how to limit the current? 
            
            return True

    def get_current_range(self):
        with self._lock:
            """
            Max discharging current, Max charging current 
            """
            return self.current_range

    def get_status(self) -> BatteryStatus:
        with self._lock:
            self.check_staleness()
            return BatteryStatus(\
                self.state['voltage'],
                self.state['current'], 
                self.state['remaining charge'] / 100, 
                self.state['maximum capacity'] / 100,
                self.current_range[0], 
                self.current_range[1])
        
# print(
#     hexlify(JBDBMS.get_read_register_command(b'\x03')), 
#     hexlify(JBDBMS.get_read_register_command(b'\x04')), 
#     hexlify(JBDBMS.get_read_register_command(b'\x05')),  
#     hexlify(JBDBMS.get_write_register_command(b'\xe1', b'\x00\x02'))
# )
# exit(0)


def test_query_info(bms: JBDBMS): 
    dev_info_str = JBDBMS.get_read_register_command(b'\x03') # b'\xdd\xa5\x03\x00\xff\xfd\x77'
    bat_info_str = JBDBMS.get_read_register_command(b'\x04') # b'\xdd\xa5\x04\x00\xff\xfc\x77'
    ver_info_str = JBDBMS.get_read_register_command(b'\x05') # b'\xdd\xa5\x05\x00\xff\xfb\x77'
    if DEBUG: print("------ querying device information ------")
    info = bms.query_info(dev_info_str)
    # print(hexlify(info))
    byte_order = "big"
    print(
        "Total Voltage (10mV): {}".format(int.from_bytes(info[4:6], byte_order)), 
        "Current (10mA): {}".format(int.from_bytes(info[6:8], byte_order)), 
        "Remaining charge (10mAh): {}".format(int.from_bytes(info[8:10], byte_order)), 
        "Claimed capacity (10mAh): {}".format(int.from_bytes(info[10:12], byte_order)),
        "Cycles: {}".format(int.from_bytes(info[12:14], byte_order)), 
        "Manufacture: {}".format(int.from_bytes(info[14:16], byte_order)), 
        "Balancing bits: {}".format(hexlify(info[16:18])), 
        "Balancing bits2: {}".format(hexlify(info[18:20])), 
        "Protection state: {}".format(hexlify(info[20:22])), 
        "Version: {}".format(info[22]), 
        "RSOC (remaining charge percentage): {}".format(info[23]), 
        "MOSFET status (bit0: charging0/discharging1?, bit1: turned off0/on1): {}".format(bin(info[24])), 
        "Batteries: {}".format(info[25]), 
        "NTC (number of temperature sensors): {}".format(info[26]), 
        "Temperature information not decoded... {}".format(info[27:-3]), 
        sep='\n'
    )

    if DEBUG: print("------ querying batteries information ------")
    info = bms.query_info(bat_info_str)
    # print(hexlify(info))
    assert (info[3] % 2 == 0)
    num_batteries = info[3] // 2
    print("#Batteries {}".format(num_batteries))
    for i in range(num_batteries): 
        print("Battery {} voltage: {}mV".format(i+1, int.from_bytes(info[4+2*i:4+2*i+2], byte_order)))
    
    if DEBUG: print("------ querying version information ------")
    info = bms.query_info(ver_info_str)
    print("version in hex: ", hexlify(info))


def test_factory_mode(bms: JBDBMS): 
    print("------ Entering factory mode ------")
    info = bms.query_info(
        JBDBMS.get_write_register_command(
            JBDBMS.enter_factory_mode_register, 
            JBDBMS.enter_factory_mode_command))
    print(hexlify(info))

    print("------ Try reading information ------")
    info = bms.query_info(
        JBDBMS.get_read_register_command(b'\x10'))
    success, length, data = JBDBMS.extract_received_data(info)
    print('design capacity', length, int.from_bytes(data, 'big'))

    info = bms.query_info(
        JBDBMS.get_read_register_command(b'\x11'))
    success, length, data = JBDBMS.extract_received_data(info)
    print('capacity per cycle', length, int.from_bytes(data, 'big'))


    info = bms.query_info(
        JBDBMS.get_read_register_command(b'\x12'))
    success, length, data = JBDBMS.extract_received_data(info)
    print('Cell capacity estimate voltage, 100%', length, int.from_bytes(data, 'big'))

    info = bms.query_info(
        JBDBMS.get_read_register_command(b'\x32'))
    success, length, data = JBDBMS.extract_received_data(info)
    print('Cell capacity estimate voltage, 80%', length, int.from_bytes(data, 'big'))

    info = bms.query_info(
        JBDBMS.get_read_register_command(b'\x33'))
    success, length, data = JBDBMS.extract_received_data(info)
    print('Cell capacity estimate voltage, 60%', length, int.from_bytes(data, 'big'))

    info = bms.query_info(
        JBDBMS.get_read_register_command(b'\x34'))
    success, length, data = JBDBMS.extract_received_data(info)
    print('Cell capacity estimate voltage, 40%', length, int.from_bytes(data, 'big'))

    info = bms.query_info(
        JBDBMS.get_read_register_command(b'\x35'))
    success, length, data = JBDBMS.extract_received_data(info)
    print('Cell capacity estimate voltage, 20%', length, int.from_bytes(data, 'big'))

    info = bms.query_info(
        JBDBMS.get_read_register_command(b'\x13'))
    success, length, data = JBDBMS.extract_received_data(info)
    print('Cell capacity estimate voltage, 0%', length, int.from_bytes(data, 'big'))

    print("------ Exiting factory mode ------")
    info = bms.query_info(
        JBDBMS.get_write_register_command(
            JBDBMS.exit_factory_mode_register, 
            JBDBMS.exit_factory_mode_command))
    print(hexlify(info))


# Calibrate BMS by entering factory mode
# - capacity: in amp-hours (Ah)
def calibrate(bms: JBDBMS, capacity, voltages: list):
    if len(voltages) != 6:
        raise BOSErr.InvalidArgument

    print("------ Entering factory mode ------")
    info = bms.query_info(
        JBDBMS.get_write_register_command(
            JBDBMS.enter_factory_mode_register, 
            JBDBMS.enter_factory_mode_command))


    info = bms.query_info(
        JBDBMS.get_write_register_command(
            b'\x10',
            (int(capacity * 100)).to_bytes(2, 'big', signed=False)
            ))

    # set voltages
    print(voltages)
    regs = [b'\x12', b'\x32', b'\x33', b'\x34', b'\x35', b'\x13']
    for p in zip(regs, voltages):
        info = bms.query_info(
            JBDBMS.get_write_register_command(
                p[0],
                (int(p[1] * 100)).to_bytes(2, 'big', signed=False)
                 ))

    print("------ Exiting factory mode ------")
    info = bms.query_info(
        JBDBMS.get_write_register_command(
            JBDBMS.exit_factory_mode_register, 
            JBDBMS.exit_factory_mode_command))
    

class FactoryMode:
    def __init__(self, bms: JBDBMS):
        self.bms = bms

    def __enter__(self):
        bms.query_info(
            JBDBMS.get_write_register_command(
                JBDBMS.enter_factory_mode_register,
                JBDBMS.enter_factory_mode_command))

    def __exit__(self, *args):
        bms.query_info(
            JBDBMS.get_write_register_command(
                JBDBMS.exit_factory_mode_register,
                JBDBMS.exit_factory_mode_command))
    
    
if __name__ == "__main__":
    prog = sys.argv[0]
    if len(sys.argv) < 3:
        print(f'usage: {prog} calibrate|factory|mdc|mcc <dev> [arg...]', file=sys.stderr)
        exit(1)

    cmd = sys.argv[1]

    bms = None
    try:
        # address = "A4:C1:38:3C:9D:22"
        address = sys.argv[2]
        print(address)
        bms = JBDBMS("JBDBMSTest", Interface.UART, address, staleness=1000)
        print(bms.get_status())

        args = sys.argv[3:]

        if cmd == "calibrate":
            if len(args) != 2:
                print(f'usage: {prog} calibrate <max_capacity> <v100>,<v80>,...<v0>', file=sys.stderr)
                exit(1)
            capacity = args[0]
            voltages = list(map(float, args[1].split(',')))
            calibrate(bms, float(args[0]), voltages)
        elif cmd == "factory":
            if len(args) != 0:
                print(f'usage: {prog} factory', file=sys.stderr)
                exit(1)
            test_factory_mode(bms)
        elif cmd == "write":
            if len(args) < 2:
                print(f'usage: {prog} write <reg> <byte>...')
                exit(1)
            regstr = args[0]
            bytesstr = args[1:]
            reg = bytes([int(regstr, 0)])
            data = bytes([int(s, 0) for s in bytesstr])
            with FactoryMode(bms) as _:
                bms.query_info(
                    JBDBMS.get_write_register_command(
                        reg,
                        data))
                        
            
        else:
            print(f'{prog}: unrecognized command {cmd}', file=sys.stderr)
            exit(1)

        # lock = threading.Lock()
        # bms.start_background_refresh()
        # time.sleep(10)
        # bms.stop_background_refresh()

    
    finally:
        if bms != None:
            bms.close()
    
    exit(0)


