//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CSensorState {
public:
    CSensorState();

    std::uint8_t GetPin() const;

    void SetPin(std::uint8_t pin);

private:
    mutable std::shared_mutex m_mutex;
    std::uint8_t m_pin;   // The GPIO pin the node will read from
};

//------------------------------------------------------------------------------------------------