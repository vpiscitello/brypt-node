//----------------------------------------------------------------------------------------------------------------------
// File: Publisher.hpp
// Description: Handle the subscription and publishing of events.
// All systems that don't have/need immediate effects should use an instance of the publisher. 
// Notes: Event subscriptions are not thread safe.  Only the main thread is allowed to subscribe and consequently must
// disable subscriptions before starting the event loop or spawning publishing threads. The reasoning behind this is
// that reads (i.e. publishing) are substantially more common. Needing to lock a listener and event mutex during each
// publish and invocation is not ideal. 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Events.hpp"
#include "SharedPublisher.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <concepts>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Event {
//----------------------------------------------------------------------------------------------------------------------

class Publisher;

//----------------------------------------------------------------------------------------------------------------------
namespace Assertions {
//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] bool IsSubscriberThread();

//----------------------------------------------------------------------------------------------------------------------
} // Assertions namespace
//----------------------------------------------------------------------------------------------------------------------
} // Event namespace
//----------------------------------------------------------------------------------------------------------------------

class Event::Publisher
{
public:
    using EventAdvertisements = std::set<Type>;

    Publisher();
    ~Publisher() = default;

    Publisher(Publisher const&) = delete; 
    Publisher(Publisher&& ) = delete; 
    Publisher& operator=(Publisher const&) = delete; 
    Publisher& operator=(Publisher&&) = delete; 

    template <Type EventType> requires MessageWithContent<EventType>
    [[maybe_unused]] bool Subscribe(typename Event::Message<EventType>::CallbackTrait const& callback);

    template <Type EventType> requires MessageWithoutContent<EventType>
    [[maybe_unused]] bool Subscribe(typename Event::Message<EventType>::CallbackTrait const& callback);

    void SuspendSubscriptions();
    
    void Advertise(Type type);
    void Advertise(EventAdvertisements&& advertised);

    template <Type EventType> requires MessageWithContent<EventType>
    void Publish(typename Event::Message<EventType>::EventContent&& content);

    template <Type EventType> requires MessageWithoutContent<EventType>
    void Publish();

    bool IsSubscribed(Type type) const;
    bool IsAdvertised(Type type) const;

    std::size_t EventCount() const;
    std::size_t ListenerCount() const;
    std::size_t AdvertisedCount() const;

    std::size_t Dispatch();

private:
    using EventProxy = std::unique_ptr<IMessage>;
    using EventQueue = std::deque<EventProxy>;
    using ListenerProxy = std::function<void(EventProxy const& upEventProxy)>;
    using Listeners = std::unordered_map<Type, std::vector<ListenerProxy>>;

    std::atomic_bool m_hasSuspendedSubscriptions;
    Listeners m_listeners;
    
    mutable std::mutex m_eventsMutex;
    EventQueue m_events;

    mutable std::shared_mutex m_advertisedMutex;
    EventAdvertisements m_advertised;
};

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type EventType> requires Event::MessageWithContent<EventType>
bool Event::Publisher::Subscribe(typename Event::Message<EventType>::CallbackTrait const& callback)
{
    assert(Assertions::IsSubscriberThread());
    if (m_hasSuspendedSubscriptions) { return false; } // Subscriptions are disabled when the event loop begins. 
    
    // In the case of events with content, the listener will need to cast to the derived event type to access the 
    // event's content and then forward them to the supplied handler. 
    m_listeners[EventType].emplace_back(
        [callback] (EventProxy const& upEventProxy) {
            // Dispatch needs to ensure this callback is provided the correct event. 
            assert(upEventProxy && upEventProxy->Type() == EventType);
            auto const pEvent = static_cast<Event::Message<EventType> const*>(upEventProxy.get());
            std::apply(callback, pEvent->GetContent());
        });
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type EventType> requires Event::MessageWithoutContent<EventType>
bool Event::Publisher::Subscribe(typename Event::Message<EventType>::CallbackTrait const& callback)
{
    assert(Assertions::IsSubscriberThread());
    if (m_hasSuspendedSubscriptions) { return false; } // Subscriptions are disabled when the event loop begins. 
    
    // In the case of events without content, the listener will simply invoke the callback.
    m_listeners[EventType].emplace_back([callback] (EventProxy const&) { std::invoke(callback); });
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type EventType> requires Event::MessageWithContent<EventType>
void Event::Publisher::Publish(typename Event::Message<EventType>::EventContent&& content)
{
    assert(m_hasSuspendedSubscriptions); // We assert that subscriptions has been suspended before publsihing has begun.
    // If there are no listeners for the specified event, there is point in adding it to the queue. All event listeners
    // should be registered before starting the node. 
    if (!IsSubscribed(EventType)) { return; }

    // In the case of events with content, we need to forward the event's content to when creating the queued event. 
    EventProxy upEventProxy = std::make_unique<Event::Message<EventType>>(std::move(content));
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(std::move(upEventProxy));
    }
}

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type EventType> requires Event::MessageWithoutContent<EventType>
void Event::Publisher::Publish()
{
    assert(m_hasSuspendedSubscriptions); // We assert that subscriptions has been suspended before publsihing has begun.
    // If there are no listeners for the specified event, there is point in adding it to the queue. All event listeners
    // should be registered before starting the node. 
    if (!IsSubscribed(EventType)) { return; }

    // In the case of events with content, we simply need to create the event to queue it. 
    EventProxy upEventProxy = std::make_unique<Event::Message<EventType>>();
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(std::move(upEventProxy));
    }
}

//----------------------------------------------------------------------------------------------------------------------
