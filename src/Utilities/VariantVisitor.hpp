//----------------------------------------------------------------------------------------------------------------------
// File: VariantVisitor.hpp
// Description: Template helper class for defining overloads for visiting variant types.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------

template<typename... Types>
struct VariantVisitor : Types... 
{ 
    using Types::operator()...;
};
template<typename... Types> VariantVisitor(Types...) -> VariantVisitor<Types...>;

//----------------------------------------------------------------------------------------------------------------------