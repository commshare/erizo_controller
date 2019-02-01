ulimit -c unlimited
cd ./build
cmake ../
make -j4 
make install 
cd ../bin/
rm ./core -f 
cp ../log4cxx.properties ./
cp ../config.json ./
cp ../iptable ./
./erizo_controller_cpp
