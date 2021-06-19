//----------------------------------------------------------------------------------------------------------------------
// File: InvokeContext.hpp
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <concepts>
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

enum class InvokeContext : std::uint32_t { Production, Test };

template<InvokeContext ContextType>
concept TestingContext = requires { ContextType == InvokeContext::Test; };

#define UT_SupportMethod(declaration) \
template <InvokeContext ContextType = InvokeContext::Production> requires TestingContext<ContextType> \
declaration

//----------------------------------------------------------------------------------------------------------------------
