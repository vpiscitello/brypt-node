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
#include "Utilities/Assertions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <concepts>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

namespace Scheduler { class Delegate; class Registrar; }

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

    explicit Publisher(std::shared_ptr<Scheduler::Registrar> const& spRegistrar);
    ~Publisher();

    Publisher(Publisher const&) = delete; 
    Publisher(Publisher&& ) = delete; 
    Publisher& operator=(Publisher const&) = delete; 
    Publisher& operator=(Publisher&&) = delete; 

    template<Type SpecificType> requires MessageWithContent<SpecificType>
    bool Subscribe(typename Event::Message<SpecificType>::CallbackTrait const& callback)
    {
        if (m_hasSuspendedSubscriptions) { return false; } // Subscriptions are disabled when the event loop begins. 
        assert(Assertions::Threading::IsCoreThread());

        // In the case of events with content, the listener will need to cast to the derived event type to access the 
        // event's content and then forward them to the supplied handler. 
        m_listeners[SpecificType].emplace_back([callback](EventProxy const& upEventProxy) {
            // Dispatch needs to ensure this callback is provided the correct event. 
            assert(upEventProxy && upEventProxy->GetType() == SpecificType);
            auto const pEvent = static_cast<Event::Message<SpecificType> const*>(upEventProxy.get());
            std::apply(callback, pEvent->GetContent());
            });
        return true;
    }

    template<Type SpecificType> requires MessageWithoutContent<SpecificType>
    bool Subscribe(typename Event::Message<SpecificType>::CallbackTrait const& callback)
    {
        if (m_hasSuspendedSubscriptions) { return false; } // Subscriptions are disabled when the event loop begins. 
        assert(Assertions::Threading::IsCoreThread());

        // In the case of events without content, the listener will simply invoke the callback.
        m_listeners[SpecificType].emplace_back([callback](EventProxy const&) { std::invoke(callback); });
        return true;
    }

    void SuspendSubscriptions();
    
    void Advertise(Type type);
    void Advertise(EventAdvertisements&& advertised);

    template<Type SpecificType, typename... Arguments> requires MessageWithContent<SpecificType>
    void Publish(Arguments&&... arguments)
    {
        // In the case of events with content, we can to forward the event's content to when creating the queued event. 
        Publish(SpecificType, std::make_unique<Event::Message<SpecificType>>(std::forward<Arguments>(arguments)...));
    }

    template<Type SpecificType> requires MessageWithoutContent<SpecificType>
    void Publish()
    {
        // In the case of events with content, we simply need to create the event to queue it. 
        Publish(SpecificType, std::make_unique<Event::Message<SpecificType>>());
    }

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

    void Publish(Type type, EventProxy&& upEventProxy);

    std::shared_ptr<Scheduler::Delegate> m_spDelegate;
    std::atomic_bool m_hasSuspendedSubscriptions;
    Listeners m_listeners;
    
    mutable std::mutex m_eventsMutex;
    EventQueue m_events;

    mutable std::shared_mutex m_advertisedMutex;
    EventAdvertisements m_advertised;
};

//----------------------------------------------------------------------------------------------------------------------
