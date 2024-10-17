PY_CFLAGS = $(shell python3-config --cflags)
PY_LDFLAGS = $(shell python3-config --ldflags)

PY_CFLAGS_EMBED = $(shell python3-config --cflags --embed)
PY_LDFLAGS_EMBED = $(shell python3-config --ldflags --embed)

CURL_LDFLAGS = $(shell curl-config --libs)

CPP_FILES = main.cpp 
CPP_FILES += BatteryStatus.cpp 
CPP_FILES += util.cpp
CPP_FILES += JBDBMS.cpp
CPP_FILES += BatteryInterface.cpp
CPP_FILES += RPC.cpp
CPP_FILES += AggregatorBattery.cpp
CPP_FILES += BALSplitter.cpp
CPP_FILES += iec61850.cpp

C_FILES = include/wiringSerial.c
INCLUDE_PATH = libiec61850/hal/inc/

LIBIEC_HOME=libiec61850

include $(LIBIEC_HOME)/make/target_system.mk
include $(LIBIEC_HOME)/make/stack_includes.mk
include $(LIBIEC_HOME)/make/common_targets.mk

.PHONY: mac
mac:
	clang++ -std=c++17 $(PY_CFLAGS_EMBED) $(PY_LDFLAGS_EMBED) $(CPP_FILES) $(C_FILES) -g $(CURL_LDFLAGS) -I $(INCLUDE_PATH)

.PHONY: rpi
rpi:
	cd libiec61850; make
	g++ -std=c++17 $(PY_CFLAGS) $(PY_LDFLAGS) $(CPP_FILES) $(C_FILES) -g $(CURL_LDFLAGS) -lpthread -fPIE -I $(INCLUDE_PATH) $(CFLAGS) $(LDFLAGS) $(INCLUDES) $(LIB_NAME) $(LDLIBS)
	
.PHONY: wsl
wsl:
	g++ -std=c++17 $(PY_CFLAGS_EMBED) $(CPP_FILES) $(C_FILES) -g $(CURL_LDFLAGS) -lpthread $(PY_LDFLAGS_EMBED) -fPIE -I $(INCLUDE_PATH)

