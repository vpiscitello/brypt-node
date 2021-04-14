//----------------------------------------------------------------------------------------------------------------------
// File: EventPublisher.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Events.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <concepts>
#include <memory>
#include <mutex>
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
    Publisher();
    ~Publisher() = default;

    Publisher(Publisher const&) = delete; 
    Publisher(Publisher&& ) = delete; 
    Publisher& operator=(Publisher const&) = delete; 
    Publisher& operator=(Publisher&&) = delete; 

    template <Event::Type type> requires MessageWithContent<type>
    void RegisterListener(typename Event::Message<type>::CallbackTrait callback);

    template <Event::Type type> requires MessageWithoutContent<type>
    void RegisterListener(typename Event::Message<type>::CallbackTrait callback);

    template <Event::Type type> requires MessageWithContent<type>
    void RegisterEvent(typename Event::Message<type>::EventContent&& content);

    template <Event::Type type> requires MessageWithoutContent<type>
    void RegisterEvent();
    
    std::size_t EventCount() const;
    std::size_t ListenerCount() const;

    std::size_t PublishEvents();

private:
    using EventProxy = std::unique_ptr<Event::IMessage>;
    using EventListenerProxy = std::function<void(EventProxy const& upEventProxy)>;
    using EventListeners = std::unordered_map<Event::Type, std::vector<EventListenerProxy>>;
    using EventQueue = std::deque<EventProxy>;

    mutable std::mutex m_listenersMutex;
    EventListeners m_listeners;
    
    mutable std::mutex m_eventsMutex;
    EventQueue m_events;
};

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type type> requires Event::MessageWithContent<type>
void Event::Publisher::RegisterListener(typename Event::Message<type>::CallbackTrait callback)
{
    // In the case of events with content, the listener will need to cast to the derived event type to access the 
    // event's content and then forward them to the supplied handler. 
    std::scoped_lock lock(m_listenersMutex);
    m_listeners[type].emplace_back(
        [callback] (EventProxy const& upEventProxy) {
            auto const* const pEvent = dynamic_cast<Event::Message<type> const* const>(upEventProxy.get());
            assert(pEvent); // PublishEvents needs to ensure this callback is provided the correct event. 
            std::apply(callback, pEvent->GetContent());
        });
}

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type type> requires Event::MessageWithoutContent<type>
void Event::Publisher::RegisterListener(typename Event::Message<type>::CallbackTrait callback)
{
    // In the case of events without content, the listener will simply invoke the callback without needing to care 
    // about the event details. 
    std::scoped_lock lock(m_listenersMutex);
    m_listeners[type].emplace_back(
        [callback] ([[maybe_unused]] EventProxy const&) { std::invoke(callback); });
}

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type type> requires Event::MessageWithContent<type>
void Event::Publisher::RegisterEvent(typename Event::Message<type>::EventContent&& content)
{
    // If there are no listeners for the specified event, there is point in adding it to the queue. All event listeners
    // should be registered before starting the node. 
    {
        std::scoped_lock lock(m_listenersMutex);
        if (!m_listeners.contains(type)) { return; }
    }

    // In the case of events with content, we need to forward the event's content to when creating the queued event. 
    EventProxy upEventProxy = std::make_unique<Event::Message<type>>(std::move(content));
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(std::move(upEventProxy));
    }
}

//----------------------------------------------------------------------------------------------------------------------

template <Event::Type type> requires Event::MessageWithoutContent<type>
void Event::Publisher::RegisterEvent()
{
    // If there are no listeners for the specified event, there is point in adding it to the queue. All event listeners
    // should be registered before starting the node. 
    {
        std::scoped_lock lock(m_listenersMutex);
        if (!m_listeners.contains(type)) { return; }
    }

    // In the case of events with content, we simply need to create the event to queue it. 
    EventProxy upEventProxy = std::make_unique<Event::Message<type>>();
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(std::move(upEventProxy));
    }
}

//----------------------------------------------------------------------------------------------------------------------
