#ifndef WATCHER_HPP
#define WATCHER_HPP

#include <utility>

#include "utility.hpp"
#include "node.hpp"
#include "state.hpp"
#include "connection.hpp"
#include "message.hpp"

const std::chrono::milliseconds UPDATE_TIMEOUT = std::chrono::seconds(10);

class PeerWatcher {
    private:
        class Node * node_instance;
        State * state;
        std::vector<class Connection *> * watched = NULL;
        size_t known_count = 0;

        SystemClock last_check;
        SystemClock update_required_by;

        bool worker_active = false;
        std::thread worker_thread;
        std::mutex worker_mutex;
        std::condition_variable worker_conditional;

    public:
        PeerWatcher(class Node * instance, struct State * state) {
            this->node_instance = instance;
            this->state = state;
            this->populate();
            this->spawn();
            this->last_check = get_system_clock();
            this->update_required_by = this->last_check + UPDATE_TIMEOUT;
        }

        void populate();
        void spawn();
        void worker();
        void check_peers();
        void heartbeat();

};

#endif
