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
#ifndef LI_SYMBOL_data
#define LI_SYMBOL_data
LI_SYMBOL(data)
#endif

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::ITracker(Awaitable::TrackerKey key, std::size_t expected)
    : m_key(key)
    , m_status(Status::Pending)
    , m_expected(expected)
    , m_received(0)
    , m_expire(std::chrono::steady_clock::now() + ExpirationPeriod)
{
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::Status Awaitable::ITracker::GetStatus() const { return m_status; }

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::Status Awaitable::ITracker::CheckStatus()
{
    if (m_status != Status::Completed && (m_received == m_expected || m_expire < std::chrono::steady_clock::now())) { 
        m_status = Status::Fulfilled;
    }
    return m_status;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::ITracker::GetExpected() const { return m_expected; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Awaitable::ITracker::GetReceived() const { return m_received; }

//----------------------------------------------------------------------------------------------------------------------

Awaitable::RequestTracker::RequestTracker(
    Awaitable::TrackerKey key, 
    std::weak_ptr<Peer::Proxy> const& wpProxy,
    Peer::Action::OnResponse const& onResponse,
    Peer::Action::OnError const& onError)
    : ITracker(key, 1)
    , m_ledger()
    , m_responses()
    , m_onResponse(onResponse)
    , m_onError(onError)
    , m_remaining(1)
{
    if (auto const spProxy = wpProxy.lock(); spProxy) { m_ledger.emplace(spProxy->GetIdentifier(), false); }
    assert(!m_ledger.empty());
    assert(m_onResponse);
    assert(m_onError);
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::RequestTracker::RequestTracker(
    Awaitable::TrackerKey key, 
    std::size_t expected, 
    Peer::Action::OnResponse const& onResponse, 
    Peer::Action::OnError const& onError)
    : ITracker(key, expected)
    , m_ledger()
    , m_responses()
    , m_onResponse(onResponse)
    , m_onError(onError)
    , m_remaining(expected)
{
    assert(m_onResponse);
    assert(m_onError);
}

//----------------------------------------------------------------------------------------------------------------------

bool Awaitable::RequestTracker::Correlate(Node::SharedIdentifier const& identifier)
{
    auto const [itr, emplaced] = m_ledger.emplace(identifier, false);
    return emplaced;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::UpdateResult Awaitable::RequestTracker::Update(Message::Application::Parcel&& message)
{
    if (m_expire < std::chrono::steady_clock::now()) { return UpdateResult::Expired; }
    auto const itr = std::ranges::find_if(m_ledger, [&source = message.GetSource()] (auto const& entry) {
        return *entry.first == source;
    });

    // Reject responses for peers that were not registered with the request intiially. 
    if (itr == m_ledger.end()) { return UpdateResult::Unexpected; }

    // Reject duplicate responses from peers.
    if (itr->second) { return UpdateResult::Unexpected; }

    m_responses.emplace_back(std::move(message));
    itr->second = true;

    if (++m_received == m_expected) { m_status = Status::Fulfilled; return UpdateResult::Fulfilled; }
    return UpdateResult::Partial;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::UpdateResult Awaitable::RequestTracker::Update(
    [[maybe_unused]] Node::Identifier const& identifier, [[maybe_unused]] Message::Payload&& data)
{
    return UpdateResult::Unexpected;
}

//----------------------------------------------------------------------------------------------------------------------

bool Awaitable::RequestTracker::Fulfill()
{
    using namespace Message;

    if (m_status == Status::Completed) { return false; }
    
    bool const expired = m_expire < std::chrono::steady_clock::now();
    if (!expired && m_responses.empty()) { return false; }

    for (auto const& message : m_responses) { 
        --m_remaining; // Decrement the number of remaining responses to be handled. 
	    
        auto const optStatus = message.GetExtension<Extension::Status>(); // Attempt to fetch the status extension.
        
        // Create the response object for this peer. Default the status code to "Ok" if one has not been set. 
        Peer::Action::Response response{ 
            m_key, message, (optStatus) ? optStatus->get().GetCode() : Message::Extension::Status::Ok, m_remaining
        };

        // If the response does not contain a status code representing an error, forward the response object to 
        // the normal response handler. Otherwise, pass it to the error handler. 
        if (!response.HasErrorCode()) {
            m_onResponse(response);
        } else {
            m_onError(response);
        }
    }

    if (expired) {
        for (auto const& [requestee, received] : m_ledger) { 
            // If the response has not been received before the tracker's expirations, create a response object 
            // representing a timeout error and forward it to the error handler. 
            if (!received) { 
                --m_remaining; // Decrement the number of remaining responses to be handled. 
                m_onError(Peer::Action::Response{ m_key, *requestee, Message::Extension::Status::RequestTimeout, m_remaining });
            }
        }
        m_received = m_expected;
    }

    m_status = (m_received == m_expected) ? Status::Completed : Status::Pending;
    m_responses.clear();

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::DeferredTracker::DeferredTracker(
    Awaitable::TrackerKey key, 
    std::weak_ptr<Peer::Proxy> const& wpRequestor,
    Message::Application::Parcel const& request,
    std::vector<Node::SharedIdentifier> const& identifiers)
    : ITracker(key, identifiers.size())
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

bool Awaitable::DeferredTracker::Correlate(Node::SharedIdentifier const&) { return false; }

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::UpdateResult Awaitable::DeferredTracker::Update(Message::Application::Parcel&& message)
{
    return Update(message.GetSource(), message.ExtractPayload());
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::UpdateResult Awaitable::DeferredTracker::Update(
    Node::Identifier const& identifier, Message::Payload&& payload)
{
    if (m_expire < std::chrono::steady_clock::now()) { return UpdateResult::Expired; }

    auto const itr = m_responses.find(identifier);
    if(itr == m_responses.end() || !itr->IsEmpty()) { return UpdateResult::Unexpected; }

    m_responses.modify(itr, [&payload] (Entry& entry) { entry.SetPayload(std::move(payload)); });

    if (++m_received == m_expected) { m_status = Status::Fulfilled; return UpdateResult::Fulfilled; }
    return UpdateResult::Success;
}

//----------------------------------------------------------------------------------------------------------------------

bool Awaitable::DeferredTracker::Fulfill()
{
    if(m_status != Status::Fulfilled) { return false; }
    m_status = Status::Completed;

    struct ResponseItem {
        std::string const& identifier;
        std::vector<std::uint8_t> data;
    };
    std::vector<ResponseItem> responses;
    std::transform(m_responses.begin(), m_responses.end(), std::back_inserter(responses), [] (auto const& entry) {
        // TODO: Creating a copy of the data is not ideal. Updating this method to avoid making copies should be included
        // in future work. Unfortunately, the JSON encoder only accepts std::vector to encode JSON arrays. 
        assert(entry.GetIdentifier());
        auto const readable = entry.GetPayload().GetReadableView();
        return ResponseItem{ *entry.GetIdentifier(), { readable.begin(), readable.end() } };
    });

    // Note: The destination of the stored request should always represent to the current node's identifier. 
    auto const& optNodeIdentifier = m_request.GetDestination();
    if (!optNodeIdentifier) { return false; }

    auto const optExtension = m_request.GetExtension<Message::Extension::Awaitable>();
    if (!optExtension) { return false; }

    auto const optResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(m_request.GetContext())
        .SetSource(*optNodeIdentifier)
        .SetDestination(m_request.GetSource())
        .SetRoute(m_request.GetRoute())
        .SetPayload(li::json_object_vector(s::identifier, s::data).encode(responses))
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Binding::Response,
            optExtension->get().GetTracker())
        .ValidatedBuild();
    assert(optResponse);

    // After the aggregate response has been generated the tracked responses can be cleared. Any subsequent responses
    // will be rejected by the service.
    m_responses.clear();

    if (auto const spRequestor = m_wpRequestor.lock(); spRequestor) {
        return spRequestor->ScheduleSend(m_request.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::DeferredTracker::Entry::Entry(Node::SharedIdentifier const& spIdentifier, Message::Payload&& payload)
    : m_spIdentifier(spIdentifier)
    , m_payload(std::move(payload))
{
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& Awaitable::DeferredTracker::Entry::GetIdentifier() const
{
    return m_spIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

Node::Internal::Identifier const& Awaitable::DeferredTracker::Entry::GetInternalIdentifier() const
{
    assert(m_spIdentifier);
    return static_cast<Node::Internal::Identifier const&>(*m_spIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

Message::Payload const& Awaitable::DeferredTracker::Entry::GetPayload() const { return m_payload; }

//----------------------------------------------------------------------------------------------------------------------
bool Awaitable::DeferredTracker::Entry::IsEmpty() const { return m_payload.IsEmpty(); }

//----------------------------------------------------------------------------------------------------------------------

void Awaitable::DeferredTracker::Entry::SetPayload(Message::Payload&& payload) { m_payload = std::move(payload); }

//----------------------------------------------------------------------------------------------------------------------
