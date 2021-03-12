//----------------------------------------------------------------------------------------------------------------------
#include "SensorState.hpp"
//----------------------------------------------------------------------------------------------------------------------

SensorState::SensorState()
    : m_mutex()
    , m_pin()
{
}

//----------------------------------------------------------------------------------------------------------------------

std::uint8_t SensorState::GetPin() const
{
    std::shared_lock lock(m_mutex);
    return m_pin;
}

//----------------------------------------------------------------------------------------------------------------------

void SensorState::SetPin(std::uint8_t pin)
{
    std::unique_lock lock(m_mutex);
    m_pin = pin;
}

//----------------------------------------------------------------------------------------------------------------------