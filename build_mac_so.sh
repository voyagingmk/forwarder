mkdir build_mac
cd build_mac
cmake  -G "Xcode" -Dbuildtarget=dll ..
xcodebuild
cd ..
