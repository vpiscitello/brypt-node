//----------------------------------------------------------------------------------------------------------------------
// File: Publisher.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Publisher.hpp"
#include "Components/Scheduler/Delegate.hpp"
#include "Components/Scheduler/Registrar.hpp"
//----------------------------------------------------------------------------------------------------------------------

Event::Publisher::Publisher(std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
    , m_hasSuspendedSubscriptions(false)
    , m_listeners()
    , m_eventsMutex()
    , m_events()
    , m_advertisedMutex()
    , m_advertised()
{
    assert(Assertions::Threading::IsCoreThread());

    // There are two assumptions:
        // 1.) All subscriptions occur on the main thread.
        // 2.) Publishing does not begin until the main thread has suspended subscriptions. 

    m_spDelegate = spRegistrar->Register<Publisher>([this] (Scheduler::Frame const&) -> std::size_t {
        return Dispatch();  // Dispatch any generated events that have been published since the last cycle. 
    }); 
    
    assert(m_spDelegate);
}

//----------------------------------------------------------------------------------------------------------------------

Event::Publisher::~Publisher()
{
    m_spDelegate->Delist();
}

//----------------------------------------------------------------------------------------------------------------------

void Event::Publisher::SuspendSubscriptions()
{
    assert(Assertions::Threading::IsCoreThread()); // Only the main thread should suspend supscriptions.
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
    auto const events = ( std::scoped_lock{ m_eventsMutex }, std::exchange(m_events, {}) );
    for (auto const& upEventProxy : events) {
        // Get the listeners for the event. If there is an event to be published, it should be guarantied that there
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
    assert(m_hasSuspendedSubscriptions); // We assert that subscriptions has been suspended before publishing has begun.

    // If there are no listeners for the specified event, there is point in adding it to the queue. All event listeners
    // should be registered before starting the node. 
    if (!IsSubscribed(type)) { return; }

    std::scoped_lock lock(m_eventsMutex);
    m_events.emplace_back(std::move(upEventProxy));
    m_spDelegate->OnTaskAvailable();
}

//----------------------------------------------------------------------------------------------------------------------
