//----------------------------------------------------------------------------------------------------------------------
// File: Publisher.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Publisher.hpp"
//----------------------------------------------------------------------------------------------------------------------

Event::Publisher::Publisher()
    : m_hasSuspendedSubscriptions(false)
    , m_listeners()
    , m_eventsMutex()
    , m_events()
    , m_advertisedMutex()
    , m_advertised()
{
    assert(Assertions::IsSubscriberThread()); // Ensure the state is set to the thread has created the publisher.
}

//----------------------------------------------------------------------------------------------------------------------

void Event::Publisher::SuspendSubscriptions()
{
    assert(Assertions::IsSubscriberThread()); // Only the thread that is allowed to subscribe can disable it. 
    m_hasSuspendedSubscriptions = true;
}

//----------------------------------------------------------------------------------------------------------------------

void Event::Publisher::Advertise(Type type)
{
    std::scoped_lock lock(m_advertisedMutex);
    m_advertised.emplace(type);
}

//----------------------------------------------------------------------------------------------------------------------

void Event::Publisher::Advertise(EventAdvertisements&& advertised)
{
    std::scoped_lock lock(m_advertisedMutex);
    m_advertised.merge(advertised);
}

//----------------------------------------------------------------------------------------------------------------------

bool Event::Publisher::IsSubscribed(Type type) const
{
    return m_listeners.contains(type);
}

//----------------------------------------------------------------------------------------------------------------------

bool Event::Publisher::IsAdvertised(Type type) const
{
    std::shared_lock lock(m_advertisedMutex);
    return m_advertised.contains(type);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Event::Publisher::EventCount() const
{
    std::scoped_lock lock(m_eventsMutex);
    return m_events.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Event::Publisher::ListenerCount() const
{
    return m_listeners.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Event::Publisher::AdvertisedCount() const
{
    std::shared_lock Lock(m_advertisedMutex);
    return m_advertised.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Event::Publisher::Dispatch()
{
    // Pull and clear the publisher's queued events to quickly unblock future events.
    auto const events = (std::scoped_lock{m_eventsMutex}, std::exchange(m_events, {}));
    for (auto const& upEventProxy : events) {
        // Get the listeners for the event. If there is an event to be published, it should be guarenteed that there
        // is at least one listener for that particular event. 
        auto const& listeners = m_listeners[upEventProxy->GetType()];
        assert(listeners.size() != 0); 
        for (auto const& listenerProxy : listeners) {
            // Call the listener and event proxies to handle publishing the event for that listener. 
            std::invoke(listenerProxy, upEventProxy);
        }
    }
    return events.size(); // Return the number of events published.
}

//----------------------------------------------------------------------------------------------------------------------

void Event::Publisher::Publish(Type type, EventProxy&& upEventProxy)
{
    assert(m_hasSuspendedSubscriptions); // We assert that subscriptions has been suspended before publsihing has begun.

    // If there are no listeners for the specified event, there is point in adding it to the queue. All event listeners
    // should be registered before starting the node. 
    if (!IsSubscribed(type)) { return; }

    std::scoped_lock lock(m_eventsMutex);
    m_events.emplace_back(std::move(upEventProxy));
}

//----------------------------------------------------------------------------------------------------------------------

bool Event::Assertions::IsSubscriberThread()
{
    // The intent of this method is to verify the listener container does not need to be protected by a lock. 
    // There are two assumptions:
        // 1.) All subscriptions occur on the main thread.
        // 2.) Publishing does not begin until the main thread has suspended subscriptions. 
    // The constructor must call this method first to properly initialize the identifier to the main thread's value.  
    static auto const thread = std::this_thread::get_id();
    return (thread == std::this_thread::get_id());
}

//----------------------------------------------------------------------------------------------------------------------
