mkdir build_mac
cd build_mac
cmake  -G "Xcode" -Dbuildtarget=exe ..
xcodebuild
cd ..
