//----------------------------------------------------------------------------------------------------------------------
// File: Payload.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "DataInterface.hpp"
#include "MessageTypes.hpp"
#include "PackUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <concepts>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Message {
//----------------------------------------------------------------------------------------------------------------------

template<typename Source>
concept BufferStorageType = 
    std::is_same_v<Source, Message::Buffer> ||
    std::is_same_v<Source, std::span<std::uint8_t const>> ||
    std::is_same_v<Source, std::string>;

template<typename Source>
concept SharedStorageType = 
    std::is_same_v<Source, std::shared_ptr<Message::Buffer const>> ||
    std::is_same_v<Source, std::shared_ptr<std::string const>>;

template<typename Source>
concept SupportedStorageType = BufferStorageType<Source> || SharedStorageType<Source>;

template<SupportedStorageType StorageType>
class StorageContainer;

//----------------------------------------------------------------------------------------------------------------------

class StorageInterface : public virtual DataInterface::Viewable, public virtual DataInterface::Packable
{
public:
    ~StorageInterface() = default;
	[[nodiscard]] virtual std::unique_ptr<StorageInterface> Clone() const = 0;
    virtual void ExtractToJsonValue(boost::json::value& target) = 0;
};

//----------------------------------------------------------------------------------------------------------------------

template<BufferStorageType StorageType>
class StorageContainer<StorageType> : public StorageInterface
{
public:
    StorageContainer() = default;
    ~StorageContainer() = default;
    
    explicit StorageContainer(StorageType&& data);

	// StorageInterface {
	[[nodiscard]] virtual std::unique_ptr<StorageInterface> Clone() const override;
    virtual void ExtractToJsonValue(boost::json::value& target) override;
	// } StorageInterface

	// Viewable {
    [[nodiscard]] virtual std::span<std::uint8_t const> GetReadableView() const override;
    [[nodiscard]] virtual std::string_view GetStringView() const override;
    [[nodiscard]] virtual bool IsEmpty() const override;
	// } Viewable

	// Packable {
	[[nodiscard]] virtual std::size_t GetPackSize() const override;
    virtual void Inject(Buffer& buffer) const override;
    [[nodiscard]] virtual bool Unpack(
        std::span<std::uint8_t const>::iterator& begin,
        std::span<std::uint8_t const>::iterator const& end) override;
	// } Packable

private:
    StorageType m_data;
};

//----------------------------------------------------------------------------------------------------------------------

template<SharedStorageType StorageType>
class StorageContainer<StorageType> : public StorageInterface
{
public:
    StorageContainer() = default;
    ~StorageContainer() = default;

    explicit StorageContainer(StorageType&& data)
        : m_data(std::move(data))
    {
        assert(m_data);
    }

	// StorageInterface {
	[[nodiscard]] virtual std::unique_ptr<StorageInterface> Clone() const override
    {
        return std::make_unique<StorageContainer<StorageType>>(StorageType{ m_data });
    }

    virtual void ExtractToJsonValue(boost::json::value& target) override
    {
        target = boost::json::value_from(*m_data, boost::json::storage_ptr{});
    }
	// } StorageInterface

	// Viewable {
    [[nodiscard]] virtual std::span<std::uint8_t const> GetReadableView() const override;
    [[nodiscard]] virtual std::string_view GetStringView() const override;
    [[nodiscard]] virtual bool IsEmpty() const override
    {
        if (!m_data) { return true; }
        return m_data->empty();
    }
	// } Viewable

	// Packable {
	[[nodiscard]] virtual std::size_t GetPackSize() const override
    {
        std::size_t size = 0;
        size += sizeof(std::uint32_t); // 4 bytes for payload size
        if (m_data) { size += m_data->size(); }
        assert(std::in_range<std::uint32_t>(size));
        return size;
    }

    virtual void Inject(Buffer& buffer) const override
    {
        if (m_data) {
            assert(std::in_range<std::uint32_t>(m_data->size()));
            PackUtils::PackChunk<std::uint32_t>(*m_data, buffer);
        }
        else {
            PackUtils::PackChunk(std::uint32_t{ 0 }, buffer);
        }
    }

    [[nodiscard]] virtual bool Unpack(
        std::span<std::uint8_t const>::iterator& begin,
        std::span<std::uint8_t const>::iterator const& end) override
    {
        std::uint32_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return false; }

        auto const UnpackSharedStorage = [&] <typename InternalType> (std::shared_ptr<InternalType const>&container) -> bool {
            InternalType data;
            if (!PackUtils::UnpackChunk(begin, end, data, size)) { return false; }
            container = std::make_shared<InternalType const>(std::move(data));
            return true;
        };
        if (!UnpackSharedStorage(m_data)) { return false; }

