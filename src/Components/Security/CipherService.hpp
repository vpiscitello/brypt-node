//----------------------------------------------------------------------------------------------------------------------
// File: CipherService.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "SecurityDefinitions.hpp"
#include "Components/Configuration/Options.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

namespace Node { class ServiceProvider; }

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

class CipherService;
class CipherSuite;
class PackageSynchronizer;

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------

class Security::CipherService
{
public:
    explicit CipherService(Configuration::Options::SupportedAlgorithms const& options);

    [[nodiscard]] Configuration::Options::SupportedAlgorithms const& GetSupportedAlgorithms() const;

    [[nodiscard]] std::unique_ptr<Security::PackageSynchronizer> CreateSynchronizer(ExchangeRole role) const;

private:
    std::shared_ptr<Configuration::Options::SupportedAlgorithms> const m_spSupportedAlgorithms;
};

//----------------------------------------------------------------------------------------------------------------------
