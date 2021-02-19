//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <boost/asio/awaitable.hpp>
#include <boost/system/error_code.hpp>
//------------------------------------------------------------------------------------------------
#include <cstdint>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Network::TCP {
//------------------------------------------------------------------------------------------------

enum class CompletionOrigin : std::uint8_t { Self, Peer, Error };
using SocketProcessor = boost::asio::awaitable<CompletionOrigin>;

[[nodiscard]] bool IsInducedError(boost::system::error_code const& error);

//------------------------------------------------------------------------------------------------
} // Network::TCP namespace
//------------------------------------------------------------------------------------------------

inline bool Network::TCP::IsInducedError(boost::system::error_code const& error)
{
    switch (error.value()) {
        case boost::asio::error::operation_aborted:
        case boost::asio::error::shut_down: {
            return true;
        }
        default: return false;
    }
}

//------------------------------------------------------------------------------------------------
