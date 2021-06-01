//----------------------------------------------------------------------------------------------------------------------
// File: Publisher.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Events.hpp"
//----------------------------------------------------------------------------------------------------------------------
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
    void Subscribe(typename Event::Message<EventType>::CallbackTrait const& callback);

    template <Type EventType> requires MessageWithoutContent<EventType>
    void Subscribe(typename Event::Message<EventType>::CallbackTrait const& callback);
    
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

    mutable std::mutex m_listenersMutex;
    Listeners m_listeners;
    
    mutable std::mutex m_eventsMutex;
    EventQueue m_events;

    mutable std::shared_mutex m_advertisedMutex;
    EventAdvertisements m_advertised;
};

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type EventType> requires Event::MessageWithContent<EventType>
void Event::Publisher::Subscribe(typename Event::Message<EventType>::CallbackTrait const& callback)
{
    // In the case of events with content, the listener will need to cast to the derived event type to access the 
    // event's content and then forward them to the supplied handler. 
    std::scoped_lock lock(m_listenersMutex);
    m_listeners[EventType].emplace_back(
        [callback] (EventProxy const& upEventProxy) {
            // Dispatch needs to ensure this callback is provided the correct event. 
            assert(upEventProxy && upEventProxy->Type() == EventType);
            auto const pEvent = static_cast<Event::Message<EventType> const*>(upEventProxy.get());
            std::apply(callback, pEvent->GetContent());
        });
}

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type EventType> requires Event::MessageWithoutContent<EventType>
void Event::Publisher::Subscribe(typename Event::Message<EventType>::CallbackTrait const& callback)
{
    // In the case of events without content, the listener will simply invoke the callback without needing to care 
    // about the event details. 
    std::scoped_lock lock(m_listenersMutex);
    m_listeners[EventType].emplace_back([callback] (EventProxy const&) { std::invoke(callback); });
}

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type EventType> requires Event::MessageWithContent<EventType>
void Event::Publisher::Publish(typename Event::Message<EventType>::EventContent&& content)
{
    // If there are no listeners for the specified event, there is point in adding it to the queue. All event listeners
    // should be registered before starting the node. 
    {
        std::scoped_lock lock(m_listenersMutex);
        if (!m_listeners.contains(EventType)) { return; }
    }

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
    // If there are no listeners for the specified event, there is point in adding it to the queue. All event listeners
    // should be registered before starting the node. 
    {
        std::scoped_lock lock(m_listenersMutex);
        if (!m_listeners.contains(EventType)) { return; }
    }

    // In the case of events with content, we simply need to create the event to queue it. 
    EventProxy upEventProxy = std::make_unique<Event::Message<EventType>>();
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(std::move(upEventProxy));
    }
}

//----------------------------------------------------------------------------------------------------------------------
