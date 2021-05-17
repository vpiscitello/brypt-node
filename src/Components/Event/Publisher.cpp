//----------------------------------------------------------------------------------------------------------------------
// File: EventPublisher.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Publisher.hpp"
//----------------------------------------------------------------------------------------------------------------------

Event::Publisher::Publisher()
    : m_listenersMutex()
    , m_listeners()
    , m_eventsMutex()
    , m_events()
    , m_advertisedMutex()
    , m_advertised()
{
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
    std::scoped_lock Lock(m_listenersMutex);
    return m_listeners.find(type) != m_listeners.end();
}

//----------------------------------------------------------------------------------------------------------------------

bool Event::Publisher::IsAdvertised(Type type) const
{
    std::shared_lock Lock(m_advertisedMutex);
    return m_advertised.find(type) != m_advertised.end();
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
    std::scoped_lock lock(m_listenersMutex);
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

    std::scoped_lock lock(m_listenersMutex);
    for (auto const& upEventProxy : events) {
        // Get the listeners for the event. If there is an event to be published, it should be guarenteed that there
        // is at least one listener for that particular event. 
        auto const& listeners = m_listeners[upEventProxy->Type()];
        assert(listeners.size() != 0); 
        for (auto const& listenerProxy : listeners) {
            // Call the listener and event proxies to handle publishing the event for that listener. 
            std::invoke(listenerProxy, upEventProxy);
        }
    }
    return events.size(); // Return the number of events published.
}

//----------------------------------------------------------------------------------------------------------------------
