#include "utility.hpp"
#include "node.hpp"

int main(int argc, char **argv) {
    std::cout << "== Testing Connection Factory" << '\n';
    std::vector<Connection *> connections;
    connections.push_back( ConnectionFactory(DIRECT_TYPE) );
    connections.push_back( ConnectionFactory(BLE_TYPE) );
    connections.push_back( ConnectionFactory(LORA_TYPE) );
    connections.push_back( ConnectionFactory(DIRECT_TYPE) );
    connections.push_back( ConnectionFactory(WEBSOCKET_TYPE) );

    for (int idx = 0; idx < connections.size(); idx++) {
        connections.at(idx)->whatami();
        connections.at(idx)->unspecial();
        std::cout << '\n';
    }

    std::cout << "== Starting Brypt Node" << std::endl;
    class Node alpha;
    char packet[] = "";
    alpha.connect();

    std::string local_ip = alpha.get_local_address();
    std::cout << "Local Connection IPV4: " << local_ip << '\n';


    return 0;
}
