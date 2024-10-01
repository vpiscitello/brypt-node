//----------------------------------------------------------------------------------------------------------------------
// File: Field.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <concepts>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Configuration {
//----------------------------------------------------------------------------------------------------------------------

template<typename T>
concept FieldNameTag = requires(T t)
{
    { static_cast<std::string_view>(t) } -> std::same_as<std::string_view>;
};

template <std::size_t SourceSize>
constexpr std::size_t GetSnakeCaseSize(const char (&source)[SourceSize])
{
    std::size_t size = SourceSize > 0 ? 1 : 0;

    for (std::size_t idx = 1; idx < SourceSize; ++idx) {
        if (source[idx] >= 'A' && source[idx] <= 'Z' && source[idx - 1] >= 'a' && source[idx - 1] <= 'z') {
            ++size;
        }
        ++size;
    }
    return size;
}

template <std::size_t DestinationSize, std::size_t SourceSize>
constexpr auto ConvertToSnakeCase(const char (&source)[SourceSize])
{
    std::array<char, DestinationSize> converted{};

    std::size_t idx = 0;
    for (std::size_t jdx = 0; jdx < SourceSize - 1; ++jdx) {
        if (source[jdx] >= 'A' && source[jdx] <= 'Z') {
            if (jdx > 0 && source[jdx - 1] >= 'a' && source[jdx - 1] <= 'z') { converted[idx++] = '_'; }
            converted[idx++] = source[jdx] - 'A' + 'a';
        } else {
            converted[idx++] = source[jdx];
        }
    }

    converted[idx] = '\0';
    return converted;
}