        return true;
    }
	// } Packable

private:
    StorageType m_data;
};

//----------------------------------------------------------------------------------------------------------------------

class Payload : public DataInterface::Viewable, public DataInterface::Packable
{
public:
    Payload();

    template<SupportedStorageType StorageType>
    Payload(StorageType&& data);

    template<SupportedStorageType StorageType>
    Payload(StorageType const& data);

    Payload(std::string_view data);
    Payload(std::nullptr_t data);

    Payload(Payload&& other) noexcept;
    Payload(Payload const& other);
    Payload& operator=(Payload&& other) noexcept;
    Payload& operator=(Payload const& other);

    [[nodiscard]] bool operator==(Payload const& other) const;

    void ExtractToJsonValue(boost::json::value& target);

	// Viewable {
    [[nodiscard]] virtual std::span<std::uint8_t const> GetReadableView() const override;
    [[nodiscard]] virtual std::string_view GetStringView() const override;
    [[nodiscard]] virtual bool IsEmpty() const override;
	// } Viewable

	// Packable {
	[[nodiscard]] virtual std::size_t GetPackSize() const override;
    virtual void Inject(Buffer& buffer) const override;
    [[nodiscard]] virtual bool Unpack(
        std::span<std::uint8_t const>::iterator& begin,
        std::span<std::uint8_t const>::iterator const& end) override;
	// } Packable

private:
    std::unique_ptr<StorageInterface> m_storage;
};

//----------------------------------------------------------------------------------------------------------------------
} // Message namespace
//----------------------------------------------------------------------------------------------------------------------

