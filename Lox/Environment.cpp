#include "Environment.h"
#include <sstream>
#include "RuntimeError.h"

namespace Lox
{
	Environment::Environment() : Values({}), Enclosing(nullptr)
	{}

	Environment::Environment(std::shared_ptr<Environment> enclosing) : Values({}), Enclosing(enclosing)
	{}

	void Environment::Define(const std::string& name, std::shared_ptr<Object> value)
	{
		Values[name] = value;
	}

	void Environment::Assign(const Token& name, std::shared_ptr<Object> value)
	{
		auto it = Values.find(name.Lexeme);
		if (it != Values.end())
		{
			it->second = value;
			return;
		}

		if (Enclosing != nullptr)
		{
			Enclosing->Assign(name, value);
			return;
		}

		std::stringstream ss;
		ss << "Undefined variable '" << name.Lexeme << "'.";
		throw RuntimeException(name, ss.str());
	}

	void Environment::AssignAt(const size_t distance, const Token& name, std::shared_ptr<Object> value)
	{
		Ancestor(distance)->Values.find(name.Lexeme)->second = value;
	}

	std::shared_ptr<Object> Environment::Get(const Token& name) const
	{
		auto it = Values.find(name.Lexeme);
		if (it != Values.end())
		{
			return it->second;
		}

		if (Enclosing != nullptr)
		{
			return Enclosing->Get(name);
		}

		std::stringstream ss;
		ss << "Undefined variable '" << name.Lexeme << "'.";
		throw RuntimeException(name, ss.str());
	}

	std::shared_ptr<Object> Environment::GetAt(const size_t distance, const Token& name)
	{
		return GetAt(distance, name.Lexeme);
	}

	std::shared_ptr<Object> Environment::GetAt(const size_t distance, const std::string& name)
	{
		return Ancestor(distance)->Values.find(name)->second;
	}

	Environment* Environment::Ancestor(const size_t distance)
	{
		Environment* environment = this;
		for (size_t idx = 0; idx < distance; idx++)
		{
			environment = environment->Enclosing.get();
		}

		return environment;
	}
}