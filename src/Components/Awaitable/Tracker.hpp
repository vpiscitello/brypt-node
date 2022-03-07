//----------------------------------------------------------------------------------------------------------------------
// File: Tracker.hpp
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
namespace Awaitable {
//----------------------------------------------------------------------------------------------------------------------

class ITracker;
class AggregateTracker;

//----------------------------------------------------------------------------------------------------------------------
} // Awaitable namespace
//----------------------------------------------------------------------------------------------------------------------

class Awaitable::ITracker
{
public:
    static constexpr auto ExpirationPeriod = std::chrono::milliseconds{ 1'500 };

    enum class Status : std::uint32_t { Pending, Fulfilled, Completed };
    enum class UpdateResult : std::uint32_t { Expired, Unexpected, Success, Fulfilled };

    virtual ~ITracker() = default;

    [[nodiscard]] Status GetStatus() const;
    [[nodiscard]] Status CheckStatus();
    [[nodiscard]] std::size_t GetExpected() const;
    [[nodiscard]] std::size_t GetReceived() const;

    [[nodiscard]] virtual UpdateResult Update(Message::Application::Parcel const& response) = 0;
    [[nodiscard]] virtual UpdateResult Update(Node::Identifier const& identifier, std::string_view data) = 0;
    [[nodiscard]] virtual bool Fulfill() = 0;

protected:
    explicit ITracker(std::size_t expected);

    Status m_status;
    std::size_t m_expected;
    std::size_t m_received;
    std::chrono::steady_clock::time_point const m_expire;
};

//----------------------------------------------------------------------------------------------------------------------

class Awaitable::AggregateTracker : public Awaitable::ITracker
{
public:
    AggregateTracker(
        std::weak_ptr<Peer::Proxy> const& wpRequestor,
        Message::Application::Parcel const& request,
        std::vector<Node::SharedIdentifier> const& identifiers);

    // ITracker {
    [[nodiscard]] virtual UpdateResult Update(Message::Application::Parcel const& response) override;
    [[nodiscard]] virtual UpdateResult Update(Node::Identifier const& identifier, std::string_view data) override;
    [[nodiscard]] virtual bool Fulfill() override;
    // } ITracker

private:
    class Entry {
    public:
        Entry(Node::SharedIdentifier const& spNodeIdentifier, std::string_view pack);
        
        [[nodiscard]] Node::SharedIdentifier const& GetIdentifier() const;
        [[nodiscard]] Node::Internal::Identifier const& GetInternalIdentifier() const;

        [[nodiscard]] std::string const& GetData() const;
        [[nodiscard]] bool IsEmpty() const;
        void SetData(std::string const& data);

    private:
        Node::SharedIdentifier const m_spIdentifier;
        std::string m_data;
    };

    [[nodiscard]] UpdateResult Update(Node::Identifier const& identifier, std::span<std::uint8_t const> data);

    using Responses = boost::multi_index_container<
        Entry,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::const_mem_fun<
                    Entry, Node::Internal::Identifier const&, &Entry::GetInternalIdentifier>>>>;

    std::weak_ptr<Peer::Proxy> m_wpRequestor;
    Message::Application::Parcel const m_request;
    Responses m_responses;
};

//----------------------------------------------------------------------------------------------------------------------
