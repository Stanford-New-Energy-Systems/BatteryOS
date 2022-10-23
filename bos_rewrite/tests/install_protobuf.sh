#!/bin/bash

# This file installs the protobuf compiler from the GitHub repo. This installation
# assumes C++ is present on the system as it is necessary for the build. If C++ is 
# not on your system, first install g++ and then run this script.

echo "Installing protobuf 3.21.6 tar file from GitHub"
wget https://github.com/protocolbuffers/protobuf/releases/download/v21.6/protobuf-cpp-3.21.6.tar.gz

echo "Untar directory and removing tar file"
tar -xvzf protobuf-cpp-3.21.6.tar.gz
rm protobuf-cpp-3.21.6.tar.gz

echo "cd into protobuf directory"
cd protobuf-3.21.6/

echo "Installing protoc ... sudo access necessary here"
sleep(5)
./configure
make -j 8 # uses 8 cores ( can make this number higher if more cores ) 
make check
sudo make install
sudo ldconfig # refresh shared library cache.

echo "Checking if protoc exists"
which protoc

echo "Remove protobuf package"
rm -rf protobuf-3.21.6
