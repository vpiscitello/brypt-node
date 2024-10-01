//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <iostream>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------

using namespace std::chrono_literals;

//----------------------------------------------------------------------------------------------------------------------

std::int32_t main()
{
    brypt::result result;
    brypt::service service{ result };
    if (result.is_error()) { return 1; }
    
    service.set_option(brypt::option::core_threads, BRYPT_DISABLE_CORE_THREAD);
    service.set_option(brypt::option::use_bootstraps, false);

    service.set_supported_algorithms({ {
        brypt::confidentiality_level::high,
        {
            { "kem-kyber768" },
            { "aes-256-ctr" },
            { "blake2b512" }
        }
    }});

    service.set_option(brypt::option::log_level, brypt::log_level::info);
    auto const attached = service.register_logger([&](brypt::log_level, std::string_view message) {
        std::cout << message;
    });
    if (attached.is_error()) { return 1; }

    service.subscribe<brypt::event::peer_connected>([] (auto identifier, auto) {
        std::cout << identifier << ": connected." << std::endl;
    });

    service.subscribe<brypt::event::peer_disconnected>([] (auto identifier, auto, auto const&) {
        std::cout << identifier << ": disconnected." << std::endl;
    });

    service.subscribe<brypt::event::runtime_started>([] () {
        std::cout << "Runtime started." << std::endl;
    });

    service.subscribe<brypt::event::runtime_stopped>([] (auto const&) {
        std::cout << "Runtime stopped." << std::endl;
    });

    auto const setup = service.route(
        "/ping", [&] (std::string_view source, std::span<uint8_t const> payload, brypt::next const& next) {
            std::cout << "[ ping ] " << source << ": " << brypt::helpers::to_string_view(payload) << std::endl;
            auto const responded = next.respond(brypt::helpers::marshal("pong!"), brypt::status_code::ok);
            return responded.is_success();
        });
    if (setup.is_error()) { return 1; }

    service.attach_endpoint({ brypt::protocol::tcp, "lo", "127.0.0.1:35216" });
    service.startup();

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
