#pragma once

#include <string>
#include <map>
#include "LoxCallable.h"

namespace Lox
{
	class LoxClass : public LoxCallable
	{
	public:
		LoxClass(const std::string& name, std::shared_ptr<LoxClass> superclass, std::unordered_map<std::string, LoxFunction> methods);

		std::shared_ptr<LoxFunction> FindMethod(const std::string& name) const;

		//Object interface
		std::string ToString() const override;
		LiteralTypes GetType() const override { return LiteralTypes::CLASS; }
		bool Equals(Object* obj) const override;

		//LoxCallable interface
		virtual std::shared_ptr<Object> Call(Interpreter& interpreter, std::vector<std::shared_ptr<Object>> args) override;
		virtual size_t Arity() const override;
	private:
		std::string Name;
		std::unordered_map<std::string, LoxFunction> Methods;
		std::shared_ptr<LoxClass> Super;
	};

	class LoxInstance : public Object
	{
	public:
		LoxInstance(LoxClass& klass) : Class(klass) {}
		LoxInstance(const LoxInstance& other);

		std::shared_ptr<Object> Get(const Token& name) const;
		void Set(const Token& name, std::shared_ptr<Object> value);

		//Object interface
		std::string ToString() const override { return Class.ToString() + " instance"; }
		LiteralTypes GetType() const override { return LiteralTypes::INSTANCE; }
		bool Equals(Object* obj) const override { return obj == this; }
	private:
		LoxClass& Class;
		std::map<std::string, std::shared_ptr<Object>> Fields;
	};
}