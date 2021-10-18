PY_CFLAGS := $(shell python3-config --cflags)
PY_LDFLAGS := $(shell python3-config --ldflags)

CPP_FILES = main.cpp 
CPP_FILES += BatteryStatus.cpp 

C_FILES = include/wiringSerial.c


.PHONY: mac
mac:
	clang++ -std=c++17 $(PY_CFLAGS) $(PY_LDFLAGS) $(CPP_FILES) $(C_FILES) -lpython3.9 -g

.PHONY: rpi
rpi:
	g++ -std=c++17 $(PY_CFLAGS) $(PY_LDFLAGS) $(CPP_FILES) $(C_FILES) -g 

