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

class AwaitObject {
    private:
        bool fulfilled;
        unsigned int expected_responses;
        unsigned int received_responses;
        class Message request;
        json11::Json::object aggregate_object;
        SystemClock expire;

    public:
        AwaitObject(class Message request, unsigned int expected_responses) {
            this->fulfilled = false;
            this->received_responses = 0;
            this->expected_responses = expected_responses;
            this->request = request;
            this->expire = get_system_clock() + std::chrono::milliseconds(500);
        }

        std::string get_response() {
            bool ready = false;
            std::string response = "";

            if (this->expected_responses == this->received_responses) {
                ready = true;
            } else {
                if (this->expire < get_system_clock()) {
                    ready = true;
                }
            }

            if (ready) {
                json11::Json aggregate_json = json11::Json::object(this->aggregate_object);
                response = aggregate_json.dump();
            }

            return response;
        }

        bool update_response(class Message response) {
            this->aggregate_object[response.get_source_id()] = response.get_pack();

            this->received_responses++;
            if (this->received_responses == this->expected_responses) {
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
        std::string push_request(class Message message, unsigned int expected_responses) {
            std::string key = "";

            key = this->key_generator(message.get_pack());
            std::cout << "== [Await] Pushing AwaitObject with key: " << key << '\n';
            this->awaiting.emplace(key, AwaitObject(message, expected_responses));

            return key;
        }

        bool push_response(std::string key, class Message message) {
            bool success = true;

            std::cout << "== [Await] Pushing response to AwaitObject " << key << '\n';
            bool fulfilled = this->awaiting.at(key).update_response(message);
            if (fulfilled) {
                std::cout << "== [Await] AwaitObject has been fulfilled, Waiting to transmit" << '\n';
            }

            return success;
        }

        bool push_response(class Message message) {
            bool success = true;

            std::string key = message.get_await_id();
            std::cout << "== [Await] Pushing response to AwaitObject " << key << '\n';
            bool fulfilled = this->awaiting.at(key).update_response(message);
            if (fulfilled) {
                std::cout << "== [Await] AwaitObject has been fulfilled, Waiting to transmit" << '\n';
            }

            return success;
        }

        std::vector<std::string> get_fulfilled() {
            std::vector<std::string> fulfilled;
            fulfilled.reserve(this->awaiting.size());

            for ( auto it = this->awaiting.begin(); it != this->awaiting.end(); ) {
                std::cout << "== [Await] Checking AwaitObject " << it->first << '\n';
                std::string response = it->second.get_response();
                if (response != "") {
                    std::cout << response << '\n';
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
