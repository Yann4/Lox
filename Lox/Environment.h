#pragma once

#include <map>
#include <memory>
#include <string>
#include "Object.h"
#include "Token.h"

namespace Lox
{
	class Environment
	{
	public:
		Environment();
		Environment(std::shared_ptr<Environment> enclosing);

		void Define(const std::string& name, std::shared_ptr<Object> value);
		void Assign(const Token& name, std::shared_ptr<Object> value);
		void AssignAt(const size_t distance, const Token& name, std::shared_ptr<Object> value);
		std::shared_ptr<Object> Get(const Token& name) const;
		std::shared_ptr<Object> GetAt(const size_t distance, const Token& name);
		std::shared_ptr<Object> GetAt(const size_t distance, const std::string& name);
		std::shared_ptr<Environment> EnclosingEnvironment() const { return Enclosing; }
	private:
		Environment* Ancestor(const size_t distance);
	private:
		std::map<std::string, std::shared_ptr<Object>> Values;
		std::shared_ptr<Environment> Enclosing;
	};
}