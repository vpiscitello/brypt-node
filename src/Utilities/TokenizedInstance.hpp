//----------------------------------------------------------------------------------------------------------------------
// File: TokenizedInstance.hpp
// Description: A helper class to expose a static factory member for classes inheriting std::enable_shared_from_this. 
// If an instance of that class is held outside of instantiated std::shared_ptr, it may lead to undefined behavior when
// shared_from_this() is invoked. 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <concepts>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

template<typename SharedType>
    requires std::is_class_v<SharedType>
class TokenizedInstance
{
public:
    template<typename... Arguments>
    static std::shared_ptr<SharedType> CreateInstance(Arguments&&... arguments)
    {
        return std::make_shared<SharedType>(InstanceToken{}, std::forward<Arguments>(arguments)...);
    }

protected:
    struct InstanceToken {
    private:
        constexpr InstanceToken() noexcept = default;
#if defined(WIN32)
        // On Windows, we can't scope the InstanceToken to the CreateInstance method. But, we can still cause UNIX
        // builds to fail if it's misused. 
        friend class TokenizedInstance;
#else
        template<typename... Arguments>
        friend std::shared_ptr<SharedType> TokenizedInstance::CreateInstance(Arguments&&... arguments);
#endif
    };
};

//----------------------------------------------------------------------------------------------------------------------
