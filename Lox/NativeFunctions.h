#pragma once
#include <memory>
#include <string>
#include "LoxCallable.h"
#include <chrono>

namespace Lox
{
	class Clock : public LoxCallable
	{
	public:
		std::shared_ptr<Object> Call(Interpreter& interpreter, std::vector <std::shared_ptr<Object>> args)
		{
			return std::make_shared<NumberObject>(std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count());
		}

		size_t Arity() const override { return 0; }
		std::string ToString() const override { return "<native fn>"; }
	};
}