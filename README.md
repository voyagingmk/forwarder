
# INSTALL

## cmake

mkdir build

cd build

cmake ..

make 

./bin/forwarder

## Xcode

mddir xcode_build

cd xcode_build

cmake .. -G Xcode

### unity ios

1. copy all the *src* files to Assets/Plugins/iOS/ directory.
2. Open Files->Build Settings->iOS->Build, waiting for the build process
3. after the xcode project directory built, open it with xcode
4. choose a Signing Team
5. copy forwarder's include directory to build/Classes/ directory
6. append two header search paths:  $(SRCROOT)/Classes/include  $(SRCROOT)/Classes/include/zlib  
7. enable rtti (TODO)
8. run the program on your device

## centos 6

yum install centos-release-scl-rh
yum install devtoolset-3-gcc devtoolset-3-gcc-c++

scl enable devtoolset-3 bash

cmake  -DCMAKE_C_COMPILER:FILEPATH=clang -DCMAKE_CXX_COMPILER:FILEPATH=clang++  ..


## DEPENDENCIES

### spdlog-0.11.0

https://github.com/gabime/spdlog

### rapidjson-1.1.0

https://github.com/miloyip/rapidjson

### enet-1.3.13

https://github.com/lsalzman/enet

### zlib-1.2.8

http://zlib.net/zlib-1.2.8.tar.gz

### websocketpp-0.7.0 and no-boost-asio-1.10.8

http://think-async.com/Asio

https://github.com/zaphoyd/websocketpp

### pool.h 26 Mar 2015

https://github.com/mdashti/HPDS/blob/master/src/utils/pool.h

https://www.quora.com/What-is-the-most-efficient-memory-pooling-mechanism-used-in-C-or-C%2B%2B-with-an-explanation-about-how-it-works/answer/Mohammad-Dashti?srid=2JGa

### aes.h

https://github.com/B-Con/crypto-algorithms

# TEST

## Docker

docker run --rm -it -v D:\workplace\github\forwarder:/data myubuntu
