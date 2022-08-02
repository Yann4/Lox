#include "Logging.h"

#include <iostream>
#include <sstream>

namespace Lox
{
	namespace Logging
	{
		static bool ErrorFlag = false;
		static bool RuntimeErrorFlag = false;

		void Report(size_t line, const char* position, const char* message)
		{
			std::cout << "[Line " << line << "] Error" << position << ": " << message << "\n";
			ErrorFlag = true;
		}

		void Error(size_t line, const char* message)
		{
			Report(line, "", message);
		}

		void Error(Token token, const char* message)
		{
			if (token.Type == TokenType::END_OF_FILE)
			{
				Report(token.Line, " at end", message);
			}
			else
			{
				std::stringstream ss;
				ss << " at '" << token.Lexeme << "'";
				Report(token.Line, ss.str().c_str(), message);
			}
		}

		void RuntimeError(const RuntimeException& ex)
		{
			std::cout << ex.Message << "\n[line " << ex.ErrorToken.Line << "]\n";
			RuntimeErrorFlag = true;
		}

		bool HadError()
		{
			return ErrorFlag;
		}
		
		bool HadRuntimeError()
		{
			return RuntimeErrorFlag;
		}

		void ResetError()
		{
			ErrorFlag = false;
		}
	}
}