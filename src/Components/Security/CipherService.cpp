//----------------------------------------------------------------------------------------------------------------------
// File: CipherService.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "CipherService.hpp"
#include "CipherPackage.hpp"
#include "PackageSynchronizer.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Message/PackUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Security::CipherService::CipherService(Configuration::Options::SupportedAlgorithms const& options)
    : m_spSupportedAlgorithms(std::make_shared<Configuration::Options::SupportedAlgorithms>(options))
{
    PackageSynchronizer::PackAndCacheSupportedAlgorithms(*m_spSupportedAlgorithms);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::SupportedAlgorithms const& Security::CipherService::GetSupportedAlgorithms() const
{
    return *m_spSupportedAlgorithms;
}

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Security::PackageSynchronizer> Security::CipherService::CreateSynchronizer(ExchangeRole role) const
{
    return std::make_unique<PackageSynchronizer>(role, m_spSupportedAlgorithms);
}

//----------------------------------------------------------------------------------------------------------------------
