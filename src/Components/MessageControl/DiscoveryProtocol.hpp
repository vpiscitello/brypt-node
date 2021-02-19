//------------------------------------------------------------------------------------------------
// File: DiscoveryProtocol.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Components/Configuration/Configuration.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/EndpointMediator.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
//------------------------------------------------------------------------------------------------

class BryptPeer;
class MessageContext;

//------------------------------------------------------------------------------------------------

class DiscoveryProtocol : public IConnectProtocol
{
public:
    explicit DiscoveryProtocol(Configuration::EndpointConfigurations const& configurations);

    DiscoveryProtocol(DiscoveryProtocol&&) = delete;
    DiscoveryProtocol(DiscoveryProtocol const&) = delete;
    void operator=(DiscoveryProtocol const&) = delete;

    // IConnectProtocol {
    virtual bool SendRequest(
        BryptIdentifier::SharedContainer const& spSourceIdentifier,
        std::shared_ptr<BryptPeer> const& spBryptPeer,
        MessageContext const& context) const override;
    // } IConnectProtocol

private:
    std::string m_data;
};

//------------------------------------------------------------------------------------------------ 