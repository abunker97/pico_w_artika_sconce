rm -r build
mkdir build
cd build
cmake .. -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Debug -GNinja
cd ..