template<Message::BufferStorageType StorageType>
inline Message::StorageContainer<StorageType>::StorageContainer(StorageType&& data)
    : m_data(std::move(data))
{
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::BufferStorageType StorageType>
inline std::unique_ptr<Message::StorageInterface> Message::StorageContainer<StorageType>::Clone() const
{
	return std::make_unique<StorageContainer<StorageType>>(StorageType{ m_data });
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::BufferStorageType StorageType>
inline void Message::StorageContainer<StorageType>::ExtractToJsonValue(boost::json::value& target)
{
    target = boost::json::value_from(std::move(m_data), boost::json::storage_ptr{});
}

//----------------------------------------------------------------------------------------------------------------------

template<>
inline std::span<std::uint8_t const> Message::StorageContainer<std::string>::GetReadableView() const
{
    return std::span{ reinterpret_cast<std::uint8_t const*>(m_data.data()), m_data.size() };
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::BufferStorageType StorageType>
inline std::span<std::uint8_t const> Message::StorageContainer<StorageType>::GetReadableView() const
{
    return m_data;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
inline std::string_view Message::StorageContainer<std::string>::GetStringView() const
{
    return m_data;
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::BufferStorageType StorageType>
inline std::string_view Message::StorageContainer<StorageType>::GetStringView() const
{
    return std::string_view{ reinterpret_cast<char const*>(m_data.data()), m_data.size() };
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::BufferStorageType StorageType>
inline bool Message::StorageContainer<StorageType>::IsEmpty() const
{
    return m_data.empty();
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::BufferStorageType StorageType>
inline std::size_t Message::StorageContainer<StorageType>::GetPackSize() const
{
	std::size_t size = 0;
	size += sizeof(std::uint32_t); // 4 bytes for payload size
	size += m_data.size();
    assert(std::in_range<std::uint32_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::BufferStorageType StorageType>
inline void Message::StorageContainer<StorageType>::Inject(Buffer& buffer) const
{
    assert(std::in_range<std::uint32_t>(m_data.size()));
    PackUtils::PackChunk<std::uint32_t>(m_data, buffer);
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::BufferStorageType StorageType>
inline bool Message::StorageContainer<StorageType>::Unpack(
    std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end)
{
    m_data = {};
    std::uint32_t size = 0;
    if (!PackUtils::UnpackChunk(begin, end, size)) { return false; }
    if (!PackUtils::UnpackChunk(begin, end, m_data, size)) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
inline bool Message::StorageContainer<std::span<std::uint8_t const>>::Unpack(
    std::span<std::uint8_t const>::iterator&, std::span<std::uint8_t const>::iterator const&)
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
inline std::span<std::uint8_t const> Message::StorageContainer<std::shared_ptr<std::string const>>::GetReadableView() const
{
    if (!m_data) { return {}; }
    return std::span{ reinterpret_cast<std::uint8_t const*>(m_data->data()), m_data->size() };
}

//----------------------------------------------------------------------------------------------------------------------

template<>
inline std::span<std::uint8_t const> Message::StorageContainer<std::shared_ptr<Message::Buffer const>>::GetReadableView() const
{
    if (!m_data) { return {}; }
    return *m_data;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
inline std::string_view Message::StorageContainer<std::shared_ptr<std::string const>>::GetStringView() const
{
    if (!m_data) { return {}; }
    return *m_data;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
inline std::string_view Message::StorageContainer<std::shared_ptr<Message::Buffer const>>::GetStringView() const
{
    if (!m_data) { return {}; }
    return std::string_view{ reinterpret_cast<char const*>(m_data->data()), m_data->size() };
}

//----------------------------------------------------------------------------------------------------------------------

inline Message::Payload::Payload()
    : m_storage(std::make_unique<StorageContainer<Buffer>>())
{
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::SupportedStorageType StorageType>
inline Message::Payload::Payload(StorageType&& data)
    : m_storage(std::make_unique<StorageContainer<StorageType>>(std::move(data)))
{
}

//----------------------------------------------------------------------------------------------------------------------

template<Message::SupportedStorageType StorageType>
inline Message::Payload::Payload(StorageType const& data)
    : m_storage(std::make_unique<StorageContainer<StorageType>>(StorageType{ data }))
{
}

//----------------------------------------------------------------------------------------------------------------------

inline Message::Payload::Payload(std::nullptr_t)
    : m_storage(std::make_unique<StorageContainer<std::shared_ptr<std::vector<std::uint8_t> const>>>())
{
}

//----------------------------------------------------------------------------------------------------------------------

inline Message::Payload::Payload(std::string_view data)
    : m_storage(std::make_unique<StorageContainer<std::string>>(std::string{ data.begin(), data.end() }))
{
}

//----------------------------------------------------------------------------------------------------------------------

inline Message::Payload::Payload(Payload&& other) noexcept
    : m_storage(std::move(other.m_storage))
{
}

//----------------------------------------------------------------------------------------------------------------------

inline Message::Payload::Payload(Payload const& other)
    : m_storage(other.m_storage->Clone())
{
}

//----------------------------------------------------------------------------------------------------------------------

inline Message::Payload& Message::Payload::operator=(Payload&& other) noexcept
{
    m_storage = std::move(other.m_storage);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

inline Message::Payload& Message::Payload::operator=(Payload const& other)
{
    if (other.m_storage) [[likely]] {
        m_storage = other.m_storage->Clone();
    }
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

inline bool Message::Payload::operator==(Payload const& other) const
{
    if (!m_storage || !other.m_storage) [[unlikely]] { return false; }
    return std::ranges::equal(m_storage->GetReadableView(), other.m_storage->GetReadableView());
}

//----------------------------------------------------------------------------------------------------------------------

inline void Message::Payload::ExtractToJsonValue(boost::json::value& target)
{
    if (m_storage) [[likely]] { m_storage->ExtractToJsonValue(target); }
}

//----------------------------------------------------------------------------------------------------------------------

inline std::span<std::uint8_t const> Message::Payload::GetReadableView() const
{
    if (!m_storage) [[unlikely]] { return {}; }
    return m_storage->GetReadableView();
}

//----------------------------------------------------------------------------------------------------------------------

inline std::string_view Message::Payload::GetStringView() const
{
    if (!m_storage) [[unlikely]] { return {}; }
    return m_storage->GetStringView();
}

//----------------------------------------------------------------------------------------------------------------------

inline bool Message::Payload::IsEmpty() const
{
    if (!m_storage) [[unlikely]] { return true; }
    return m_storage->IsEmpty();
}

//----------------------------------------------------------------------------------------------------------------------

inline std::size_t Message::Payload::GetPackSize() const
{
    if (!m_storage) [[unlikely]] { return 0; }
    return m_storage->GetPackSize();
}

//----------------------------------------------------------------------------------------------------------------------

inline void Message::Payload::Inject(Buffer& buffer) const
{
    if (m_storage) [[likely]] { m_storage->Inject(buffer); }
}

//----------------------------------------------------------------------------------------------------------------------

inline bool Message::Payload::Unpack(
    std::span<std::uint8_t const>::iterator& begin,
	std::span<std::uint8_t const>::iterator const& end)
{
    if (!m_storage) [[unlikely]] { return false; }
    return m_storage->Unpack(begin, end);
}

//----------------------------------------------------------------------------------------------------------------------
