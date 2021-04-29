from bluepy import btle
import time
import binascii

class ReadDelegate(btle.DefaultDelegate):
    def __init__(self):
        btle.DefaultDelegate.__init__(self)

    def handleNotification(self, cHandle, data):
        # ... perhaps check cHandle
        # ... process 'data'
        print(data)

dev = btle.Peripheral("a4:c1:38:e5:32:26")
for svc in dev.services:
    print(str(svc))

dev.setDelegate(ReadDelegate())

bms_service_uuid = btle.UUID('0000ff00-0000-1000-8000-00805f9b34fb') # btle.UUID("ff00") # 
bms_write_uuid = btle.UUID('0000ff02-0000-1000-8000-00805f9b34fb') # btle.UUID("ff02") # 
bms_read_uuid = btle.UUID('0000ff01-0000-1000-8000-00805f9b34fb') # btle.UUID("ff01") # 

# bms_service_uuid = btle.UUID('0000ffe0-0000-1000-8000-00805f9b34fb')  # not found...
# bms_write_uuid = btle.UUID('0000ffe1-0000-1000-8000-00805f9b34fb')
# bms_read_uuid = btle.UUID('0000ffe1-0000-1000-8000-00805f9b34fb')

bms_service = dev.getServiceByUUID(bms_service_uuid)
for ch in bms_service.getCharacteristics(): 
    print(str(ch))



bms_writer = bms_service.getCharacteristics(bms_write_uuid)[0]
bms_reader = bms_service.getCharacteristics(bms_read_uuid)[0]

print(bms_writer.propertiesToString())
print(bms_reader.propertiesToString())

# print(bms_writer.uuid)
# print(bms_reader.uuid)

# dev_info_str_h = binascii.a2b_hex("dda50300fffd77")
# print(dev_info_str_h)
dev_info_str = b'\xdd\xa5\x03\x00\xff\xfd\x77'
bat_info_str = b'\xdd\xa5\x03\x00\xff\xfc\x77'
bms_writer.write(dev_info_str, withResponse=False)
while 1:
    if dev.waitForNotifications(1.0): 
        continue
    print('waiting...')

# time.sleep(3)
# if (bms_reader.supportsRead()): 
#     print(bms_reader.read())
#     # print(bms_writer.read())
