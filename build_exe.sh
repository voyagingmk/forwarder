mkdir build_ios
cd build_ios
cmake  -G "Xcode" -Dbuildtarget=exe ..
xcodebuild
cd ..
