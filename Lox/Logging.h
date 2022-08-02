#pragma once

#include "Token.h"
#include "RuntimeError.h"

namespace Lox
{
	namespace Logging
	{
		bool HadError();
		bool HadRuntimeError();
		void ResetError();

		void Report(size_t line, const char* position, const char* message);
		void Error(size_t line, const char* message);
		void Error(Token token, const char* message);
		void RuntimeError(const RuntimeException& ex);
	}
}