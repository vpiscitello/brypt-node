//----------------------------------------------------------------------------------------------------------------------
// File: DiscoveryProtocol.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Configuration/Options.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/EndpointMediator.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <string>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }
namespace Message { class Context; }

//----------------------------------------------------------------------------------------------------------------------

class DiscoveryProtocol : public IConnectProtocol
{
public:
    explicit DiscoveryProtocol(Configuration::Options::Endpoints const& endpoints);

    DiscoveryProtocol(DiscoveryProtocol&&) = delete;
    DiscoveryProtocol(DiscoveryProtocol const&) = delete;
    void operator=(DiscoveryProtocol const&) = delete;

    // IConnectProtocol {
    virtual bool SendRequest(
        Node::SharedIdentifier const& spSourceIdentifier,
        std::shared_ptr<Peer::Proxy> const& spPeerProxy,
        Message::Context const& context) const override;
    // } IConnectProtocol

private:
    std::string m_data;
};

//---------------------------------------------------------------------------------------------------------------------- 