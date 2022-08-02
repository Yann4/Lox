#pragma once
#include <vector>
#include <memory>
#include "Object.h"
#include "Interpreter.h"
#include "Stmt.h"

namespace Lox
{
	class LoxCallable : public Object
	{
	public:
		virtual std::shared_ptr<Object> Call(Interpreter& interpreter, std::vector<std::shared_ptr<Object>> args) = 0;
		virtual size_t Arity() const = 0;

		virtual LiteralTypes GetType() const { return LiteralTypes::CALLABLE; }
		virtual bool Equals(Object* obj) const { return this == obj; }
	};

	class LoxFunction : public LoxCallable
	{
	public:
		LoxFunction(Function& declaration, std::shared_ptr<Environment> closure, bool isInit);

		std::shared_ptr<LoxFunction> Bind(std::shared_ptr<Object> instance) const;

		std::shared_ptr<Object> Call(Interpreter& interpreter, std::vector<std::shared_ptr<Object>> args) override;
		size_t Arity() const override { return Declaration.Params.size(); }
		std::string ToString() const override;
	private:
		Function& Declaration;
		std::shared_ptr<Environment> Closure;
		bool IsInitialiser;
	};
}