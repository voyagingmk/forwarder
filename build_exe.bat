mkdir build64
cd build64
cmake  -G "Visual Studio 14 2015 Win64" -Dbuildtarget=exe ..
msbuild forwarder.sln
cd ..
