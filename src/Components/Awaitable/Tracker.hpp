//----------------------------------------------------------------------------------------------------------------------
// File: Tracker.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Identifier/IdentifierTypes.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Message/Payload.hpp"
#include "Components/Peer/Action.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json/array.hpp>
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
namespace Awaitable {
//----------------------------------------------------------------------------------------------------------------------

class ITracker;
class DeferredTracker;
class RequestTracker;

//----------------------------------------------------------------------------------------------------------------------
} // Awaitable namespace
//----------------------------------------------------------------------------------------------------------------------

class Awaitable::ITracker
{
public:
    static constexpr auto ExpirationPeriod = std::chrono::milliseconds{ 1'500 };

    enum class Status : std::uint32_t { Pending, Fulfilled, Completed };
    enum class UpdateResult : std::uint32_t { Expired, Unexpected, Success, Partial, Fulfilled };

    virtual ~ITracker() = default;

    [[nodiscard]] TrackerKey GetKey() const;
    [[nodiscard]] Status GetStatus() const;
    [[nodiscard]] Status CheckStatus();
    [[nodiscard]] std::size_t GetExpected() const;
    [[nodiscard]] std::size_t GetReceived() const;

    [[nodiscard]] virtual bool Correlate(Node::SharedIdentifier const& identifier) = 0;
    [[nodiscard]] virtual UpdateResult Update(Message::Application::Parcel&& message) = 0;
    [[nodiscard]] virtual UpdateResult Update(
        Node::Identifier const& identifier, Message::Payload&& data) = 0;
    [[nodiscard]] virtual bool Fulfill() = 0;

protected:
    ITracker(TrackerKey key, std::size_t expected);

    TrackerKey m_key;
    Status m_status;
    std::size_t m_expected;
    std::size_t m_received;
    std::chrono::steady_clock::time_point const m_expire;
};

//----------------------------------------------------------------------------------------------------------------------

class Awaitable::RequestTracker : public Awaitable::ITracker
{
public:
    RequestTracker(
        TrackerKey key, 
        std::weak_ptr<Peer::Proxy> const& wpProxy,
        Peer::Action::OnResponse const& onResponse,
        Peer::Action::OnError const& onError);

    RequestTracker(
        TrackerKey key,
        std::size_t expected,
        Peer::Action::OnResponse const& onResponse,
        Peer::Action::OnError const& onError);

    // ITracker {
    [[nodiscard]] virtual bool Correlate(Node::SharedIdentifier const& identifier) override;
    [[nodiscard]] virtual UpdateResult Update(Message::Application::Parcel&& message) override;
    [[nodiscard]] virtual UpdateResult Update(
        Node::Identifier const& identifier, Message::Payload&& data) override;
    [[nodiscard]] virtual bool Fulfill() override;
    // } ITracker

private:
    std::unordered_map<Node::SharedIdentifier, bool, Node::SharedIdentifierHasher> m_ledger;
    std::vector<Message::Application::Parcel> m_responses;
    Peer::Action::OnResponse m_onResponse;
    Peer::Action::OnError m_onError;
    std::size_t m_remaining;
};

//----------------------------------------------------------------------------------------------------------------------

class Awaitable::DeferredTracker : public Awaitable::ITracker
{
public:
    DeferredTracker(
        TrackerKey key, 
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        Message::Application::Parcel const& request,
        std::vector<Node::SharedIdentifier> const& identifiers);

    // ITracker {
    [[nodiscard]] virtual bool Correlate(Node::SharedIdentifier const& identifier) override;
    [[nodiscard]] virtual UpdateResult Update(Message::Application::Parcel&& message) override;
    [[nodiscard]] virtual UpdateResult Update(
        Node::Identifier const& identifier, Message::Payload&& data) override;
    [[nodiscard]] virtual bool Fulfill() override;
    // } ITracker

private:
    std::weak_ptr<Peer::Proxy> m_wpRequestor;
    Message::Application::Parcel const m_request;
    boost::json::object m_responses;
};

//----------------------------------------------------------------------------------------------------------------------
