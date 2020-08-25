//------------------------------------------------------------------------------------------------
// File: Await.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Message/Message.hpp"
#include "../../Utilities/NodeUtils.hpp"
#include "../../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <openssl/md5.h>
//------------------------------------------------------------------------------------------------
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Await {
//------------------------------------------------------------------------------------------------

enum class Status : std::uint8_t { Fulfilled, Unfulfilled };

struct TResponseObject;
class CMessageObject;
class CObjectContainer;

using MessageMap = std::unordered_map<NodeUtils::ObjectIdType, CMessageObject>;

constexpr TimeUtils::TimePeriod Timeout = std::chrono::milliseconds(1500);

//------------------------------------------------------------------------------------------------
} // Await namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------

struct Await::TResponseObject
{
    TResponseObject(
        BryptIdentifier::CContainer const id,
        std::string const& pack)
        : id(id)
        , pack(pack)
    {
    }
    BryptIdentifier::CContainer const id;
    std::string const pack;
};

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class Await::CMessageObject
{
public:
    CMessageObject(
        CMessage const& request,
        BryptIdentifier::CContainer const& peer);

    CMessageObject(
        CMessage const& request,
        std::set<BryptIdentifier::CContainer> const& peers);

    Await::Status GetStatus();

    std::optional<CMessage> GetResponse();
    Await::Status UpdateResponse(CMessage const& response);

private:
    Await::Status m_status;
    std::uint32_t m_expected;
    std::uint32_t m_received;

    CMessage const m_request;
    std::optional<CMessage> m_optAggregateResponse;

    std::unordered_map<BryptIdentifier::CContainer, std::string, BryptIdentifier::Hasher> m_responses;

    TimeUtils::Timepoint const m_expire;
};

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class Await::CObjectContainer
{
public:
    NodeUtils::ObjectIdType PushRequest(
        CMessage const& message,
        BryptIdentifier::CContainer const& peer);

    NodeUtils::ObjectIdType PushRequest(
        CMessage const& message,
        std::set<BryptIdentifier::CContainer> const& peers);

    bool PushResponse(CMessage const& message);
    std::vector<CMessage> GetFulfilled();

    bool IsEmpty() const;

private:
    NodeUtils::ObjectIdType KeyGenerator(std::string const& pack) const;

    MessageMap m_awaiting;
};

//------------------------------------------------------------------------------------------------
