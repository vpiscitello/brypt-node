//------------------------------------------------------------------------------------------------
// File: Await.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Await.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Utilities/NodeUtils.hpp"
#include <algorithm>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Intended for multiple peers with responses from each
//------------------------------------------------------------------------------------------------
Await::CMessageObject::CMessageObject(
    CMessage const& request,
    std::set<NodeUtils::NodeIdType> const& peerNames,
    std::uint32_t const expected)
    : m_fulfilled(false)
    , m_expected(expected)
    , m_received(0)
    , m_request(request)
    , m_aggregateObject()
    , m_expire(NodeUtils::GetSystemTimePoint() + Await::Timeout)
{
    BuildResponseObject(peerNames);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Intended for a single peer
//------------------------------------------------------------------------------------------------
Await::CMessageObject::CMessageObject(
    CMessage const& request,
    NodeUtils::NodeIdType const& peerName,
    std::uint32_t expected)
    : m_fulfilled(false)
    , m_expected(expected)
    , m_received(0)
    , m_request(request)
    , m_aggregateObject()
    , m_expire(NodeUtils::GetSystemTimePoint() + Await::Timeout)
{
    m_aggregateObject[std::to_string(peerName)] = "Unfulfilled";
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Iterates over the set of peers and creates a response object for each
//------------------------------------------------------------------------------------------------
void Await::CMessageObject::BuildResponseObject(std::set<NodeUtils::NodeIdType> const& peerNames)
{
    NodeUtils::NodeIdType const& reqSourceId = m_request.GetSourceId();
    for (auto itr = peerNames.begin(); itr != peerNames.end(); ++itr) {
        if (*itr == reqSourceId) {
            continue;
        }
        m_aggregateObject[std::to_string(*itr)] = "Unfulfilled";
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Determines whether or not the await object is ready. It is ready if it has m_received all responses requested, or it has timed-out.
// Returns: true if the object is ready and false otherwise.
//------------------------------------------------------------------------------------------------
bool Await::CMessageObject::Ready()
{
    if (m_expected == m_received) {
        m_fulfilled = true;
    } else {
        if (m_expire < NodeUtils::GetSystemTimePoint()) {
            m_fulfilled = true;
        }
    }

    return m_fulfilled;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Gathers information from the aggregate object and packages it into a new message.
// Returns: The aggregate message.
//------------------------------------------------------------------------------------------------
CMessage Await::CMessageObject::GetResponse()
{
    std::string data = std::string();

    if (m_fulfilled) {
        json11::Json const aggregateJson = json11::Json::object(m_aggregateObject);
        data = aggregateJson.dump();
    }

    return CMessage(
        m_request.GetDestinationId(),
        m_request.GetSourceId(),
        m_request.GetCommand(),
        m_request.GetPhase() + 1,
        data,
        m_request.GetNonce() + 1
     );
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: This places a response message into the aggregate object for this await object.
// Returns: true if the await object is fulfilled, false otherwise.
//------------------------------------------------------------------------------------------------
bool Await::CMessageObject::UpdateResponse(CMessage const& response)
{
    NodeUtils::NodeIdType const& id = response.GetSourceId();
    std::string const source = std::to_string(id);
    auto const itr = m_aggregateObject.find(source);

    if(itr == m_aggregateObject.end()) {
        return false;
    }

    if (m_aggregateObject[source].dump() != "\"Unfulfilled\"") {
        NodeUtils::printo("Unexpected node response", NodeUtils::PrintType::AWAIT);
        return m_fulfilled;
    }

    m_aggregateObject[source] = response.GetPack();

    if (++m_received >= m_expected) {
        m_fulfilled = true;
    }

    return m_fulfilled;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// AwaitObject (Multiple peers) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//------------------------------------------------------------------------------------------------
NodeUtils::ObjectIdType Await::CObjectContainer::PushRequest(
    CMessage const& message,
    std::set<NodeUtils::NodeIdType> const& peerNames,
    std::uint32_t m_expected)
{
    NodeUtils::ObjectIdType const key = KeyGenerator(message.GetPack());
    NodeUtils::printo("Pushing AwaitObject with key: " + std::to_string(key), NodeUtils::PrintType::AWAIT);
    m_awaiting.emplace(key, CMessageObject(message, peerNames, m_expected));
    return key;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// AwaitObject (Single peer) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//------------------------------------------------------------------------------------------------
NodeUtils::ObjectIdType Await::CObjectContainer::PushRequest(
    CMessage const& message,
    NodeUtils::NodeIdType const& peerName,
    std::uint32_t m_expected)
{
    NodeUtils::ObjectIdType const key = KeyGenerator(message.GetPack());
    NodeUtils::printo("Pushing AwaitObject with key: " + std::to_string(key), NodeUtils::PrintType::AWAIT);
    m_awaiting.emplace(key, CMessageObject(message, peerName, m_expected));
    return key;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Pushes a response message onto the await object for the given key (in the AwaitMap)
// Returns: boolean indicating success or not
//------------------------------------------------------------------------------------------------
bool Await::CObjectContainer::PushResponse(NodeUtils::ObjectIdType const& key, CMessage const& message)
{
    bool success = true;

    NodeUtils::printo("Pushing response to AwaitObject " + std::to_string(key), NodeUtils::PrintType::AWAIT);
    bool const m_fulfilled = m_awaiting.at(key).UpdateResponse(message);
    if (m_fulfilled) {
        NodeUtils::printo("AwaitObject has been m_fulfilled, Waiting to transmit", NodeUtils::PrintType::AWAIT);
    }

    return success;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Pushes a response message onto the await object, finds the key from the message
// await ID
// Returns: boolean indicating success or not
//------------------------------------------------------------------------------------------------
bool Await::CObjectContainer::PushResponse(CMessage const& message)
{
    // Try to get an await object ID from the message
    auto const& optKey = message.GetAwaitId();
    if (!optKey) {
        return false;
    }

    // Try to find the awaiting object in the awaiting container
    auto const itr = m_awaiting.find(*optKey);
    if(itr == m_awaiting.end()) {
        return false;
    }

    // Update the response to the waiting message with th new message
    NodeUtils::printo("Pushing response to AwaitObject " + std::to_string(*optKey), NodeUtils::PrintType::AWAIT);
    bool const m_fulfilled = itr->second.UpdateResponse(message);
    if (m_fulfilled) {
        NodeUtils::printo("AwaitObject has been m_fulfilled, Waiting to transmit", NodeUtils::PrintType::AWAIT);
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Iterates over the await objects within the AwaitMap and pushes ready responses
// onto a local m_fulfilled vector of messages.
// Returns: the vector of messages that have been m_fulfilled.
//------------------------------------------------------------------------------------------------
std::vector<CMessage> Await::CObjectContainer::GetFulfilled()
{
    std::vector<CMessage> m_fulfilled;
    m_fulfilled.reserve(m_awaiting.size());

    for (auto it = m_awaiting.begin(); it != m_awaiting.end();) {
        NodeUtils::printo("Checking AwaitObject " + std::to_string(it->first), NodeUtils::PrintType::AWAIT);
        if (it->second.Ready()) {
            CMessage const& response = it->second.GetResponse();
            m_fulfilled.push_back(response);
            it = m_awaiting.erase(it);
        } else {
            ++it;
        }
    }

    return m_fulfilled;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Await::CObjectContainer::Empty() const
{
    return m_awaiting.empty();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
NodeUtils::ObjectIdType Await::CObjectContainer::KeyGenerator(std::string const& pack) const
{
    std::uint8_t digest[MD5_DIGEST_LENGTH];

    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, pack.data(), pack.size());
    MD5_Final(digest, &ctx);

    NodeUtils::ObjectIdType key = 0;
    std::copy(&digest[0], &digest[sizeof(key)], &key); // Truncate the 128 bit hash to 32 bits
    return key;

    /* char hash_cstr[33];
    for (int idx = 0; idx < MD5_DIGEST_LENGTH; ++idx) {
        sprintf(&hash_cstr[idx * 2], "%02x", static_cast<std::uint32_t>(digest[idx]));
    } */
}

//------------------------------------------------------------------------------------------------
