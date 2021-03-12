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

class BryptPeer;

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
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        std::string_view pack)
        : identifier(spBryptIdentifier)
        , pack(pack)
    {
        assert(spBryptIdentifier);
    }

    BryptIdentifier::Internal::Type GetPeerIdentifier() const
    {
        return identifier->GetInternalRepresentation();
    }

    BryptIdentifier::SharedContainer const identifier;
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
        std::weak_ptr<BryptPeer> const& wpRequestor,
        ApplicationMessage const& request,
        BryptIdentifier::SharedContainer const& spPeerIdentifier);

    ResponseTracker(
        std::weak_ptr<BryptPeer> const& wpRequestor,
        ApplicationMessage const& request,
        std::set<BryptIdentifier::SharedContainer> const& identifiers);

    BryptIdentifier::Container GetSource() const;
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
                    BryptIdentifier::Internal::Type,
                    &ResponseEntry::GetPeerIdentifier>>>>;

    Await::ResponseStatus m_status;
    std::uint32_t m_expected;
    std::uint32_t m_received;

    std::weak_ptr<BryptPeer> m_wpRequestor;
    ApplicationMessage const m_request;
    ResponseContainer m_responses;

    TimeUtils::Timepoint const m_expire;
};

//----------------------------------------------------------------------------------------------------------------------
