cd ./build
cmake ../
make -j4 
make install 
cd ../bin/
cp ../log4cxx.properties ./
cp ../config.json ./
cp ../iptable ./
./erizo_controller_cpp
