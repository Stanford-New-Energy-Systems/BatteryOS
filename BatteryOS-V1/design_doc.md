TODO: create 2-3 page draft, similar to [tock](https://github.com/tock/tock/blob/tock-2.0-dev/doc/reference/trd-syscalls.md)

## Languages
* The interface is in Python.
* The virtualizer is mostly python.
* The driver is in C.

## compute
* Apps run on same device as Virtualizer
* drivers (potentially) run on a separate edge device.


## APIs

## entities

### Naming, retrieval of virtual batteries
* trivial approach: 0, 1, 2, ... -> deallocation issues
* like POSIX: physical volume, logical volumes

### Topology/Hierarchy of Virtualization
e.g.
5 physical batteries: bms1, bms2, bms3, bms4, bms5

I partition each physical battery into 3 virtual ones: bms1a, bms1b, bms1c, etc.

I now have 15 virtual batteries

I aggregate 5 virtual batteries into 1 virtual battery, 3 times

bms1a, bms2a, bms3a, bms4a, bms5a-> bmsx1, bmsx2, bmsx3

-> each bmsxi controlls a third of each of the physical batteries
