#pragma once

#include <exception>
#include <string>
#include "Token.h"

namespace Lox
{
	class RuntimeException : public std::exception
	{
	public:
		RuntimeException(Token t, std::string message) :
			ErrorToken(t), Message(message)
		{ }

		const char* what() const throw()
		{
			return Message.c_str();
		}

	public:
		Token ErrorToken;
		std::string Message;
	};
}