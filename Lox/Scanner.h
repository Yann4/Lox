#pragma once
#include <vector>
#include <string>
#include <map>

#include "Token.h"

namespace Lox
{
	class Scanner
	{
	public:
		Scanner(const std::string& source);

		const std::vector<Token>& ScanTokens();

	private:
		void ScanToken();

		bool IsDigit(const char c) const { return c >= '0' && c <= '9'; }
		bool IsAlpha(const char c) const { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
		bool IsAlphaNumeric(const char c) const { return IsDigit(c) || IsAlpha(c); }

		bool AtEnd() const { return current >= Source.length(); }
		const char Advance() { return Source[current++]; }
		bool Match(const char expected);
		const char Peek();
		const char PeekNext();

		void String();
		void Number();
		void Identifier();
		void BlockComment();

		void AddToken(TokenType type) { AddToken(type, nullptr); }
		void AddToken(TokenType type, std::shared_ptr<LitType> ltrl);
	private:
		const std::string Source;

		std::vector<Token> Tokens;

		size_t start;
		size_t current;
		size_t line;

		struct cmp_str
		{
			bool operator()(char const* lhs, char const* rhs) const
			{
				return std::strcmp(lhs, rhs) < 0;
			}
		};

		static const std::map<const char*, TokenType, cmp_str> Keywords;
	};
}