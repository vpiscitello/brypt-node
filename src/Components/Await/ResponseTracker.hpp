//------------------------------------------------------------------------------------------------
// File: ResponseTracker.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/IdentifierTypes.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

class CBryptPeer;

//------------------------------------------------------------------------------------------------
namespace Await {
//------------------------------------------------------------------------------------------------

struct TResponseEntry;

class CResponseTracker;

//------------------------------------------------------------------------------------------------
} // Await namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------

struct Await::TResponseEntry
{
    TResponseEntry(
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

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class Await::CResponseTracker
{
public:
    constexpr static auto const ExpirationPeriod = std::chrono::milliseconds(1500);

    CResponseTracker(
        std::weak_ptr<CBryptPeer> const& wpRequestor,
        CApplicationMessage const& request,
        BryptIdentifier::SharedContainer const& spPeerIdentifier);

    CResponseTracker(
        std::weak_ptr<CBryptPeer> const& wpRequestor,
        CApplicationMessage const& request,
        std::set<BryptIdentifier::SharedContainer> const& identifiers);

    Await::ResponseStatus UpdateResponse(CApplicationMessage const& response);
    Await::ResponseStatus CheckResponseStatus();
    std::uint32_t GetResponseCount() const;

    bool SendFulfilledResponse();

private:
    using ResponseTracker = boost::multi_index_container<
        TResponseEntry,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::const_mem_fun<
                    TResponseEntry,
                    BryptIdentifier::Internal::Type,
                    &TResponseEntry::GetPeerIdentifier>>>>;

    Await::ResponseStatus m_status;
    std::uint32_t m_expected;
    std::uint32_t m_received;

    std::weak_ptr<CBryptPeer> m_wpRequestor;
    CApplicationMessage const m_request;
    ResponseTracker m_responses;

    TimeUtils::Timepoint const m_expire;
};

//------------------------------------------------------------------------------------------------
