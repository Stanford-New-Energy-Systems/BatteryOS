GPP = g++

all: main

OBJS = battery_manager.pb.o battery.pb.o main.o

CFLAGS = -std=c++14

main: $(OBJS)
	$(GPP) -g -lprotobuf -o $@ $^

%.o: %.cc
	$(GPP) $(CFLAGS) -c $< -o $@

main.o: main.cpp
	$(GPP) $(CFLAGS) -c $< -o $@

clean:
	\rm *.o
	\rm main
