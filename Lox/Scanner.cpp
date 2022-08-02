#include "Scanner.h"
#include "Logging.h"

namespace Lox
{

	const std::map<const char*, TokenType, Scanner::cmp_str> Scanner::Keywords =
	{
		{"and", TokenType::AND},
		{"class", TokenType::CLASS},
		{"else", TokenType::ELSE},
		{"false", TokenType::FALSE},
		{"for", TokenType::FOR},
		{"fun", TokenType::FUN},
		{"if", TokenType::IF},
		{"nil", TokenType::NIL},
		{"or", TokenType::OR},
		{"print", TokenType::PRINT},
		{"return", TokenType::RETURN},
		{"super", TokenType::SUPER},
		{"this", TokenType::THIS},
		{"true", TokenType::TRUE},
		{"var", TokenType::VAR},
		{"while", TokenType::WHILE},
		{"break", TokenType::BREAK},
		{"continue", TokenType::CONTINUE},
	};


	Scanner::Scanner(const std::string& source) :
		Source(source),
		start(0), current(0), line(1)
	{ }

	const std::vector<Token>& Scanner::ScanTokens()
	{
		while (!AtEnd())
		{
			start = current;
			ScanToken();
		}

		Tokens.emplace_back(TokenType::END_OF_FILE, "", nullptr, line);
		return Tokens;
	}

	void Scanner::ScanToken()
	{
		char c = Advance();

		switch (c)
		{
		case '(': AddToken(TokenType::LEFT_PAREN); break;
		case ')': AddToken(TokenType::RIGHT_PAREN); break;
		case '{': AddToken(TokenType::LEFT_BRACE); break;
		case '}': AddToken(TokenType::RIGHT_BRACE); break;
		case ',': AddToken(TokenType::COMMA); break;
		case '.': AddToken(TokenType::DOT); break;
		case '-': AddToken(TokenType::MINUS); break;
		case '+': AddToken(TokenType::PLUS); break;
		case ';': AddToken(TokenType::SEMICOLON); break;
		case '*': AddToken(TokenType::STAR); break;
		case '!':
			AddToken(Match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
			break;
		case '=':
			AddToken(Match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
			break;
		case '<':
			AddToken(Match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
			break;
		case '>':
			AddToken(Match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
			break;
		case '/':
			if (Match('/'))
			{
				//A comment goes to the end of the line
				while (Peek() != '\n' && !AtEnd())
				{
					Advance();
				}
			}
			else if (Match('*')) //Handle multiline block comments
			{
				BlockComment();
			}
			else
			{
				AddToken(TokenType::SLASH);
			}
			break;
		case ' ':
		case '\r':
		case '\t':
			//Ignore whitespace
			break;
		case '\n':
			line++;
			break;
		case '"': String(); break;
		default:
			if (IsDigit(c))
			{
				Number();
			}
			else if (IsAlpha(c))
			{
				Identifier();
			}
			else
			{
				Logging::Error(line, "Unexpected character.");
			}
			break;
		}
	}

	bool Scanner::Match(const char expected)
	{
		if (AtEnd())
		{
			return false;
		}

		if (Source[current] != expected)
		{
			return false;
		}

		current++;
		return true;
	}

	const char Scanner::Peek()
	{
		return AtEnd() ? '\0' : Source[current];
	}

	const char Scanner::PeekNext()
	{
		if (current + 1 >= Source.length())
		{
			return '\0';
		}

		return Source[current + 1];
	}

	void Scanner::String()
	{
		while (Peek() != '"' && !AtEnd())
		{
			if (Peek() == '\n')
			{
				line++;
			}

			Advance();
		}

		if (AtEnd())
		{
			Logging::Error(line, "Unterminated string literal.");
			return;
		}

		//The closing "
		Advance();

		//Trim surrounding quotes
		AddToken(TokenType::STRING, std::make_shared<StringObject>(Source.substr(start + 1, current - start - 2)));
	}

	void Scanner::Number()
	{
		while (IsDigit(Peek()))
		{
			Advance();
		}

		//Look for a fractional part
		if (Peek() == '.' && IsDigit(PeekNext()))
		{
			//Consume the '.'
			Advance();

			while (IsDigit(Peek()))
			{
				Advance();
			}
		}

		AddToken(TokenType::NUMBER, std::make_shared<NumberObject>(std::atof(Source.substr(start, current - start).c_str())));
	}

	void Scanner::Identifier()
	{
		while (IsAlphaNumeric(Peek()))
		{
			Advance();
		}

		std::string text = Source.substr(start, current - start);


		auto iterator = Keywords.find(text.c_str());
		if (iterator != Keywords.end())
		{
			AddToken(iterator->second);
		}
		else
		{
			AddToken(TokenType::IDENTIFIER);
		}
	}

	void Scanner::BlockComment()
	{
		while (!AtEnd())
		{
			char c = Advance();
			if (c == '*' && Peek() == '/')
			{
				Advance();
				Advance();
				return;
			}
			//TODO: Bound recursion?
			else if (c == '/' && Peek() == '*') // Support nested block comments	
			{
				BlockComment();
			}
		}
	}

	void Scanner::AddToken(TokenType type, std::shared_ptr<LitType> ltrl)
	{
		std::string text = Source.substr(start, current - start);
		Tokens.emplace_back(type, text, ltrl, line);
	}
}