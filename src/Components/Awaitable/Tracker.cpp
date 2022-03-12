//----------------------------------------------------------------------------------------------------------------------
// File: Tracker.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Tracker.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/Z85.hpp"
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
    : m_status(Status::Pending)
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
    std::weak_ptr<Peer::Proxy> const& wpProxy,
    Peer::Action::OnResponse const& onResponse,
    Peer::Action::OnError const& onError)
    : ITracker(1)
    , m_spRequestee()
    , m_optResponse()
    , m_onResponse(onResponse)
    , m_onError(onError)
    , m_wpProxy(wpProxy)
{
    if (auto const spProxy = wpProxy.lock(); spProxy) {
        m_spRequestee = spProxy->GetIdentifier();
    }
    assert(m_spRequestee);
    assert(m_onResponse);
    assert(m_onError);
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::UpdateResult Awaitable::RequestTracker::Update(Message::Application::Parcel const& response)
{
    if (m_expire < std::chrono::steady_clock::now()) { return UpdateResult::Expired; }
    if(m_optResponse || response.GetSource() != *m_spRequestee) { return UpdateResult::Unexpected; }
    m_optResponse = std::move(response);
    ++m_received;
    return UpdateResult::Fulfilled;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::UpdateResult Awaitable::RequestTracker::Update(
    [[maybe_unused]] Node::Identifier const& identifier, [[maybe_unused]] std::string_view data)
{
    return UpdateResult::Unexpected;
}

//----------------------------------------------------------------------------------------------------------------------

bool Awaitable::RequestTracker::Fulfill()
{
    if(m_status != Status::Fulfilled) { return false; }
    m_status = Status::Completed;

    if (!m_optResponse) {
        m_onError(Peer::Action::Error::Expired);
        return true;
    }

    m_onResponse(*m_optResponse);
    return true;
}

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

Awaitable::ITracker::UpdateResult Awaitable::AggregateTracker::Update(Message::Application::Parcel const& response)
{
    return Update(response.GetSource(), response.GetPayload());
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::UpdateResult Awaitable::AggregateTracker::Update(
    Node::Identifier const& identifier, std::string_view data)
{
    return Update(identifier, { reinterpret_cast<std::uint8_t const*>(data.begin()), data.size() });
}

//----------------------------------------------------------------------------------------------------------------------

bool Awaitable::AggregateTracker::Fulfill()
{
    if(m_status != Status::Fulfilled) { return false; }
    m_status = Status::Completed;

    struct ResponseItem {
        std::string const& identifier;
        std::string const& pack;
    };
    std::vector<ResponseItem> responses;
    std::transform(m_responses.begin(), m_responses.end(), std::back_inserter(responses), [] (auto const& entry) {
        assert(entry.GetIdentifier());
        return ResponseItem{ *entry.GetIdentifier(), entry.GetData() };
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

    // After the aggregate response has been generated the tracked responses can be cleared. Any subsequent responses
    // will be rejected by the service.
    m_responses.clear();

    if (auto const spRequestor = m_wpRequestor.lock(); spRequestor) {
        return spRequestor->ScheduleSend(m_request.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::ITracker::UpdateResult Awaitable::AggregateTracker::Update(
    Node::Identifier const& identifier, std::span<std::uint8_t const> data)
{
    if (m_expire < std::chrono::steady_clock::now()) { return UpdateResult::Expired; }

    auto const itr = m_responses.find(identifier);
    if(itr == m_responses.end() || !itr->IsEmpty()) { return UpdateResult::Unexpected; }

    m_responses.modify(itr, [&data] (Entry& entry) { entry.SetData(Z85::Encode(data)); });

    if (++m_received >= m_expected) {
        m_status = Status::Fulfilled;
        return UpdateResult::Fulfilled;
    }

    return UpdateResult::Success;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::AggregateTracker::Entry::Entry(Node::SharedIdentifier const& spIdentifier, std::string_view data)
    : m_spIdentifier(spIdentifier)
    , m_data(data)
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

std::string const& Awaitable::AggregateTracker::Entry::GetData() const { return m_data; }

//----------------------------------------------------------------------------------------------------------------------
bool Awaitable::AggregateTracker::Entry::IsEmpty() const { return m_data.empty(); }

//----------------------------------------------------------------------------------------------------------------------

void Awaitable::AggregateTracker::Entry::SetData(std::string const& data) { m_data = data; }

//----------------------------------------------------------------------------------------------------------------------
