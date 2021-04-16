//----------------------------------------------------------------------------------------------------------------------
// File: ResponseTracker.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "ResponseTracker.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Peer/Proxy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

#ifndef LI_SYMBOL_identifier
#define LI_SYMBOL_identifier
LI_SYMBOL(identifier)
#endif
#ifndef LI_SYMBOL_pack
#define LI_SYMBOL_pack
LI_SYMBOL(pack)
#endif

//----------------------------------------------------------------------------------------------------------------------
// Description: Intended for a single peer
//----------------------------------------------------------------------------------------------------------------------
Await::ResponseTracker::ResponseTracker(
    std::weak_ptr<Peer::Proxy> const& wpRequestor,
    ApplicationMessage const& request,
    Node::SharedIdentifier const& spPeerIdentifier)
    : m_status(ResponseStatus::Unfulfilled)
    , m_expected(1)
    , m_received(0)
    , m_wpRequestor(wpRequestor)
    , m_request(request)
    , m_responses()
    , m_expire(TimeUtils::GetSystemTimepoint() + ExpirationPeriod)
{
    if (spPeerIdentifier) {
        ResponseEntry entry = { spPeerIdentifier, {} };
        m_responses.emplace(entry);
    }
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Intended for multiple peers with responses from each
//----------------------------------------------------------------------------------------------------------------------
Await::ResponseTracker::ResponseTracker(
    std::weak_ptr<Peer::Proxy> const& wpRequestor,
    ApplicationMessage const& request,
    std::set<Node::SharedIdentifier> const& identifiers)
    : m_status(ResponseStatus::Unfulfilled)
    , m_expected(std::uint32_t(identifiers.size()))
    , m_received(0)
    , m_wpRequestor(wpRequestor)
    , m_request(request)
    , m_responses()
    , m_expire(TimeUtils::GetSystemTimepoint() + ExpirationPeriod)
{
    Node::Identifier const& source = m_request.GetSourceIdentifier();
    for (auto const& spIdentifier: identifiers) {
        if (!spIdentifier || *spIdentifier == source) { continue; }
        ResponseEntry entry = { spIdentifier, {} };
        m_responses.emplace(entry);
    }
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier Await::ResponseTracker::GetSource() const
{
    return m_request.GetSourceIdentifier();
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: This places a response message into the aggregate object for this await object. 
// Will return an UpdateStatus to indicate a success or failure. On successful update the it 
// indicates a success, however, if the request becomes fulfilled that will take precendence. 
// On failure, the type of error will be determined and returned to the caller. 
//----------------------------------------------------------------------------------------------------------------------
Await::UpdateStatus Await::ResponseTracker::UpdateResponse(ApplicationMessage const& response)
{
    auto const itr = m_responses.find(response.GetSourceIdentifier().GetInternalValue());
    if(itr == m_responses.end() || !itr->pack.empty()) {
        if (m_expire < TimeUtils::GetSystemTimepoint()) { return UpdateStatus::Expired; }
        return UpdateStatus::Unexpected;
    }

    m_responses.modify(itr, [&response] (ResponseEntry& entry)
    {
        entry.pack = response.GetPack();
    });

    if (++m_received >= m_expected) {
        m_status = ResponseStatus::Fulfilled;
        return UpdateStatus::Fulfilled;
    }

    return UpdateStatus::Success;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Determines whether or not the await object is ready. It is ready if it has 
// m_received all responses requested, or it has timed-out.
// Returns: true if the object is ready and false otherwise.
//----------------------------------------------------------------------------------------------------------------------
Await::ResponseStatus Await::ResponseTracker::CheckResponseStatus()
{
    if (m_received == m_expected || m_expire < TimeUtils::GetSystemTimepoint()) {
        m_status = ResponseStatus::Fulfilled;
    }

    return m_status;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Returns the number of received responses
//----------------------------------------------------------------------------------------------------------------------
std::uint32_t Await::ResponseTracker::GetResponseCount() const
{
    return m_received;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Gathers information from the aggregate object and packages it into a new message.
// Returns: The aggregate message.
//----------------------------------------------------------------------------------------------------------------------
bool Await::ResponseTracker::SendFulfilledResponse()
{
    if (CheckResponseStatus() != ResponseStatus::Fulfilled) {
        return false;
    }

    std::vector<ResponseEntry> responsesVector;
    std::transform(m_responses.begin(), m_responses.end(), std::back_inserter(responsesVector), 
        [](auto const& entry) -> ResponseEntry {  return entry; });

    std::string data = li::json_vector(s::identifier, s::pack).encode(responsesVector);

    // Note: The destination of the stored request should always represent to the current
    // node's Brypt Identifier. 
    auto const& optNodeIdentifier = m_request.GetDestinationIdentifier();
    if (!optNodeIdentifier) { return false; }

    // Since we are responding to the request, the destination will point to its source.
    auto const& destination = m_request.GetSourceIdentifier();

    auto const optResponse = ApplicationMessage::Builder()
        .SetMessageContext(m_request.GetContext())
        .SetSource(*optNodeIdentifier)
        .SetDestination(destination)
        .SetCommand(m_request.GetCommand(), m_request.GetPhase() + std::uint8_t(1))
        .SetPayload(data)
        .ValidatedBuild();
    assert(optResponse);

    // After the aggregate response has been generated the tracked responses can be 
    // cleared. Therby rejecting any new responses.
    m_responses.clear();
    m_status = ResponseStatus::Completed;

    if (auto const spRequestor = m_wpRequestor.lock(); spRequestor) {
        return spRequestor->ScheduleSend(m_request.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
