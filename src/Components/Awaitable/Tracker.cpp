//----------------------------------------------------------------------------------------------------------------------
// File: Tracker.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Tracker.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Peer/Proxy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cassert>
#include <ranges>
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

Awaitable::ITracker::ITracker(std::size_t expected)
    : m_status(Status::Unfulfilled)
    , m_expected(expected)
    , m_received(0)
    , m_expire(TimeUtils::GetSystemTimepoint() + ExpirationPeriod)
{
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::Status Awaitable::ITracker::CheckStatus()
{
    if (m_received == m_expected || m_expire < TimeUtils::GetSystemTimepoint()) { m_status = Status::Fulfilled; }
    return m_status;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::ITracker::GetExpected() const { return m_expected; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::ITracker::GetReceived() const { return m_received; }

//----------------------------------------------------------------------------------------------------------------------

Awaitable::AggregateTracker::AggregateTracker(
    std::weak_ptr<Peer::Proxy> const& wpRequestor,
    Message::Application::Parcel const& request,
    std::vector<Node::SharedIdentifier> const& identifiers)
    : ITracker(identifiers.size())
    , m_wpRequestor(wpRequestor)
    , m_request(request)
    , m_responses()
{
    auto const matches = [&source = m_request.GetSource()] (auto const& identifier) { return *identifier != source; };
    std::ranges::for_each(
        identifiers | std::views::filter(matches),
        [this] (auto const& identifier) { m_responses.emplace(Entry{ identifier, {} }); });
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: This places a response message into the aggregate object for this await object. 
// Will return an UpdateStatus to indicate a success or failure. On successful update the it 
// indicates a success, however, if the request becomes fulfilled that will take precendence. 
// On failure, the type of error will be determined and returned to the caller. 
//----------------------------------------------------------------------------------------------------------------------
Awaitable::ITracker::UpdateResult Awaitable::AggregateTracker::Update(Message::Application::Parcel const& response)
{
    auto const itr = m_responses.find(response.GetSource());
    if(itr == m_responses.end() || !itr->IsEmpty()) {
        if (m_expire < TimeUtils::GetSystemTimepoint()) { return UpdateResult::Expired; }
        return UpdateResult::Unexpected;
    }

    m_responses.modify(itr, [&response] (Entry& entry) { entry.SetPack(response.GetPack()); });

    if (++m_received >= m_expected) {
        m_status = Status::Fulfilled;
        return UpdateResult::Fulfilled;
    }

    return UpdateResult::Success;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Gathers information from the aggregate object and packages it into a new message.
// Returns: The aggregate message.
//----------------------------------------------------------------------------------------------------------------------
bool Awaitable::AggregateTracker::Fulfill()
{
    if (CheckStatus() != Status::Fulfilled) { return false; }

    struct ResponseItem {
        std::string const& identifier;
        std::string const& pack;
    };
    std::vector<ResponseItem> responses;
    std::transform(m_responses.begin(), m_responses.end(), std::back_inserter(responses), [] (auto const& entry) {
        assert(entry.GetIdentifier());
        return ResponseItem{ *entry.GetIdentifier(), entry.GetPack() };
    });

    std::string data = li::json_vector(s::identifier, s::pack).encode(responses);

    // Note: The destination of the stored request should always represent to the current node's identifier. 
    auto const& optNodeIdentifier = m_request.GetDestination();
    if (!optNodeIdentifier) { return false; }

    auto const optExtension = m_request.GetExtension<Message::Application::Extension::Awaitable>();
    if (!optExtension) { return false; }

    auto const optResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(m_request.GetContext())
        .SetSource(*optNodeIdentifier)
        .SetDestination(m_request.GetSource())
        .SetRoute(m_request.GetRoute())
        .SetPayload(data)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Binding::Response,
            optExtension->get().GetTracker())
        .ValidatedBuild();
    assert(optResponse);

    // After the aggregate response has been generated the tracked responses can be 
    // cleared. Therby rejecting any new responses.
    m_responses.clear();
    m_status = Status::Completed;

    if (auto const spRequestor = m_wpRequestor.lock(); spRequestor) {
        return spRequestor->ScheduleSend(m_request.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::AggregateTracker::Entry::Entry(Node::SharedIdentifier const& spIdentifier, std::string_view pack)
    : m_spIdentifier(spIdentifier)
    , m_pack(pack)
{
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& Awaitable::AggregateTracker::Entry::GetIdentifier() const
{
    return m_spIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

Node::Internal::Identifier const& Awaitable::AggregateTracker::Entry::GetInternalIdentifier() const
{
    assert(m_spIdentifier);
    return static_cast<Node::Internal::Identifier const&>(*m_spIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Awaitable::AggregateTracker::Entry::GetPack() const { return m_pack; }

//----------------------------------------------------------------------------------------------------------------------
bool Awaitable::AggregateTracker::Entry::IsEmpty() const { return m_pack.empty(); }

//----------------------------------------------------------------------------------------------------------------------

void Awaitable::AggregateTracker::Entry::SetPack(std::string const& pack) { m_pack = pack; }

//----------------------------------------------------------------------------------------------------------------------