// Define a macro to create field name tags
#define DEFINE_FIELD_NAME(name) \
    struct name { \
        static constexpr auto FieldName = ConvertToSnakeCase<GetSnakeCaseSize(#name)>(#name); \
        static constexpr std::string_view GetFieldName() { return FieldName.data(); } \
        constexpr operator std::string_view() { return FieldName.data(); } \
    }

template<typename T>
concept IsOptionalField = std::is_same_v<std::remove_cvref_t<T>, std::nullopt_t> ||
                          std::is_same_v<std::remove_cvref_t<T>, std::optional<typename T::value_type>>;

template<typename T> 
struct DecayedValueType { using Type = T; };

template<typename T>
struct DecayedValueType<std::optional<T>> { using Type = T; };

template<FieldNameTag ProvidedNameTag, typename ValueType>
class Field
{
public:
    using FieldType = DecayedValueType<std::decay_t<ValueType>>::Type;
    using Validator = std::function<bool(FieldType const&)>;

    static constexpr std::string_view FieldName = ProvidedNameTag{};
    static constexpr auto DefaultValidator = [] (FieldType const&) { return true; };

    Field(Validator const& validator = DefaultValidator)
        : m_modified(false)
        , m_value()
        , m_validator(validator)
    {
    }

    template<typename T>
        requires std::is_same_v<std::remove_cvref_t<T>, FieldType> || 
                (std::is_same_v<ValueType, std::optional<FieldType>> && std::is_same_v<std::remove_cvref_t<T>, std::optional<FieldType>>)
    explicit Field(T&& value, Validator const& validator = DefaultValidator) 
        : m_modified(false)
        , m_value(std::forward<T>(value))
        , m_validator(validator)
    {
    }
    explicit Field(std::string_view value, Validator const& validator = DefaultValidator)
        requires std::is_same_v<FieldType, std::string>
        : m_modified(false)
        , m_value(value)
        , m_validator(validator)
    {
    }

    virtual ~Field() = default;

    [[nodiscard]] bool operator==(Field const& other) const noexcept
    {
        return m_value == other.m_value;
    }

    [[nodiscard]] std::strong_ordering operator<=>(Field const& other) const noexcept
    {
        if (auto const result = m_modified <=> other.m_modified; result != std::strong_ordering::equal) { return result; }
        if (auto const result = m_value <=> other.m_value; result != std::strong_ordering::equal) { return result; }
        return std::strong_ordering::equal;
    }

    [[nodiscard]] static constexpr std::string_view GetFieldName() { return FieldName; }
    
    [[nodiscard]] static constexpr bool IsOptional()
    { 
        return IsOptionalField<ValueType>;
    }

    [[nodiscard]] FieldType const& GetValue() const
    {
        constexpr bool isOptional = IsOptional();
        if constexpr (isOptional) {
            return *m_value;
        } else {
            return m_value;
        }
    }

    [[nodiscard]] bool HasValue() const
    {
        constexpr bool isOptional = IsOptional();
        constexpr bool isStringField = std::is_same_v<FieldType, std::string>;
        if constexpr (isOptional && isStringField) {
            if (m_value.has_value()) {
                return !m_value->empty();
            }
            return false;
        } else if constexpr (isOptional){
            return m_value.has_value();
        } else if constexpr (isStringField){
            return !m_value.empty();
        } else {
            true;
        }
    }
    
    [[nodiscard]] bool WouldMatchDefault(FieldType const& defaultValue) const
    {
        constexpr bool isOptional = IsOptional();
        if constexpr (isOptional) {
            if (m_value.has_value()) {
                return *m_value == defaultValue;
            }
            return true;
        } else {
            m_value == defaultValue;
        }
    }
    
    [[nodiscard]] bool WouldMatchDefault(std::string_view defaultValue) const
        requires std::is_same_v<FieldType, std::string>
    {
        constexpr bool isOptional = IsOptional();
        if constexpr (isOptional) {
            if (m_value.has_value()) {
                return *m_value == defaultValue;
            }
            return true;
        } else {
            m_value == defaultValue;
        }
    }
     
    [[nodiscard]] bool Modified() const { return m_modified; }
    [[nodiscard]] bool NotModified() const { return !m_modified; }

    [[nodiscard]] virtual bool SetValue(FieldType const& value)
    {
        if (value == m_value) { return true; }
        if (!m_validator(value)) { return false; }
        m_value = value;
        m_modified = true;
        return true;
    }

    [[nodiscard]] bool SetValue(std::string_view value)
        requires std::is_same_v<FieldType, std::string>
    {
        return SetValue(std::string{ value });
    }

    [[nodiscard]] virtual bool SetValueFromConfig(FieldType const& value)
    {
        if (value == m_value) { return true; }
        if (!m_validator(value)) { return false; }
        m_value = value;
        return true;
    }

    [[nodiscard]] bool SetValueFromConfig(std::string_view value)
        requires std::is_same_v<FieldType, std::string>
    {
        return SetValueFromConfig(std::string{ value });
    }
     
    void ClearModifiedFlag() { m_modified = false; }

protected:
    bool m_modified;
    ValueType m_value;
    Validator m_validator;
};

//----------------------------------------------------------------------------------------------------------------------

template<FieldNameTag ProvidedNameTag, typename ValueType>
class OptionalField : public Field<ProvidedNameTag, std::optional<ValueType>>
{
public:
    using Validator = Field<ProvidedNameTag, std::optional<ValueType>>::Validator;

    using Field<ProvidedNameTag, std::optional<ValueType>>::DefaultValidator;
    using Field<ProvidedNameTag, std::optional<ValueType>>::IsOptional;
    using Field<ProvidedNameTag, std::optional<ValueType>>::m_modified;
    using Field<ProvidedNameTag, std::optional<ValueType>>::m_value;

    OptionalField(Validator const& validator = DefaultValidator)
        : Field<ProvidedNameTag, std::optional<ValueType>>(validator)
    {
    }
    
    template<typename T> 
        requires std::is_same_v<std::remove_cvref_t<T>, ValueType> || 
                 std::is_same_v<std::remove_cvref_t<T>, std::optional<ValueType>>
    explicit OptionalField(T&& value, Validator const& validator = DefaultValidator)
        : Field<ProvidedNameTag, std::optional<ValueType>>(std::forward<T>(value), validator)
    {
    }

    explicit OptionalField(std::string_view value, Validator const& validator = DefaultValidator)
        requires std::is_same_v<ValueType, std::string>
        : Field<ProvidedNameTag, std::optional<ValueType>>(value, validator)
    {
    }

    virtual ~OptionalField() = default;

    [[nodiscard]] bool operator==(OptionalField const& other) const noexcept
    {
        return m_value == other.m_value;
    }

    [[nodiscard]] std::strong_ordering operator<=>(OptionalField const& other) const noexcept
    {
        if (auto const result = m_modified <=> other.m_modified; result != std::strong_ordering::equal) { return result; }
        if (auto const result = m_value <=> other.m_value; result != std::strong_ordering::equal) { return result; }
        return std::strong_ordering::equal;
    }
    
    [[nodiscard]] std::optional<ValueType> const& GetInternalOptionalValueMember() const
    {
        return m_value;
    }

    [[nodiscard]] ValueType const& GetValueOrElse(ValueType const& defaultValue) const
    {
        return (m_value.has_value()) ? m_value.value() : defaultValue;
    }

    void ResetValue()
    {
        if (m_value.empty()) { return; }
        m_value.reset();
        m_modified = true;
    }
};

//----------------------------------------------------------------------------------------------------------------------

template<FieldNameTag ProvidedNameTag, typename ConstructedType>
class ConstructedField : public Field<ProvidedNameTag, ConstructedType>
{
public:
    using FieldType = Field<ProvidedNameTag, ConstructedType>::FieldType;

    using ConverterTo = std::function<std::optional<FieldType>(std::string_view value)>;
    using ConverterFrom = std::function<std::optional<std::string>(FieldType const& value)>;
    using Validator = std::function<bool(FieldType const& value)>;

    using Field<ProvidedNameTag, ConstructedType>::DefaultValidator;
    using Field<ProvidedNameTag, ConstructedType>::IsOptional;
    using Field<ProvidedNameTag, ConstructedType>::m_modified;
    using Field<ProvidedNameTag, ConstructedType>::m_value;
    using Field<ProvidedNameTag, ConstructedType>::m_validator;

    ConstructedField(
        ConverterTo const& converterTo,
        ConverterFrom const& converterFrom,
        Validator const& validator = DefaultValidator)
        : Field<ProvidedNameTag, ConstructedType>(validator)
        , m_convertTo(converterTo)
        , m_convertFrom(converterFrom)
        , m_serialized()
    {
    }

    template<typename T>
        requires std::is_same_v<std::remove_cvref_t<T>, FieldType> || 
                 (std::is_same_v<ConstructedType, std::optional<FieldType>> && std::is_same_v<std::remove_cvref_t<T>, std::optional<FieldType>>)
    ConstructedField(
        T&& value,
        ConverterTo const& converterTo,
        ConverterFrom const& converterFrom,
        Validator const& validator = DefaultValidator) 
        : Field<ProvidedNameTag, ConstructedType>(validator)
        , m_convertTo(converterTo)
        , m_convertFrom(converterFrom)
        , m_serialized()
    {
        m_value = std::forward<T>(value);

        constexpr bool isOptional = IsOptional();
        if constexpr (isOptional) {
            if (m_value.has_value()) {
                auto optSerialized = m_convertFrom(m_value.value());
                if (!optSerialized) {
                    throw std::runtime_error("Failed to convert internal type to config value on field construction!");
                }
                m_serialized = std::move(*optSerialized);
            }
        } else {
            auto optSerialized = m_convertFrom(m_value);
            if (!optSerialized) {
                throw std::runtime_error("Failed to convert internal type to config value on field construction!");
            }
            m_serialized = std::move(*optSerialized);
        }
    }

    ConstructedField(
        std::string_view serialized,
        ConverterTo const& converterTo,
        ConverterFrom const& converterFrom,
        Validator const& validator = DefaultValidator)
        : Field<ProvidedNameTag, ConstructedType>(validator)
        , m_convertTo(converterTo)
        , m_convertFrom(converterFrom)
        , m_serialized(serialized)
    {
        auto optValue = m_convertTo(serialized);
        if (!optValue) {
            throw std::runtime_error("Failed to convert config value to internal type on field construction!");
        }
        m_value = std::move(*optValue);
    }

    virtual ~ConstructedField() = default;

    [[nodiscard]] bool operator==(ConstructedField const& other) const noexcept
    {
        return m_value == other.m_value;
    }

    [[nodiscard]] std::strong_ordering operator<=>(ConstructedField const& other) const noexcept
    {
        if (auto const result = m_modified <=> other.m_modified; result != std::strong_ordering::equal) { return result; }
        if (auto const result = m_value <=> other.m_value; result != std::strong_ordering::equal) { return result; }
        return std::strong_ordering::equal;
    }

    [[nodiscard]] std::string const& GetSerializedValue() const { return m_serialized; }

    [[nodiscard]] virtual bool SetValue(FieldType const& value) override
    {
        if (value == m_value) { return true; }
        if (!m_validator(value)) { return false; }
        auto const optSerialized = m_convertFrom(value);
        if (!optSerialized) { return false; }
        m_serialized = std::move(*optSerialized);
        m_value = value;
        m_modified = true;
        return true;
    }

    [[nodiscard]] bool SetValue(std::string_view serialized)
    {
        // We process setting an empty serialized value because it may have special meaning. The convertTo method 
        // may choose to do different handling in the empty case. 
        if (!serialized.empty() && serialized == m_serialized) { return true; }
        auto const optValue = m_convertTo(serialized);
        if (!optValue || !m_validator(*optValue)) { return false; }
        if (m_serialized.empty()) {
            auto const optSerialized = m_convertFrom(*optValue);
            if (!optSerialized) { return false; }
            m_serialized = std::move(*optSerialized);
        } else {
            m_serialized = serialized;
        }
        m_value = std::move(*optValue);
        m_modified = true;
        return true;
    }

    [[nodiscard]] virtual bool SetValueFromConfig(FieldType const& value) override
    {
        if (value == m_value) { return true; }
        if (!m_validator(value)) { return false; }
        auto const optSerialized = m_convertFrom(value);
        if (!optSerialized) { return false; }
        m_serialized = std::move(*optSerialized);
        m_value = value;
        return true;
    }

    [[nodiscard]] bool SetValueFromConfig(std::string_view serialized)
    {
        if (serialized == m_serialized) { return true; }
        auto const optValue = m_convertTo(serialized);
        if (!optValue || !m_validator(*optValue)) { return false; }
        m_serialized = serialized;
        m_value = std::move(*optValue);
        return true;
    }

protected:
    ConverterTo m_convertTo;
    ConverterFrom m_convertFrom;
    std::string m_serialized;
};

//----------------------------------------------------------------------------------------------------------------------

template<FieldNameTag ProvidedNameTag, typename ConstructedType>
class OptionalConstructedField : public ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>
{
public:
    using FieldType = ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>::FieldType;

    using ConverterTo = ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>::ConverterTo;
    using ConverterFrom = ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>::ConverterFrom;
    using Validator = ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>::Validator;

    using Field<ProvidedNameTag, std::optional<ConstructedType>>::DefaultValidator;
    using Field<ProvidedNameTag, std::optional<ConstructedType>>::m_modified;
    using Field<ProvidedNameTag, std::optional<ConstructedType>>::m_value;
    using ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>::m_convertTo;
    using ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>::m_serialized;

    OptionalConstructedField(
        ConverterTo const& converterTo,
        ConverterFrom const& converterFrom,
        Validator const& validator = DefaultValidator)
        : ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>(converterTo, converterFrom, validator)
    {
    }

    template<typename T> 
        requires std::is_same_v<std::remove_cvref_t<T>, ConstructedType> || 
                 std::is_same_v<std::remove_cvref_t<T>, std::optional<ConstructedType>>
    OptionalConstructedField(
        T&& value,
        ConverterTo const& converterTo,
        ConverterFrom const& converterFrom,
        Validator const& validator = DefaultValidator)
        : ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>(std::forward<T>(value), converterTo, converterFrom, validator)
    {
    }

    OptionalConstructedField(
        std::string_view serialized,
        ConverterTo const& converterTo,
        ConverterFrom const& converterFrom,
        Validator const& validator = DefaultValidator)
        : ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>(serialized, converterTo, converterFrom, validator)
    {
    }


    OptionalConstructedField(
        std::optional<std::string> const& optSerialized,
        ConverterTo const& converterTo,
        ConverterFrom const& converterFrom,
        Validator const& validator = DefaultValidator)
        : ConstructedField<ProvidedNameTag, std::optional<ConstructedType>>(converterTo, converterFrom, validator)
    {
        if (optSerialized) {
            m_serialized = *optSerialized;
            auto optValue = m_convertTo(m_serialized);
            if (!optValue) {
                throw std::runtime_error("Failed to convert config value to internal type on field construction!");
            }
            m_value = std::move(*optValue);
        }
    }

    virtual ~OptionalConstructedField() = default;

    [[nodiscard]] bool operator==(OptionalConstructedField const& other) const noexcept
    {
        return m_value == other.m_value;
    }

    [[nodiscard]] std::strong_ordering operator<=>(OptionalConstructedField const& other) const noexcept
    {
        if (auto const result = m_modified <=> other.m_modified; result != std::strong_ordering::equal) { return result; }
        if (auto const result = m_value <=> other.m_value; result != std::strong_ordering::equal) { return result; }
        return std::strong_ordering::equal;
    }

    [[nodiscard]] std::optional<ConstructedType> const& GetInternalOptionalValueMember() const
    {
        return m_value;
    }

    [[nodiscard]] ConstructedType const& GetValueOrElse(ConstructedType const& defaultValue) const
    {
        return (m_value.has_value()) ? m_value.value() : defaultValue;
    }

    void ResetValue()
    {
        if (m_value.empty()) { return; }
        m_value.reset();
        m_modified = true;
    }
};

//----------------------------------------------------------------------------------------------------------------------
} // Configuration namespace
//----------------------------------------------------------------------------------------------------------------------
