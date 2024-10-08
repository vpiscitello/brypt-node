//----------------------------------------------------------------------------------------------------------------------
// File: Statistics.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Peer {
//----------------------------------------------------------------------------------------------------------------------

class Statistics;

//----------------------------------------------------------------------------------------------------------------------
} // Peer namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Statistics
{
public:
    Statistics();

    [[nodiscard]] std::uint32_t GetSentCount() const;
    [[nodiscard]] std::uint32_t GetReceivedCount() const;

    void IncrementSentCount();
    void IncrementReceivedCount();

private:
    std::atomic<std::uint32_t> m_sent;
    std::atomic<std::uint32_t> m_received;
};

//----------------------------------------------------------------------------------------------------------------------
