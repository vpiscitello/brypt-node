//----------------------------------------------------------------------------------------------------------------------
// File: Statistics.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Statistics.hpp"
//----------------------------------------------------------------------------------------------------------------------

Peer::Statistics::Statistics()
    : m_sent(0)
    , m_received(0)
{
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Peer::Statistics::GetSentCount() const
{
    return m_sent;
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Peer::Statistics::GetReceivedCount() const
{
    return m_received;
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Statistics::IncrementSentCount()
{
    ++m_sent;
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Statistics::IncrementReceivedCount()
{
    ++m_received;
}

//----------------------------------------------------------------------------------------------------------------------
