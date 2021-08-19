//----------------------------------------------------------------------------------------------------------------------
// File: Service.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Service.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <deque>
#include <ranges>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

Scheduler::Sentinel::Sentinel()
    : m_mutex()
    , m_waiter()
    , m_available(0)
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Scheduler::Sentinel::AwaitTask(std::chrono::milliseconds timeout)
{
    if (m_available) { return false; } // If there are ready tasks, there is no need to wait. 
    std::unique_lock lock(m_mutex);
    m_waiter.wait_for(lock, timeout);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Scheduler::Sentinel::AvailableTasks() const { return m_available; }

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::Sentinel::OnTaskAvailable(std::size_t available)
{
    // Increment the count of available work. If this is the first notification of work we've had recently, try 
    // waking the runtime thread early in order to process the work as soon as possible. 
    if (auto const result = m_available += available; result == 1) {
        m_waiter.notify_one();
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::Sentinel::OnTaskCompleted(std::size_t completed) { m_available -= completed; }

//----------------------------------------------------------------------------------------------------------------------

Scheduler::Service::Service()
    : m_logger(spdlog::get(LogUtils::Name::Core.data()))
    , m_delegates()
    , m_initialized(false)
{
    assert(Assertions::Threading::IsCoreThread());
    assert(m_logger);
}

//----------------------------------------------------------------------------------------------------------------------

bool Scheduler::Service::Initialize()
{
    assert(Assertions::Threading::IsCoreThread());
    m_initialized = ResolveDependencies() && UpdatePriorityOrder();
    return m_initialized;
}

//----------------------------------------------------------------------------------------------------------------------

void Scheduler::Service::Execute()
{
    assert(Assertions::Threading::IsCoreThread());
    assert(m_initialized);

    constexpr auto ready = [] (auto const& delegate) -> bool
    { 
        return delegate->IsReady();
    };

    std::ranges::for_each(m_delegates | std::views::filter(ready), [this] (auto const& delegate) { 
       std::size_t const executed = delegate->Execute({});
       assert(executed != 0); // The delegate should always indicate at least one task was executed. 
       OnTaskCompleted(executed);
    });
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Scheduler::Delegate> Scheduler::Service::GetDelegate(Delegate::Identifier identifier) const
{
    constexpr auto projection = [] (auto const& delegate) -> auto { return delegate->GetIdentifier(); };
    auto const compare = [&identifier] (Delegate::Identifier const& other) -> bool { return identifier == other; };
    if (auto const itr = std::ranges::find_if(m_delegates, compare, projection); itr != m_delegates.end()) {
        return *itr;
    }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

bool Scheduler::Service::ResolveDependencies()
{
    using RecursiveResolve = std::function<
        bool(std::shared_ptr<Delegate>, Delegate::Dependencies&, Delegate::Dependencies&)>;
    RecursiveResolve const resolve = [&] (auto const& delegate, auto& resolved, auto& unresolved) -> bool
    {
        unresolved.emplace(delegate->GetIdentifier()); // Mark the current delegate as being actively resolved. 
        for (auto const dependency : delegate->GetDependencies()) {
            // If we haven't already resolved the current dependency, integrate the ddependency's chain. 
            if (!resolved.contains(dependency)) {
                 // If the dependency is also being resolved, we've encountered a cyclic dependency chain. 
                if (unresolved.contains(dependency)) { return false; }
                if (!resolve(GetDelegate(dependency), resolved, unresolved)) { return false; }
            }
        }
        resolved.emplace(delegate->GetIdentifier()); // After resolving, add the current delegate to the set. 
        unresolved.erase(delegate->GetIdentifier()); // Unmark the delegate as resolving. 
        return true;
    };

    using ResolvedDependencies = std::unordered_map<Delegate::Identifier, Delegate::Dependencies>;
    ResolvedDependencies store; // A temporary store of each delegate's resolved dependency chain. 
    for (auto const& delegate : m_delegates) {
        auto& resolved = store[delegate->GetIdentifier()]; 
        Delegate::Dependencies unresolved;
        // Start the recursively resolving implicit dependencies, if the resolver returns false and cyclic error
        // has been detected in the current dependency chain. 
        if (!resolve(delegate, resolved, unresolved)) {
            m_logger->critical("Failed to initialize scheduler due a cyclic dependency chain!");
            return false;
        } 
        resolved.erase(delegate->GetIdentifier()); // Remove's the delegate from it's own dependency chain. 
    }

    // Set the resolved dependency chains for each delegate. 
    std::ranges::for_each(m_delegates, [&store] (auto& delegate) {
        auto& dependencies = store[delegate->GetIdentifier()];
        delegate->Depends(std::move(dependencies));
    });

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Scheduler::Service::UpdatePriorityOrder()
{
    using DependentCounts = std::unordered_map<Delegate::Identifier, std::uint32_t>;
    using ReadyDelegates = std::deque<std::shared_ptr<Delegate>>;
    constexpr auto ComputeDependents = [] (auto const& delegates) -> DependentCounts
    {
        DependentCounts dependents;

        // Ensure each delegate has an initial entry with zero dependents. 
        std::ranges::for_each(delegates, [&dependents] (auto const& delegate) {
            dependents[delegate->GetIdentifier()] = 0;
        });

        // Use the delegate's dependency set to compute the dependent counts for each referenced delegate. 
        std::ranges::for_each(delegates, [&dependents] (auto const& delegate) {
            std::ranges::for_each(delegate->GetDependencies(), [&dependents] (auto dependency) { 
                dependents[dependency] += 1;
            });
        });

        return dependents;
    };

    auto dependents = ComputeDependents(m_delegates); // Compute the number of dependents each delegate has. 
    ReadyDelegates ready; // Store for the delegates without remaining dependents. 
    // Determine which delegates have no dependents, these will be the pushed intro the priority set. 
    std::ranges::for_each(dependents, [this, &ready] (auto const& entry) {
        auto const& [identifier, dependents] = entry;
        if (dependents == 0) { ready.emplace_back(GetDelegate(identifier)); }
    });

    Delegates resolved; 
    resolved.reserve(m_delegates.size());
    for (std::size_t idx = 0; idx < m_delegates.size(); ++idx) {
        // If there isn't a delegate without remaining dependents, we have encountered a cyclic dependency chain. 
        // Note: This should never occur given cyclic dependency chains are detected while resolving dependencies 
        if (ready.empty()) { assert(false); return false; }

        auto const& spDelegate = resolved.emplace_back(ready.front());
        spDelegate->SetPriority({}, m_delegates.size() - resolved.size() + 1); 
        ready.pop_front();

        // Decrement the degrees on each dependency that this delegates is dependent on.
        for (auto const dependency : spDelegate->GetDependencies()) {
            if (--dependents[dependency] == 0) { ready.emplace_back(GetDelegate(dependency)); }
        }
    }

    // Replace the registration order with the new priority ordered set. The priority order is reveresed, such that
    // the most most dependent delegates are executed last. 
    std::ranges::swap_ranges(m_delegates, resolved | std::views::reverse);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
