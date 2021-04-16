//----------------------------------------------------------------------------------------------------------------------
// File: ResponseTracker.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

//----------------------------------------------------------------------------------------------------------------------
namespace Await {
//----------------------------------------------------------------------------------------------------------------------

struct ResponseEntry;
class ResponseTracker;

//----------------------------------------------------------------------------------------------------------------------
} // Await namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------

struct Await::ResponseEntry
{
    ResponseEntry(
        Node::SharedIdentifier const& spNodeIdentifier,
        std::string_view pack)
        : identifier(spNodeIdentifier)
        , pack(pack)
    {
        assert(spNodeIdentifier);
    }

    Node::Internal::Identifier::Type GetPeerIdentifier() const
    {
        return identifier->GetInternalValue();
    }

    Node::SharedIdentifier const identifier;
    std::string pack;
};

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
class Await::ResponseTracker
{
public:
    constexpr static auto const ExpirationPeriod = std::chrono::milliseconds(1500);

    ResponseTracker(
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        ApplicationMessage const& request,
        Node::SharedIdentifier const& spPeerIdentifier);

    ResponseTracker(
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        ApplicationMessage const& request,
        std::set<Node::SharedIdentifier> const& identifiers);

    Node::Identifier GetSource() const;
    Await::UpdateStatus UpdateResponse(ApplicationMessage const& response);
    Await::ResponseStatus CheckResponseStatus();
    std::uint32_t GetResponseCount() const;

    bool SendFulfilledResponse();

private:
    using ResponseContainer = boost::multi_index_container<
        ResponseEntry,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::const_mem_fun<
                    ResponseEntry,
                    Node::Internal::Identifier::Type,
                    &ResponseEntry::GetPeerIdentifier>>>>;

    Await::ResponseStatus m_status;
    std::uint32_t m_expected;
    std::uint32_t m_received;

    std::weak_ptr<Peer::Proxy> m_wpRequestor;
    ApplicationMessage const m_request;
    ResponseContainer m_responses;

    TimeUtils::Timepoint const m_expire;
};

//----------------------------------------------------------------------------------------------------------------------
