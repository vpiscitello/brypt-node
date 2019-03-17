#include "watcher.hpp"

void PeerWatcher::setup() {
   std::vector<class Connection *> * connections = this->node_instance->get_connections();
   for (auto conn_iter = connections->begin(); conn_iter != connections->end(); conn_iter++) {
       this->watched.emplace_back( std::make_pair( get_system_clock(), *conn_iter ) );
   }
   this->watched_count = this->watched.size();
}
