//----------------------------------------------------------------------------------------------------------------------
// File: JsonPrettyPrinter.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <ostream>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace JSON {
//----------------------------------------------------------------------------------------------------------------------

class PrettyPrinter;

//----------------------------------------------------------------------------------------------------------------------
} // JSON namespace
//----------------------------------------------------------------------------------------------------------------------

class JSON::PrettyPrinter
{
public:
	static constexpr std::string_view ValueSeperator = ": ";
	static constexpr std::string_view FieldSeperator = ",\n";
	static constexpr std::string_view Newline = "\n";

	static constexpr std::size_t DefaultTabSize = 4;

	PrettyPrinter(std::size_t tabSize = DefaultTabSize);

	void Format(boost::json::value const& json, std::ostream& os);

private:
	void IncreaseIdent();
	void DecreaseIdent();

	std::string m_identation;
	std::size_t m_tabSize;
};

//----------------------------------------------------------------------------------------------------------------------

inline JSON::PrettyPrinter::PrettyPrinter(std::size_t tabSize)
	: m_identation()
	, m_tabSize(tabSize)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline void JSON::PrettyPrinter::Format(boost::json::value const& json, std::ostream& os)
{
	switch (json.kind()) {
		case boost::json::kind::object: {
			constexpr std::string_view OpenObject = "{\n";
			constexpr std::string_view CloseObject = "}";

			os << OpenObject;
			IncreaseIdent();
			auto const& object = json.get_object();
			if (!object.empty()) {
				for (auto itr = object.begin();;) {
					os << m_identation << boost::json::serialize(itr->key()) << ValueSeperator;
					Format(itr->value(), os);
					if (++itr == object.end()) { break; }
					os << FieldSeperator;
				}
			}
			os << Newline;
			DecreaseIdent();
			os << m_identation << CloseObject;
		} break;
		case boost::json::kind::array: {
			constexpr std::string_view OpenArray = "[\n";
			constexpr std::string_view CloseArray = "]";

			os << OpenArray;
			IncreaseIdent();
			auto const& array = json.get_array();
			if (!array.empty()) {
				for (auto itr = array.begin();;) {
					os << m_identation;
					Format(*itr, os);
					if (++itr == array.end()) { break; }
					os << FieldSeperator;
				}
			}
			os << Newline;
			DecreaseIdent();
			os << m_identation << CloseArray;
		} break;
		case boost::json::kind::bool_: {
			constexpr std::string_view True = "true";
			constexpr std::string_view False = "false";
			auto const& value = (json.get_bool()) ? True : False;
			os << value;
		}  break;
		case boost::json::kind::string: { os << boost::json::serialize(json.get_string()); }  break;
		case boost::json::kind::uint64: { os << json.get_uint64(); }  break;
		case boost::json::kind::int64: { os << json.get_int64(); }  break;
		case boost::json::kind::double_: { os << json.get_double(); }  break;
		case boost::json::kind::null: { os << "null"; }  break;
	}

	if (m_identation.empty()) { os << Newline; }
}

//----------------------------------------------------------------------------------------------------------------------

inline void JSON::PrettyPrinter::IncreaseIdent() { m_identation.append(m_tabSize, ' '); }

//----------------------------------------------------------------------------------------------------------------------

inline void JSON::PrettyPrinter::DecreaseIdent() { m_identation.resize(m_identation.size() - m_tabSize); }

//----------------------------------------------------------------------------------------------------------------------
