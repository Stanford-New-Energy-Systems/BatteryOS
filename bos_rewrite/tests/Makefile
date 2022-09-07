GPP = g++

INC_DIR = ../src/
CPP_FILES = $(wildcard ../src/*.cpp)

BATTERY_DRIVERS = $(wildcard ../src/device_drivers/*.cpp)
BATTERY_OBJECTS = $(patsubst %.cpp,%.o,$(BATTERY_DRIVERS))

OBJS = $(patsubst %.cpp,%.o,$(CPP_FILES))
OBJS += ../protobuf/battery.pb.o
OBJS += ../protobuf/battery_manager.pb.o
OBJS += $(BATTERY_OBJECTS)

CFLAGS = -std=c++2a -g -I$(INC_DIR) -I../
LFLAGS = -g -pthread -lprotobuf

define remove_file
    $(if $(wildcard $(1)), \rm $(1))
endef

all: $(OBJS) mac

mac: $(BATTERY_OBJECTS) ../src/BatteryStatus.o
	$(GPP) -dynamiclib -o libbatterydrivers.dylib $^

linux: $(BATTERY_OBJECTS) ../src/BatteryStatus.o
	$(GPP) -shared -o libbatterydrivers.so $^
	
bos: $(OBJS) fifo.o
	$(GPP) $(LFLAGS) -o $@ $^

socket: $(OBJS) socket.o 
	$(GPP) $(LFLAGS) -o $@ $^

fifoTest: $(OBJS) testFifo.o
	$(GPP) $(LFLAGS) -o $@ $^

socketTest: $(OBJS) testSocket.o 
	$(GPP) $(LFLAGS) -o $@ $^

manager: $(OBJS) testDirectoryManager.o
	$(GPP) $(LFLAGS) -o $@ $^

dynamic: $(OBJS) testDynamic.o
	$(GPP) $(LFLAGS) -L ./ -lbatterydrivers -o $@ $^

../src/device_drivers/%.o: ../src/device_drivers/%.cpp
	$(GPP) $(CFLAGS) -fPIC -c $< -o $@	

../src/BatteryStatus.o: ../src/BatteryStatus.cpp
	$(GPP) $(CFLAGS) -fPIC -c $< -o $@	

../protobuf/%.o: ../protobuf/%.cc
	$(GPP) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(GPP) $(CFLAGS) -c $< -o $@

clean:
	$(call remove_file,*.o)
	$(call remove_file,../src/*.o)
	$(call remove_file,../protobuf/*.o)
	$(call remove_file,../src/device_drivers/*.o)

	$(call remove_file,bos)
	$(call remove_file,socket)
	$(call remove_file,manager)
	$(call remove_file,dynamic)
	$(call remove_file,fifoTest)
	$(call remove_file,socketTest)
	
	$(call remove_file,libbatterydrivers.so)
	$(call remove_file,libbatterydrivers.dylib)
