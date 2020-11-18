//------------------------------------------------------------------------------------------------
// File: PeerStatistics.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
//------------------------------------------------------------------------------------------------

class CPeerStatistics
{
public:
    CPeerStatistics();

    std::uint32_t GetSentCount() const;
    std::uint32_t GetReceivedCount() const;

    void IncrementSentCount();
    void IncrementReceivedCount();

private:
    std::uint32_t m_sent;
    std::uint32_t m_received;

};

//------------------------------------------------------------------------------------------------
