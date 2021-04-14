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
{
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

std::size_t Event::Publisher::PublishEvents()
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
