//------------------------------------------------------------------------------------------------
// File: Await.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Await.hpp"
#include "../../Message/MessageBuilder.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
#include <algorithm>
//------------------------------------------------------------------------------------------------

IOD_SYMBOL(id)
IOD_SYMBOL(pack)

//------------------------------------------------------------------------------------------------
// Description: Intended for a single peer
//------------------------------------------------------------------------------------------------
Await::CMessageObject::CMessageObject(
    CMessage const& request,
    NodeUtils::NodeIdType const& peer)
    : m_status(Status::Unfulfilled)
    , m_expected(1)
    , m_received(0)
    , m_request(request)
    , m_optAggregateResponse()
    , m_responses()
    , m_expire(TimeUtils::GetSystemTimepoint() + Await::Timeout)
{
    m_responses[peer] = std::string();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Intended for multiple peers with responses from each
//------------------------------------------------------------------------------------------------
Await::CMessageObject::CMessageObject(
    CMessage const& request,
    std::set<NodeUtils::NodeIdType> const& peers)
    : m_status(Status::Unfulfilled)
    , m_expected(peers.size())
    , m_received(0)
    , m_request(request)
    , m_optAggregateResponse()
    , m_responses()
    , m_expire(TimeUtils::GetSystemTimepoint() + Await::Timeout)
{
    NodeUtils::NodeIdType const& source = m_request.GetSource();
    for (auto const& peer: peers) {
        if (peer == source) {
            continue;
        }
        m_responses[peer] = std::string();
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Determines whether or not the await object is ready. It is ready if it has 
// m_received all responses requested, or it has timed-out.
// Returns: true if the object is ready and false otherwise.
//------------------------------------------------------------------------------------------------
Await::Status Await::CMessageObject::GetStatus()
{
    if (m_expected == m_received || m_expire < TimeUtils::GetSystemTimepoint()) {
        m_status = Status::Fulfilled;
    } 

    return m_status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Gathers information from the aggregate object and packages it into a new message.
// Returns: The aggregate message.
//------------------------------------------------------------------------------------------------
std::optional<CMessage> Await::CMessageObject::GetResponse()
{
    if (m_status != Status::Fulfilled) {
        return {};
    }

    if (m_optAggregateResponse) {
        return m_optAggregateResponse;
    }

    std::vector<TResponseObject> responsesVector;
    std::transform(m_responses.begin(), m_responses.end(), std::back_inserter(responsesVector), 
                   [](auto const& response) -> TResponseObject { 
                       return {response.first, response.second}; });


    std::string data = iod::json_vector(s::id, s::pack).encode(responsesVector);

    m_optAggregateResponse = CMessage::Builder()
        .SetMessageContext(m_request.GetMessageContext())
        .SetSource(m_request.GetDestination())
        .SetDestination(m_request.GetSource())
        .SetCommand(m_request.GetCommandType(), m_request.GetPhase() + 1)
        .SetData(data, m_request.GetNonce() + 1)
        .ValidatedBuild();

    // After the aggregate response has been generated the tracked responses can be 
    // cleared. Therby rejecting any new responses.
    m_responses.clear();

    return m_optAggregateResponse;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: This places a response message into the aggregate object for this await object.
// Returns: true if the await object is fulfilled, false otherwise.
//------------------------------------------------------------------------------------------------
Await::Status Await::CMessageObject::UpdateResponse(CMessage const& response)
{
    auto const itr = m_responses.find(response.GetSource());
    if(itr == m_responses.end() || !itr->second.empty()) {
        NodeUtils::printo("Unexpected node response", NodeUtils::PrintType::Await);
        return m_status;
    }

    itr->second = response.GetPack();
    if (++m_received >= m_expected) {
        m_status = Status::Fulfilled;
    }

    return m_status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// AwaitObject (Single peer) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//------------------------------------------------------------------------------------------------
NodeUtils::ObjectIdType Await::CObjectContainer::PushRequest(
    CMessage const& message,
    NodeUtils::NodeIdType const& peer)
{
    NodeUtils::ObjectIdType const key = KeyGenerator(message.GetPack());
    NodeUtils::printo("Pushing AwaitObject with key: " + std::to_string(key), NodeUtils::PrintType::Await);
    m_awaiting.emplace(key, CMessageObject(message, peer));
    return key;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Creates an await key for a message. Registers the key and the newly created
// AwaitObject (Multiple peers) into the AwaitMap for this container
// Returns: the key for the AwaitMap
//------------------------------------------------------------------------------------------------
NodeUtils::ObjectIdType Await::CObjectContainer::PushRequest(
    CMessage const& message,
    std::set<NodeUtils::NodeIdType> const& peers)
{
    NodeUtils::ObjectIdType const key = KeyGenerator(message.GetPack());
    NodeUtils::printo("Pushing AwaitObject with key: " + std::to_string(key), NodeUtils::PrintType::Await);
    m_awaiting.emplace(key, CMessageObject(message, peers));
    return key;
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
    auto const& optKey = message.GetAwaitingKey();
    if (!optKey) {
        return false;
    }

    // Try to find the awaiting object in the awaiting container
    auto const itr = m_awaiting.find(*optKey);
    if(itr == m_awaiting.end()) {
        return false;
    }

    // Update the response to the waiting message with th new message
    NodeUtils::printo("Pushing response to AwaitObject " + std::to_string(*optKey), NodeUtils::PrintType::Await);
    auto const status = itr->second.UpdateResponse(message);
    if (status == Status::Fulfilled) {
        NodeUtils::printo("AwaitObject has been fulfilled, Waiting to transmit", NodeUtils::PrintType::Await);
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Iterates over the await objects within the AwaitMap and pushes ready responses
// onto a local fulfilled vector of messages.
// Returns: the vector of messages that have been fulfilled.
//------------------------------------------------------------------------------------------------
std::vector<CMessage> Await::CObjectContainer::GetFulfilled()
{
    std::vector<CMessage> fulfilled;
    fulfilled.reserve(m_awaiting.size());

    for (auto itr = m_awaiting.begin(); itr != m_awaiting.end();) {
        NodeUtils::printo("Checking AwaitObject " + std::to_string(itr->first), NodeUtils::PrintType::Await);
        if (itr->second.GetStatus() == Status::Fulfilled) {
            std::optional<CMessage> const optResponse = itr->second.GetResponse();
            if (optResponse) {
                fulfilled.push_back(*optResponse);
                itr = m_awaiting.erase(itr);
            }
        } else {
            ++itr;
        }
    }

    return fulfilled;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Await::CObjectContainer::IsEmpty() const
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
    std::memcpy(&key, digest, sizeof(key)); // Truncate the 128 bit hash to 32 bits
    return key;
}

//------------------------------------------------------------------------------------------------
