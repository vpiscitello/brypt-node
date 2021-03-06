//------------------------------------------------------------------------------------------------
// File: PeerStatistics.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
//------------------------------------------------------------------------------------------------

class PeerStatistics
{
public:
    PeerStatistics();

    [[nodiscard]] std::uint32_t GetSentCount() const;
    [[nodiscard]] std::uint32_t GetReceivedCount() const;

    void IncrementSentCount();
    void IncrementReceivedCount();

private:
    std::uint32_t m_sent;
    std::uint32_t m_received;
};

//------------------------------------------------------------------------------------------------
