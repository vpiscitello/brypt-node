//----------------------------------------------------------------------------------------------------------------------
// Description: C++ language bindings for the Brypt Shared Library C Interface.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <any>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt {
//----------------------------------------------------------------------------------------------------------------------

enum class success_code : brypt_status_t {
    accepted = BRYPT_ACCEPTED
};

enum class error_code : brypt_status_t {
    unspecified = BRYPT_EUNSPECIFIED,
    access_denied = BRYPT_EACCESSDENIED,
    invalid_argument = BRYPT_EINVALIDARGUMENT,
    operation_not_supported = BRYPT_EOPERNOTSUPPORTED,
    operation_timeout = BRYPT_EOPERTIMEOUT,
    initialization_failure = BRYPT_EINITNFAILURE,
    already_started = BRYPT_EALREADYSTARTED,
    not_started = BRYPT_ENOTSTARTED,
    shutdown_failure = BRYPT_ESHUTDOWNFAILURE,
    file_not_found = BRYPT_EFILENOTFOUND,
    file_not_supported = BRYPT_EFILENOTSUPPORTED,
    missing_parameter = BRYPT_EMISSINGPARAMETER,
    network_binding_failed = BRYPT_ENETBINDFAILED,
    network_connection_failed = BRYPT_ENETCONNNFAILED,
};


class status;
class option;
class service;

//----------------------------------------------------------------------------------------------------------------------
// Concepts {
//----------------------------------------------------------------------------------------------------------------------
template<typename CodeType>
concept status_code = std::is_same_v<CodeType, success_code> || std::is_same_v<CodeType, error_code>;

// Note: Supported primitive and string types are specified seperatley to allow the std::string_view constructor. 
// Otherwise, std::string would be taken by reference instead of copy and char const*/char const[] would not be allowed.
template<typename ValueType>
concept suported_primitive_type = std::is_same_v<ValueType, bool> || std::is_same_v<ValueType, int32_t>;

template<typename ValueType>
concept supported_string_type = std::is_same_v<ValueType, std::string>;

template<typename ValueType>
concept supported_option_type = suported_primitive_type<ValueType> || supported_string_type<ValueType>;
//----------------------------------------------------------------------------------------------------------------------
// } Concepts 
//----------------------------------------------------------------------------------------------------------------------
} // brypt namespace
//----------------------------------------------------------------------------------------------------------------------

class brypt::status : public std::runtime_error
{
public:
    status() noexcept
        : std::runtime_error("")
        , m_status(static_cast<brypt_status_t>(success_code::accepted))
    {
    }

    explicit status(brypt_status_t status) noexcept
        : std::runtime_error(brypt_error_description(status))
        , m_status(status)
    {
    }

    template<status_code CodeType>
    explicit status(CodeType status) noexcept
        : std::runtime_error(brypt_error_description(static_cast<brypt_status_t>(status)))
        , m_status(static_cast<brypt_status_t>(status))
    {
    }
    
    bool operator==(status const& other) const noexcept
    {
        return m_status == other.m_status;
    }

    template<status_code CodeType>
    bool operator==(CodeType other) const noexcept
    {
        return m_status == static_cast<brypt_status_t>(other);
    }

    bool operator!=(status const& other) const noexcept
    {
        return !operator==(other);
    }

    template<status_code CodeType>
    bool operator!=(CodeType other) const noexcept
    {
        return m_status != static_cast<brypt_status_t>(other);
    }

    [[nodiscard]] virtual char const* what() const noexcept override
    {
        return std::runtime_error::what();
    }

    [[nodiscard]] bool succeeded() const noexcept
    {
        return m_status == BRYPT_ACCEPTED;
    }

    [[nodiscard]] bool failed() const noexcept
    {
        return m_status != BRYPT_ACCEPTED;
    }

    [[nodiscard]] brypt_status_t value() const noexcept
    {
        return m_status;
    }

private:
    brypt_status_t m_status;
};

//----------------------------------------------------------------------------------------------------------------------

class brypt::option
{
public:
    enum service_types : brypt_option_t {
        use_bootstraps = BRYPT_OPT_USE_BOOTSTRAPS,
    };

    using use_bootstraps_t = bool;

    enum configuration_types : brypt_option_t {
        base_path = BRYPT_OPT_BASE_FILEPATH,
        configuration_filename = BRYPT_OPT_CONFIGURATION_FILENAME,
        peers_filename = BRYPT_OPT_PEERS_FILENAME,
    };

    using base_path_t = std::string;
    using peers_filename_t = std::string;
    using configuration_filename_t = std::string;

    option()
        : m_name(0)
        , m_value()
    {
    }

    option(brypt_option_t name, std::string_view value)
        : m_name(name)
        , m_value(std::string(value))
    {
        switch (name) {
            case base_path:
            case configuration_filename:
            case peers_filename: break;
            default: throw status(error_code::invalid_argument);
        }
    }

