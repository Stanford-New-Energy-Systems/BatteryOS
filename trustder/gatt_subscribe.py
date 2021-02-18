import pygatt
from binascii import hexlify
import time
import threading
adapter = pygatt.GATTToolBackend()

def handle_data(handle, value):
    """
    handle -- integer, characteristic read handle the data was received on
    value -- bytearray, the data returned in the notification
    """
    h = hexlify(value)
    print("Received data: {} with length: {}".format(h, len(h)), flush=True)

bms_service_uuid = ('0000ff00-0000-1000-8000-00805f9b34fb') # btle.UUID("ff00") # 
bms_write_uuid = ('0000ff02-0000-1000-8000-00805f9b34fb') # btle.UUID("ff02") # 
bms_read_uuid = ('0000ff01-0000-1000-8000-00805f9b34fb') # btle.UUID("ff01") # 

dev_info_str = b'\xdd\xa5\x03\x00\xff\xfd\x77'
bat_info_str = b'\xdd\xa5\x03\x00\xff\xfc\x77'

try:
    adapter.start()
    # time.sleep(5)
    device = adapter.connect('a4:c1:38:e5:32:26')
    # time.sleep(5)

    device.subscribe(bms_read_uuid, indication=False, callback=handle_data)

    while 1: 
        pass
    # # time.sleep(3)
    # device.char_write(bms_write_uuid, bat_info_str)
    # time.sleep(5)

    # device.char_write(bms_write_uuid, dev_info_str)
    # time.sleep(5)
    # # device.subscribe(bms_read_uuid, indication=False, callback=handle_data)
finally:
    adapter.stop()