#pragma once
#include <string>
#include <sstream>
#include <memory>

#include "TokenType.h"
#include "Object.h"
#include "Magic_Enum.hpp"

namespace Lox
{
	using LitType = Object;

	class Token
	{
	public:
		Token(TokenType type, std::string lexeme, std::shared_ptr<LitType> literal, size_t line) :
			Type(type), Lexeme(lexeme), Literal(literal), Line(line)
		{}

		std::string ToString() const
		{
			if (Literal != nullptr)
			{
				std::ostringstream ss;
				ss << magic_enum::enum_name(Type) << " " << Lexeme << " " << Literal->ToString() << "\n";
				return ss.str();
			}
			else
			{
				std::ostringstream ss;
				ss << magic_enum::enum_name(Type) << " " << Lexeme << "\n";
				return ss.str();
			}
		}
	public:
		TokenType Type;
		std::string Lexeme;
		std::shared_ptr<LitType> Literal;
		size_t Line;
	};
}