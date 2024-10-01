//----------------------------------------------------------------------------------------------------------------------
#include "helpers.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <boost/core/quick_exit.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <csignal>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <thread>
#include <random>
#include <sstream>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

using namespace std::chrono_literals;

//----------------------------------------------------------------------------------------------------------------------
namespace brypt::examples {
//----------------------------------------------------------------------------------------------------------------------

volatile std::sig_atomic_t StopRequested = 0;

extern "C" void on_shutdown_requested(std::int32_t signal);

//----------------------------------------------------------------------------------------------------------------------
} // brypt::examples namespace
//----------------------------------------------------------------------------------------------------------------------

extern "C" void brypt::examples::on_shutdown_requested(std::int32_t signal)
{
    StopRequested = signal;
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t main()
{
    auto const logger = brypt::examples::generate_logger("omega");

    // Register listeners such that we can properly handle shutdown requests via process signals. 
    if (SIG_ERR == std::signal(SIGINT, brypt::examples::on_shutdown_requested)) { return 1; }
    if (SIG_ERR == std::signal(SIGTERM, brypt::examples::on_shutdown_requested)) { return 1; }

    // First let's setup the brypt::service object. We can do this by providing a result to be set. If no result object
    // is passed in, the constructor may throw if any errors occur. Since we didn't provide a root filepath the service
    // will operate with filesystem operations disabled. This means any configuration or bootstrap addresses will 
    // not be written out to be used in subsequent runs. 
    brypt::result result;
    brypt::service service{ result };
    if (result.is_error()) { return 1; }

    // Before starting the service we can configure, attach loggers, or subscribe to events. After the service is 
    // started these actions will be blocked until the service is stopped.
    
    // The most important part of securing your brypt network is configuring the supported cipher suites of the node. 
    // You can designate three tiers of confidentiality (high, medium, and low). When two nodes attempt a connection
    // they will agree one a cipher package to be used for their communication. The preference of the algorithms is in 
    // the order that they are passed in. Most non-deprecated algorithms offered through OpenSSL may be configured with 
    // your node. 
    service.set_supported_algorithms({ {
        brypt::confidentiality_level::high,
        {
            { "kem-kyber768" },
            { "aes-256-ctr" },
            { "blake2b512" }
        }
    }});

    // You can configure any of the available options to fit your application's needs. 
    // Or, if operating with the filesystem enabled expose these methods through an 
    // interface that your user's can adjust. If the configuration is written out they will be used on subsequent runs
    // without the need to set them explicitly. 
    service.set_option(brypt::option::use_bootstraps, true);
    service.set_option(brypt::option::connection_timeout, 5'000ms);
    service.set_option(brypt::option::connection_retry_interval, 1'000ms);

    // The service will log out messages that might be useful for monitoring or debugging the network. You can 
    // optionally attach a log capturing which you can then include in your application's output. 
    service.set_option(brypt::option::log_level, brypt::log_level::info);
    auto const attached = service.register_logger([&] (brypt::log_level level, std::string_view message) {
        logger->log(brypt::examples::translate(level), message);
    });
    if (attached.is_error()) { return 1; }

    // The service offers several different events that can be subscribed to. These events may be used to drive 
    // your application. For example, you can start messaging when a peer connects or update a user interface to
    // reflect the state of the network. 
    service.subscribe<brypt::event::runtime_started>([&] {
        logger->info("Welcome to the Brypt Network! Your identifier is: {}", service.get_identifier().as_string());
    });

    service.subscribe<brypt::event::runtime_stopped>([&] (auto const&) {
        logger->info("Thank you for visiting the Brypt Network!");
    });

    service.subscribe<brypt::event::peer_connected>([&] (auto identifier, auto protocol) {
        logger->info("Peer [{}] connected over {}.", identifier, brypt::to_string(protocol));
    });

    service.subscribe<brypt::event::peer_disconnected>([&] (auto identifier, auto protocol, auto const&) {
        logger->info("Peer [{}] disconnected over {}.", identifier, brypt::to_string(protocol));
    });

    // Message handling in the brypt network works through what are known as routes. Here we setup a basic route under
    // "/ping" that will respond to the requester with a "pong!" message. You can setup a route for almost any string of 
    // characters. There are a few built in routes that all nodes will have; be careful, you can override them! 
    auto const setup = service.route(
        "/ping", [&] (std::string_view source, std::span<uint8_t const> payload, brypt::next const& next) {
        logger->info("[ping] {}: {}", source, brypt::helpers::to_string_view(payload));
        auto const responded = next.respond(brypt::helpers::marshal("pong!"), brypt::status_code::ok);
        return responded.is_success();
    });
    if (setup.is_error()) { return 1; }

    // Lastly we have to attach an endpoint to be used for the network. Here we will use the local network and random 
    // port. A bootstrap can be provided with the endpoint options in order to connect to a network. This bootstrap 
    // represents the address hosted by "alpha" example. Before running this node you must start the "alpha" runtime. 
    // Currently, there is no other mode of discovery available. 
    std::random_device device;
    std::mt19937 engine(device());
    std::uniform_int_distribution distribution(35217, 35255);

    std::ostringstream oss;
    oss << "127.0.0.1:" << distribution(engine);
    service.attach_endpoint({ brypt::protocol::tcp, "lo", oss.str(), "127.0.0.1:35216" });

    // Finally, the service can be started. This will cause any endpoints to be spun up and connection to the brypt 
    // network to begin. 
    auto const running = service.startup();
    if (running.is_error()) { return 1; } // Startup has indicated an error has occured, the application may not proceed. 

    // By default, the service runs on a background thread and will return immediately to the caller. Let's keep 
    // the application alive and occasionally others in the network. 
    while (!brypt::examples::StopRequested) {
        // The service offers a few different messaging constructs for sending messages; standard messages and 
        // requests. A standard message will be sent to the designated route and does not expect a response from the 
        // receiver. Requests operate in a similar manner, but do expect a response from the receiver. Further, 
        // standard messages and requests can be sent to specific peers, all peers, or a random sample. 

        // Let's send out a "ping" request to a random sample of our neighbors. The first functor provided to the 
        // call is the response handle whereas the second functor is the error handler. If we receive a response, 
        // let's log out the payload. If we receive an error, let's log out the reason. 
        service.sample_request(
            "/ping", brypt::helpers::marshal("ping!"), 0.5,
            [&] (brypt::response const& response) {
                logger->info("[pong] {}: {}", response.get_source(), brypt::helpers::to_string_view(response.get_payload()));
            },
            [&] (brypt::response const& response) {
                logger->warn("[pong] {}: {}", response.get_source(), response.get_status().message());
            });

        std::this_thread::sleep_for(5s);
    }

    // When the application receives a termination signal, you should tell the service to shutdown to ensure that 
    // resources are properly cleaned up and any final serialization (if-applicable) can be performed. 
    service.shutdown();

    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
