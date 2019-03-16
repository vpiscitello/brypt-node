g++ test.cpp json11.cpp mqueue.cpp node.cpp -o device -O2 -Wall -lzmq -lpthread -lcrypto -std=c++11
./device --root -id 1 -type DIRECT -port 3005
./device --leaf -id 2 -type DIRECT -port 3007 -peer 192.168.1.137 -pp 3005
