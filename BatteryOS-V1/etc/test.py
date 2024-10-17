import sys
import time
import serial
import struct
from binascii import unhexlify
import requests as req

ser = serial.Serial(
    port='/dev/rfcomm0',
    baudrate = 9600,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout = 0
)

print(ser.name)
# test = 'DDA50300FFFD77'
test = b'\xDD\xA5\x03\x00\xFF\xFD\x77'
# test2 =  b'\x77\xFD\xFF\x00\x03\xA5\xDD'
ser.write(test)
# ser.flush()

time.sleep(10)

if(ser.isOpen() == False):
    ser.open()

if ser.readable(): 
    ret = ser.read(size=1)
    print(ret)
    print(len(ret))
    ser.cancel_read()

ser.flush()
ser.close()

