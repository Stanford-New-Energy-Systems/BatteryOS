GPP = g++
GCC = gcc

## check if macOS or linux, store to variable and replace at all:
## need python3-config 
## to install on Linux (sudo apt-get install python3-dev)
## to install on MacOS (brew install python) ... NEED HOMEBREW
## for python versions 3.8 and above you must use --embed flag 
## to run python script you need to pip install minimalmodbus 
## might need to install pkg-config as well 

INC_DIR = ../include/
CPP_FILES = $(wildcard ../src/*.cpp)

BATTERY_DRIVERS = $(wildcard ../src/device_drivers/*.cpp)
BATTERY_OBJECTS = $(patsubst %.cpp,%.o,$(BATTERY_DRIVERS))

OBJS = $(patsubst %.cpp,%.o,$(CPP_FILES))
OBJS += ../protobuf/battery.pb.o
OBJS += ../protobuf/battery_manager.pb.o
OBJS += ../src/wiringSerial.o
OBJS += $(BATTERY_OBJECTS)

LFLAGS = -pthread 
CFLAGS = -I$(INC_DIR) -I../
LFLAGS += `pkg-config --libs protobuf`
CFLAGS += `pkg-config --cflags protobuf`
CFLAGS += `python3-config --cflags --embed`
LFLAGS += `python3-config --ldflags --embed`

define remove_file
    $(if $(wildcard $(1)), \rm $(1))
endef

all: $(OBJS) mac

mac: $(BATTERY_OBJECTS) ../src/BatteryStatus.o ../src/wiringSerial.o
	$(GPP) -dynamiclib -o libbatterydrivers.dylib $^

linux: $(BATTERY_OBJECTS) ../src/BatteryStatus.o ../src/wiringSerial.o
	$(GPP) -shared -o libbatterydrivers.so $^
	
bos: $(OBJS) fifo.o
	$(GPP) -o $@ $^ $(LFLAGS)

socket: $(OBJS) socket.o 
	$(GPP) -o $@ $^ $(LFLAGS)

fifoTest: $(OBJS) testFifo.o
	$(GPP) -o $@ $^ $(LFLAGS)

socketTest: $(OBJS) testSocket.o 
	$(GPP) -o $@ $^ $(LFLAGS)

manager: $(OBJS) testDirectoryManager.o
	$(GPP) -o $@ $^ $(LFLAGS)

aggregate: $(OBJS) testAggregate.o
	$(GPP) -o $@ $^ $(LFLAGS)

partition: $(OBJS) testPartition.o
	$(GPP) -o $@ $^ $(LFLAGS)

pseudo: $(OBJS) testPseudo.o
	$(GPP) -o $@ $^ $(LFLAGS)

dynamic: $(OBJS) testDynamic.o
	$(GPP) -o $@ $^ $(LFLAGS) -L ./ -lbatterydrivers

../src/device_drivers/%.o: ../src/device_drivers/%.cpp
	$(GPP) -std=c++14 -fPIC -c $< -o $@	$(CFLAGS)

../src/BatteryStatus.o: ../src/BatteryStatus.cpp
	$(GPP) -std=c++14 -fPIC -c $< -o $@ $(CFLAGS)	

../src/wiringSerial.o: ../src/wiringSerial.c
	$(GCC) -fPIC -c $< -o $@ $(CFLAGS)

../protobuf/%.o: ../protobuf/%.cc
	$(GPP) -std=c++14 -c $< -o $@ $(CFLAGS)

%.o: %.cpp
	$(GPP) -std=c++14 -c $< -o $@ $(CFLAGS)

clean:
	$(call remove_file,*.o)
	$(call remove_file,../src/*.o)
	$(call remove_file,../protobuf/*.o)
	$(call remove_file,../src/device_drivers/*.o)

	$(call remove_file,bos)
	$(call remove_file,socket)
	$(call remove_file,pseudo)
	$(call remove_file,manager)
	$(call remove_file,dynamic)
	$(call remove_file,fifoTest)
	$(call remove_file,aggregate)
	$(call remove_file,partition)
	$(call remove_file,socketTest)
	
	$(call remove_file,libbatterydrivers.so)
	$(call remove_file,libbatterydrivers.dylib)
