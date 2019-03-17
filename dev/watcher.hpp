#ifndef WATCHER_HPP
#define WATCHER_HPP

#include <utility>

#include "utility.hpp"
#include "node.hpp"
#include "state.hpp"
#include "connection.hpp"
#include "message.hpp"

class PeerWatcher {
    private:
        class Node * node_instance;
        State * state;
        std::vector<std::pair<SystemClock, class Connection *>> watched;
        size_t watched_count = 0;

        SystemClock last_check;

        bool worker_active = false;
        std::thread worker_thread;
        std::mutex worker_mutex;
        std::condition_variable worker_conditional;

    public:
        PeerWatcher(class Node * instance, struct State * state) {
            this->node_instance = instance;
            this->state = state;
            this->setup();
            this->spawn();
        }

        void setup();

        void spawn() {
            std::cout << "== [PeerWatcher] Spawning PeerWatcher thread\n";
            this->worker_thread = std::thread(&PeerWatcher::worker, this);
        }

        void worker() {
            this->worker_active = true;

        }

        void heartbeat() {

        }

};

#endif
