#include "LoxCallable.h"
#include <sstream>

namespace Lox
{
	LoxFunction::LoxFunction(Function& declaration, std::shared_ptr<Environment> closure, bool isInit) :
		Declaration(declaration), Closure(closure), IsInitialiser(isInit)
	{}

	std::shared_ptr<LoxFunction> LoxFunction::Bind(std::shared_ptr<Object> instance) const
	{
		std::shared_ptr<Environment> environment = std::make_shared<Environment>(Closure);
		environment->Define("this", instance);
		return std::make_shared<LoxFunction>(Declaration, environment, IsInitialiser);
	}

	std::shared_ptr<Object> LoxFunction::Call(Interpreter& interpreter, std::vector<std::shared_ptr<Object>> args)
	{
		std::shared_ptr<Environment> environment = std::make_shared<Environment>(Closure);
		for (int idx = 0; idx < Declaration.Params.size(); idx++)
		{
			environment->Define(Declaration.Params[idx].Lexeme, args[idx]);
		}

		try
		{
			interpreter.ExecuteBlock(Declaration.Body, environment);
		}
		catch (const ReturnStatement& returnValue)
		{
			if (IsInitialiser)
			{
				return Closure->GetAt(0, "this");
			}
			else
			{
				return returnValue.Value;
			}
		}

		if (IsInitialiser)
		{
			return Closure->GetAt(0, "this");
		}
		else
		{
			return std::make_shared<NilObject>();
		}
	}

	std::string LoxFunction::ToString() const
	{
		std::stringstream ss;
		ss << "<fn " << Declaration.Name.Lexeme << ">";
		return ss.str();
	}
}