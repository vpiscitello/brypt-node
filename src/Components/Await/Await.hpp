//------------------------------------------------------------------------------------------------
// File: Await.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Utilities/Message.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/Json11/json11.hpp"
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
class CMessageObject;
class CObjectContainer;

using TimePeriod = std::chrono::milliseconds;
using MessageMap = std::unordered_map<NodeUtils::ObjectIdType, CMessageObject>;

constexpr TimePeriod Timeout = std::chrono::milliseconds(1500);
//------------------------------------------------------------------------------------------------
} // Await namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class Await::CMessageObject
{
public:
    CMessageObject(
        CMessage const& request,
        std::set<NodeUtils::NodeIdType> const& peerNames,
        std::uint32_t const expected);

    CMessageObject(CMessage const& request, NodeUtils::NodeIdType const& peerName, std::uint32_t expected);

    void BuildResponseObject(std::set<NodeUtils::NodeIdType> const& peerNames);
    bool Ready();
    CMessage GetResponse();
    bool UpdateResponse(CMessage const& response);

private:
    bool m_fulfilled;
    std::uint32_t m_expected;
    std::uint32_t m_received;

    CMessage const m_request;

    json11::Json::object m_aggregateObject;

    NodeUtils::TimePoint const m_expire;
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
        std::set<NodeUtils::NodeIdType> const& peerNames,
        std::uint32_t expected);

    NodeUtils::ObjectIdType PushRequest(
        CMessage const& message,
        NodeUtils::NodeIdType const& peerName,
        std::uint32_t expected);

    bool PushResponse(NodeUtils::ObjectIdType const& key, CMessage const& message);
    bool PushResponse(CMessage const& message);
    std::vector<CMessage> GetFulfilled();
    bool Empty() const;

private:
    std::string KeyGenerator(std::string const& pack) const;

    MessageMap m_awaiting;
};

//------------------------------------------------------------------------------------------------
