#include "LoxClass.h"
#include "RuntimeError.h"

namespace Lox
{
	/*
	* LoxClass implementations
	*/

	LoxClass::LoxClass(const std::string& name, std::shared_ptr<LoxClass> superclass, std::unordered_map<std::string, LoxFunction> methods) :
		Name(name),
		Super(superclass),
		Methods(methods)
	{}

	std::shared_ptr<LoxFunction> LoxClass::FindMethod(const std::string& name) const
	{
		const auto iterator = Methods.find(name);
		if (iterator != Methods.cend())
		{
			return std::make_shared<LoxFunction>(iterator->second);
		}

		if (Super != nullptr)
		{
			return Super->FindMethod(name);
		}

		return nullptr;
	}

	std::string LoxClass::ToString() const 
	{
		return Name;
	}

	bool LoxClass::Equals(Object* obj) const 
	{
		return obj == this;
	}

	std::shared_ptr<Object> LoxClass::Call(Interpreter& interpreter, std::vector<std::shared_ptr<Object>> args)
	{
		std::shared_ptr<LoxInstance> instance = std::make_shared<LoxInstance>(*this);
		
		//Bind init method to instance & call it (if the class has one)
		std::shared_ptr<LoxFunction> initialiser = FindMethod("init");
		if (initialiser != nullptr)
		{
			initialiser->Bind(instance)->Call(interpreter, args);
		}

		return instance;
	}

	size_t LoxClass::Arity() const
	{
		std::shared_ptr<LoxFunction> init = FindMethod("init");
		return init == nullptr ? 0 : init->Arity();
	}

	/*
	* LoxInstance implementations
	*/

	LoxInstance::LoxInstance(const LoxInstance& other) :
		Class(other.Class),
		Fields(other.Fields)
	{ }

	std::shared_ptr<Object> LoxInstance::Get(const Token& name) const
	{
		auto field = Fields.find(name.Lexeme);
		if (field != Fields.end())
		{
			return field->second;
		}

		std::shared_ptr<LoxFunction> method = Class.FindMethod(name.Lexeme);
		if (method != nullptr)
		{
			return method->Bind(std::make_shared<LoxInstance>(*this));
		}

		throw RuntimeException(name, "Undefined property '" + name.Lexeme + "' .");
	}

	void LoxInstance::Set(const Token& name, std::shared_ptr<Object> value)
	{
		Fields[name.Lexeme] = value;
	}
}