//------------------------------------------------------------------------------------------------
// File: MessageHeader.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
#include "../BryptIdentifier/BryptIdentifier.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <utility>
//------------------------------------------------------------------------------------------------

class CMessageHeader {
public:
	CMessageHeader();

	// CApplicationBuilder {
	friend class CApplicationBuilder;
    friend class CHandshakeBuilder;
    // } CApplicationBuilder 

	Message::Protocol GetMessageProtocol() const;
    Message::Version const& GetVersion() const;
    BryptIdentifier::CContainer const& GetSourceIdentifier() const;

    Message::Destination GetDestinationType() const;
    std::optional<BryptIdentifier::CContainer> const& GetDestinationIdentifier() const;


    std::uint32_t GetPackSize() const;
	Message::Buffer GetPackedBuffer() const;

	bool IsValid() const;

private:
    bool ParseBuffer(
        Message::Buffer::const_iterator& begin,
        Message::Buffer::const_iterator const& end);

	constexpr std::uint32_t FixedPackSize() const;

    Message::Protocol m_protocol;
    Message::Version m_version;
    BryptIdentifier::CContainer m_source;
    Message::Destination m_destination;

    std::optional<BryptIdentifier::CContainer> m_optDestinationIdentifier;

};

//------------------------------------------------------------------------------------------------
