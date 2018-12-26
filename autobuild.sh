cd ./build
cmake ../
make -j4 
make install 
cd ../bin/
cp ../log4cxx.properties ./
cp ../config.json ./
./erizo_controller_cpp
