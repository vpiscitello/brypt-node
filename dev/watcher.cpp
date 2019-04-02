#include "watcher.hpp"

#include <ctime>

void PeerWatcher::populate() {
    if (this->watched == NULL) {
        this->watched = this->node_instance->get_connections();
    }
    this->known_count = this->watched->size();
}

void PeerWatcher::spawn() {
    std::cout << "== [PeerWatcher] Spawning PeerWatcher thread\n";
    this->worker_thread = std::thread(&PeerWatcher::worker, this);
}

void PeerWatcher::worker() {
    this->worker_active = true;

    do {
        // Check updates to connections
        if (this->known_count != this->watched->size()) {
            this->populate();
        }

        for (auto conn_iter = this->watched->begin(); conn_iter != this->watched->end(); conn_iter++) {
            SystemClock conn_last_update = (*conn_iter)->get_update_clock();
            // If the connection update clock hasn't been updated before an update is required
            // std::cout << conn_last_update.time_since_epoch().count() - this->update_required_by.time_since_epoch().count() << '\n';
            if (conn_last_update < this->update_required_by) {
                std::cout << "== [PeerWatcher] Peer " << (*conn_iter)->get_peer_name() << " needs to be checked with a heartbeat\n";
                // Schedule a heartbeat message
            }
            // If Update is missed the last cycle and this one
            if (conn_last_update + UPDATE_TIMEOUT < this->update_required_by) {
                std::cout << "== [PeerWatcher] Peer " << (*conn_iter)->get_peer_name() << " has timed out\n";
                // Clean up the Connection and MessageQueue pipe
            }
        }

        this->update_required_by = get_system_clock() - UPDATE_TIMEOUT;

        std::this_thread::sleep_for(UPDATE_TIMEOUT);
    } while(true);

}

void PeerWatcher::check_peers() {

}

void PeerWatcher::heartbeat() {

}
