//------------------------------------------------------------------------------------------------
// File: DiscoveryProtocol.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Configuration/Configuration.hpp"
#include "../../Interfaces/ConnectProtocol.hpp"
#include "../../Interfaces/EndpointMediator.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CMessageContext;

//------------------------------------------------------------------------------------------------

class CDiscoveryProtocol : public IConnectProtocol
{
public:
    explicit CDiscoveryProtocol(Configuration::EndpointConfigurations const& configurations);

    CDiscoveryProtocol(CDiscoveryProtocol&&) = delete;
    CDiscoveryProtocol(CDiscoveryProtocol const&) = delete;
    void operator=(CDiscoveryProtocol const&) = delete;

    // IConnectProtocol {
    virtual bool SendRequest(
        BryptIdentifier::SharedContainer const& spSourceIdentifier,
        std::shared_ptr<CBryptPeer> const& spBryptPeer,
        CMessageContext const& context) const override;
    // } IConnectProtocol

private:
    std::string m_data;

};

//------------------------------------------------------------------------------------------------ 