    option(brypt_option_t name, std::string_view value, status& result) noexcept
        : m_name(name)
        , m_value(std::string(value))
    {
        switch (name) {
            case base_path:
            case configuration_filename:
            case peers_filename: break;
            default: { handle_invalid_value(result); return; }
        }
        result = {};
    }

    template<suported_primitive_type ValueType>
    option(brypt_option_t name, ValueType value)
        : m_name(name)
        , m_value(value)
    {
        switch (name) {
            case use_bootstraps: {
                if (typeid(ValueType) != typeid(bool)) { throw status(error_code::invalid_argument); }
            } break;
            default: throw status(error_code::invalid_argument);
        }
    }

    template<suported_primitive_type ValueType>
    option(brypt_option_t name, ValueType value, status& result) noexcept
        : m_name(name)
        , m_value(value)
    {
        switch (name) {
            case use_bootstraps: {
                if (typeid(ValueType) != typeid(bool)) { handle_invalid_value(result); return; }
            } break;
            default: { handle_invalid_value(result); return; }
        }
        result = {};
    }
    

    [[nodiscard]] brypt_option_t name() const noexcept
    {
        return m_name;
    }

    [[nodiscard]] auto const& type() const noexcept
    {
        return m_value.type();
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return m_value.has_value();
    }

    template<supported_option_type ValueType>
    [[nodiscard]] bool is() const noexcept
    {
        return m_value.type() == typeid(ValueType);
    }

    template<suported_primitive_type ValueType>
    [[nodiscard]] std::optional<ValueType> value() const
    {
        try { return std::any_cast<ValueType>(m_value); }
        catch(...) { return {}; }
    }

    template<supported_string_type ValueType>
    [[nodiscard]] std::optional<std::reference_wrapper<ValueType const>> value() const
    {
        try { return std::any_cast<ValueType const&>(m_value); }
        catch(...) { return {}; }
    }

    template<supported_option_type ValueType>
    [[nodiscard]] status get(ValueType& value) const
    {
        try { value = std::any_cast<ValueType>(m_value); }
        catch(...) { return status(error_code::invalid_argument); }
        return {};
    }

private:
    void handle_invalid_value(status& result)
    {
        m_value.reset();
        result = status(error_code::invalid_argument);
    }

    brypt_option_t m_name;
    std::any m_value;
};

//----------------------------------------------------------------------------------------------------------------------

class brypt::service
{
public:
    explicit service(std::string_view base_path)
        : m_service(
            brypt_service_create(base_path.data(), base_path.size()), &brypt_service_destroy)
    {
        if (!m_service) { throw status(error_code::initialization_failure); }
    }

    service(std::string_view base_path, status& result)
        : m_service(
            brypt_service_create(base_path.data(), base_path.size()), &brypt_service_destroy)
    {
        if (!m_service) {
            result = status(error_code::initialization_failure);
            return;
        }
        result = {};
    }

    ~service()
    {
        if (m_service) { [[maybe_unused]] auto const result = shutdown(); }
    }

    service(service const& other) = delete;
    service& operator=(service const& other) = delete;

    service(service&& other) = default;
    service& operator=(service&& other) = default;

    [[nodiscard]] option get_option(brypt_option_t type) const noexcept
    {
        if (!m_service) { throw status(error_code::operation_not_supported); }

        switch (type) {
            case option::base_path:
            case option::configuration_filename:
            case option::peers_filename: {
                return option(type, brypt_option_get_str(m_service.get(), type));
            }
            case option::use_bootstraps: {
                return option(type, brypt_option_get_bool(m_service.get(), type));
            }
            default: throw status(error_code::invalid_argument);
        }
    }

    [[nodiscard]] status set_option(option const& opt) noexcept
    {
        if (!m_service) { return status(error_code::operation_not_supported); }

        switch (opt.name()) {
            case option::base_path:
            case option::configuration_filename:
            case option::peers_filename: {
                auto const optValue = opt.value<std::string>();
                if (optValue) {
                    auto const& value = optValue.value().get();
                    return status(brypt_option_set_str(m_service.get(), opt.name(), value.data(), value.size()));
                }
            } break;
            case option::use_bootstraps: {
                auto const optValue = opt.value<bool>();
                if (optValue) {
                    return status(brypt_option_set_bool(m_service.get(), opt.name(), *optValue));
                }
            } break;
            default: break;
        }

        return status(error_code::invalid_argument);
    }
    
    [[nodiscard]] status startup() noexcept
    {
        return status(brypt_service_start(m_service.get()));
    }

    [[nodiscard]] status shutdown() noexcept
    {
        return status(brypt_service_stop(m_service.get()));
    }

private:
    using unique_service_t = std::unique_ptr<brypt_service_t, decltype(&brypt_service_destroy)>;
    unique_service_t m_service;
};
