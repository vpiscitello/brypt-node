//----------------------------------------------------------------------------------------------------------------------
// File: ServiceProvider.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <any>
#include <memory>
#include <typeindex>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Node {
//----------------------------------------------------------------------------------------------------------------------

class ServiceProvider;

//----------------------------------------------------------------------------------------------------------------------
} // Node namespace
//----------------------------------------------------------------------------------------------------------------------

class Node::ServiceProvider final 
{
public:
    ServiceProvider() = default;

    template<typename Service>
    bool Register(std::shared_ptr<Service> const& spService);

    template<typename Service>
    [[nodiscard]] bool Contains() const;

    template<typename Service>
    [[nodiscard]] std::weak_ptr<Service> Fetch() const;

private:
    std::unordered_map<std::type_index, std::any> m_services;
};

//----------------------------------------------------------------------------------------------------------------------

template<typename Service>
bool Node::ServiceProvider::Register(std::shared_ptr<Service> const& spService)
{
    auto const [itr, result] = m_services.insert_or_assign(typeid(Service), std::weak_ptr<Service>{ spService });
    assert(itr != m_services.end());
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename Service>
bool Node::ServiceProvider::Contains() const { return m_services.contains(typeid(Service)); }

//----------------------------------------------------------------------------------------------------------------------

template<typename Service>
std::weak_ptr<Service> Node::ServiceProvider::Fetch() const
{
    if (auto const itr = m_services.find(typeid(Service)); itr != m_services.end()) {
        auto const& [key, store] = *itr;
        return std::any_cast<std::weak_ptr<Service>>(store);
    }

    return std::weak_ptr<Service>{};
}

//----------------------------------------------------------------------------------------------------------------------
