//------------------------------------------------------------------------------------------------
// File: ResponseTracker.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ResponseTracker.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../../Message/MessageBuilder.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cassert>
//------------------------------------------------------------------------------------------------

IOD_SYMBOL(identifier)
IOD_SYMBOL(pack)

//------------------------------------------------------------------------------------------------
// Description: Intended for a single peer
//------------------------------------------------------------------------------------------------
Await::CResponseTracker::CResponseTracker(
    std::weak_ptr<CBryptPeer> const& wpRequestor,
    CMessage const& request,
    BryptIdentifier::SharedContainer const& spBryptPeerIdentifier)
    : m_status(ResponseStatus::Unfulfilled)
    , m_expected(1)
    , m_received(0)
    , m_wpRequestor(wpRequestor)
    , m_request(request)
    , m_responses()
    , m_expire(TimeUtils::GetSystemTimepoint() + ExpirationPeriod)
{
    if (spBryptPeerIdentifier) {
        TResponseEntry entry = { spBryptPeerIdentifier, {} };
        m_responses.emplace(entry);
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Intended for multiple peers with responses from each
//------------------------------------------------------------------------------------------------
Await::CResponseTracker::CResponseTracker(
    std::weak_ptr<CBryptPeer> const& wpRequestor,
    CMessage const& request,
    std::set<BryptIdentifier::SharedContainer> const& identifiers)
    : m_status(ResponseStatus::Unfulfilled)
    , m_expected(identifiers.size())
    , m_received(0)
    , m_wpRequestor(wpRequestor)
    , m_request(request)
    , m_responses()
    , m_expire(TimeUtils::GetSystemTimepoint() + ExpirationPeriod)
{
    BryptIdentifier::CContainer const& source = m_request.GetSource();
    for (auto const& spBryptPeerIdentifier: identifiers) {
        if (!spBryptPeerIdentifier || *spBryptPeerIdentifier == source) {
            continue;
        }
        TResponseEntry entry = { spBryptPeerIdentifier, {} };
        m_responses.emplace(entry);
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: This places a response message into the aggregate object for this await object.
// Returns: true if the await object is fulfilled, false otherwise.
//------------------------------------------------------------------------------------------------
Await::ResponseStatus Await::CResponseTracker::UpdateResponse(CMessage const& response)
{
    auto const itr = m_responses.find(response.GetSource().GetInternalRepresentation());
    if(itr == m_responses.end() || !itr->pack.empty()) {
        NodeUtils::printo("Unexpected node response", NodeUtils::PrintType::Await);
        return m_status;
    }

    m_responses.modify(itr, [&response](TResponseEntry& entry)
    {
        entry.pack = response.GetPack();
    });

    if (++m_received >= m_expected) {
        m_status = ResponseStatus::Fulfilled;
    }

    return m_status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Determines whether or not the await object is ready. It is ready if it has 
// m_received all responses requested, or it has timed-out.
// Returns: true if the object is ready and false otherwise.
//------------------------------------------------------------------------------------------------
Await::ResponseStatus Await::CResponseTracker::CheckResponseStatus()
{
    if (m_received == m_expected || m_expire < TimeUtils::GetSystemTimepoint()) {
        m_status = ResponseStatus::Fulfilled;
    }

    return m_status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Returns the number of received responses
//------------------------------------------------------------------------------------------------
std::uint32_t Await::CResponseTracker::GetResponseCount() const
{
    return m_received;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Gathers information from the aggregate object and packages it into a new message.
// Returns: The aggregate message.
//------------------------------------------------------------------------------------------------
bool Await::CResponseTracker::SendFulfilledResponse()
{
    if (CheckResponseStatus() != ResponseStatus::Fulfilled) {
        return false;
    }

    std::vector<TResponseEntry> responsesVector;
    std::transform(m_responses.begin(), m_responses.end(), std::back_inserter(responsesVector), 
        [](auto const& entry) -> TResponseEntry
        { 
            return entry;
        });

    std::string data = iod::json_vector(s::identifier, s::pack).encode(responsesVector);

    auto const optResponse = CMessage::Builder()
        .SetMessageContext(m_request.GetMessageContext())
        .SetSource(m_request.GetDestination())
        .SetDestination(m_request.GetSource())
        .SetCommand(m_request.GetCommandType(), m_request.GetPhase() + 1)
        .SetData(data, m_request.GetNonce() + 1)
        .ValidatedBuild();
    assert(optResponse);

    // After the aggregate response has been generated the tracked responses can be 
    // cleared. Therby rejecting any new responses.
    m_responses.clear();
    m_status = ResponseStatus::Completed;

    if (auto const spRequestor = m_wpRequestor.lock(); spRequestor) {
        return spRequestor->ScheduleSend(*optResponse);
    }

    return false;
}

//------------------------------------------------------------------------------------------------
