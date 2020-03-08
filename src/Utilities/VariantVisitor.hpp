//------------------------------------------------------------------------------------------------
// File: VariantVisitor.hpp
// Description: Template helper class for defining overloads for visiting variant types.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------

template<typename... Types>
struct TVariantVisitor : Types... 
{ 
    using Types::operator()...;
};
template<typename... Types> TVariantVisitor(Types...) -> TVariantVisitor<Types...>;

//------------------------------------------------------------------------------------------------