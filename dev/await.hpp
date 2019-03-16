#ifndef AWAIT_HPP
#define AWAIT_HPP

#include <string>
#include <unordered_map>

#include "utility.hpp"
#include "json11.hpp"

typedef std::unordered_map<std::string, class AwaitObject> AwaitMap;

class AwaitObject {
    private:
        // std::string id = hash(request.get_pack());
        unsigned int expected_responses;
        unsigned int received_responses;
        class Message request;
        json11::Json::object aggregate_object;

    public:
        AwaitObject(class Message request, unsigned int expected_responses) {
            this->expected_responses = expected_responses;
            this->request = request;
        }

        std::string get_response() {
            std::string response = "";
            if (this->expected_responses == this->received_responses) {
                json11::Json aggregate_json = json11::Json::object(this->aggregate_object);
                response = aggregate_json.dump();
            }
            return response;
        }

        bool update_response(class Message response) {
            bool success = true;

            this->aggregate_object[response.get_source_id()] = response.get_pack();

            this->received_responses++;
            return success;
        }


};

class AwaitContainer {
    private:
        // std::vector<class AwaitObject> awaiting;
        AwaitMap awaiting;
        AwaitMap::hasher hash = awaiting.hash_function();

    public:
        std::string push_request(class Message message, unsigned int expected_responses) {
            std::string key = "";

            key = std::to_string(this->hash(message.get_pack()));
            std::cout << "== [Await] Pushing AwaitObject with key: " << key << '\n';
            this->awaiting.emplace(key, AwaitObject(message, expected_responses));

            return key;
        }

        bool push_response(class Message message) {
            bool success = true;

            std::string key = message.get_await_id();
            std::cout << "== [Await] Pushing response to AwaitObject " << key << '\n';

            return success;
        }

        bool check_awaiting() {
            bool success = true;

            return success;
        }

};

#endif
