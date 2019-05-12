#ifndef AWAIT_HPP
#define AWAIT_HPP

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <openssl/md5.h>

#include "utility.hpp"
#include "json11.hpp"

typedef std::unordered_map<std::string, class AwaitObject> AwaitMap;
const std::chrono::milliseconds AWAIT_TIMEOUT = std::chrono::milliseconds(1500);

class AwaitObject {
    private:
        bool fulfilled;
        unsigned int expected_responses;
        unsigned int received_responses;
        std::string source_id;
        class Message request;
        json11::Json::object aggregate_object;
        SystemClock expire;

    public:
        AwaitObject(class Message request, std::vector<std::string> * peer_names, unsigned int expected_responses) {
            this->fulfilled = false;
            this->received_responses = 0;
            this->expected_responses = expected_responses;
            this->request = request;
            this->source_id = this->request.get_source_id();
            this->expire = get_system_clock() + AWAIT_TIMEOUT;
            if (peer_names != NULL) {
                this->instantiate_response_object(peer_names);
            }
        }

        AwaitObject(class Message request, std::string * peer_name, unsigned int expected_responses) {
            this->fulfilled = false;
            this->received_responses = 0;
            this->expected_responses = expected_responses;
            this->request = request;
            this->expire = get_system_clock() + AWAIT_TIMEOUT;
            this->aggregate_object[(*peer_name)] = "Unfulfilled";
        }

        void instantiate_response_object(std::vector<std::string> * peer_names) {
            std::vector<std::string>::iterator name_it = peer_names->begin();
            for (name_it; name_it != peer_names->end(); name_it++) {
                if ((*name_it) == this->source_id) {
                    continue;
                }
                this->aggregate_object[(*name_it)] = "Unfulfilled";
            }
        }

        bool ready() {
            if (this->expected_responses == this->received_responses) {
                this->fulfilled = true;
            } else {
                if (this->expire < get_system_clock()) {
                    this->fulfilled = true;
                }
            }

            return this->fulfilled;
        }

        class Message get_response() {
            std::string data = "";

            if (this->fulfilled) {
                json11::Json aggregate_json = json11::Json::object(this->aggregate_object);
                data = aggregate_json.dump();
            }

            class Message response(
                this->request.get_destination_id(),
                this->request.get_source_id(),
                this->request.get_command(),
                this->request.get_phase() + 1,
                data,
                this->request.get_nonce() + 1
             );

             return response;
        }

        bool update_response(class Message response) {
            if (this->aggregate_object[response.get_source_id()].dump() != "\"Unfulfilled\"") {
                printo("Unexpected node response", AWAIT_P);
                return this->fulfilled;
            }

            this->aggregate_object[response.get_source_id()] = response.get_pack();

            this->received_responses++;
            if (this->received_responses >= this->expected_responses) {
                this->fulfilled = true;
            }

            return this->fulfilled;
        }


};

class AwaitContainer {
    private:
        AwaitMap awaiting;

        std::string key_generator(std::string pack) {
            unsigned char digest[MD5_DIGEST_LENGTH];

            MD5_CTX ctx;
            MD5_Init(&ctx);
            MD5_Update(&ctx, pack.c_str(), strlen(pack.c_str()));
            MD5_Final(digest, &ctx);

            char hash_cstr[33];
            for (int idx = 0; idx < MD5_DIGEST_LENGTH; idx++) {
                sprintf(&hash_cstr[idx * 2], "%02x", (unsigned int)digest[idx]);
            }

            return std::string(hash_cstr);
        }

    public:
        std::string push_request(class Message message, std::vector<std::string> * peer_names, unsigned int expected_responses) {
            std::string key = "";

            key = this->key_generator(message.get_pack());
            printo("Pushing AwaitObject with key: " + key, AWAIT_P);
            this->awaiting.emplace(key, AwaitObject(message, peer_names, expected_responses));

            return key;
        }

        std::string push_request(class Message message, std::string * peer_name, unsigned int expected_responses) {
            std::string key = "";

            key = this->key_generator(message.get_pack());
            printo("Pushing AwaitObject with key: " + key, AWAIT_P);
            this->awaiting.emplace(key, AwaitObject(message, peer_name, expected_responses));

            return key;
        }

        bool push_response(std::string key, class Message message) {
            bool success = true;

	        printo("Pushing response to AwaitObject " + key, AWAIT_P);
            bool fulfilled = this->awaiting.at(key).update_response(message);
            if (fulfilled) {
                printo("AwaitObject has been fulfilled, Waiting to transmit", AWAIT_P);
            }

            return success;
        }

        bool push_response(class Message message) {
            bool success = true;

            std::string key = message.get_await_id();
            printo("Pushing response to AwaitObject " + key, AWAIT_P);
            bool fulfilled = this->awaiting.at(key).update_response(message);
            if (fulfilled) {
                printo("AwaitObject has been fulfilled, Waiting to transmit", AWAIT_P);
            }

            return success;
        }

        std::vector<class Message> get_fulfilled() {
            std::vector<class Message> fulfilled;
            fulfilled.reserve(this->awaiting.size());

            for ( auto it = this->awaiting.begin(); it != this->awaiting.end(); ) {
                printo("Checking AwaitObject " + it->first, AWAIT_P);
                if (it->second.ready()) {
                    class Message response = it->second.get_response();
                    std::cout << response.get_data() << '\n';
                    fulfilled.push_back(response);
                    it = this->awaiting.erase(it);
                } else {
                    it++;
                }
            }

            return fulfilled;
        }

        bool empty() {
            return this->awaiting.empty();
        }

};

#endif
