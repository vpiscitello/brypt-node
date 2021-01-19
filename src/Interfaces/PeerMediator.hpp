//------------------------------------------------------------------------------------------------
// File: PeerMediator.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/ConnectionState.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class IPeerObserver;

//------------------------------------------------------------------------------------------------

class IPeerMediator
{
public:
    using OptionalRequest = std::optional<std::string>;

    virtual ~IPeerMediator() = default;

    virtual void RegisterObserver(IPeerObserver* const observer) = 0;
    virtual void UnpublishObserver(IPeerObserver* const observer) = 0;

    virtual OptionalRequest DeclareResolvingPeer(std::string_view uri) = 0;
    virtual OptionalRequest DeclareResolvingPeer(
        BryptIdentifier::SharedContainer const& spIdentifier) = 0;

    virtual std::shared_ptr<CBryptPeer> LinkPeer(
        BryptIdentifier::CContainer const& identifier,
        std::string_view uri = "") = 0;

    virtual void DispatchPeerStateChange(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology,
        ConnectionState change) = 0;
};

//------------------------------------------------------------------------------------------------