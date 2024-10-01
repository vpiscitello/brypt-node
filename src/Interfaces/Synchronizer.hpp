//----------------------------------------------------------------------------------------------------------------------
// File: ISynchronizer.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Security/SecurityTypes.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

namespace Security { class CipherPackage; }

//----------------------------------------------------------------------------------------------------------------------

class ISynchronizer
{
public:
    ISynchronizer() = default;
    //ISynchronizer(ExchangeRole role, std::weak_ptr<Configuration::Options::SupportedAlgorithms> const& wpSupportedAlgorithms);

    ISynchronizer(ISynchronizer const& other) = delete;
    ISynchronizer& operator=(ISynchronizer const& other) = delete;

    virtual ~ISynchronizer() = default;

    [[nodiscard]] virtual Security::ExchangeRole GetExchangeRole() const = 0;

    [[nodiscard]] virtual std::uint32_t GetStages() const = 0;
    [[nodiscard]] virtual Security::SynchronizationStatus GetStatus() const = 0;
    [[nodiscard]] virtual bool Synchronized() const = 0;

    [[nodiscard]] virtual Security::SynchronizationResult Initialize() = 0;
    [[nodiscard]] virtual Security::SynchronizationResult Synchronize(Security::ReadableView buffer) = 0;
    [[nodiscard]] virtual std::unique_ptr<Security::CipherPackage> Finalize() = 0;
};

//----------------------------------------------------------------------------------------------------------------------
