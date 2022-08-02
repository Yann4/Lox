#pragma once
#include <string>
#include <cmath>

namespace Lox
{
	enum class LiteralTypes
	{
		STRING,
		NUMBER,
		BOOL,
		CALLABLE,
		CLASS,
		INSTANCE,
		NIL
	};

	class Object
	{
	public:
		virtual std::string ToString() const = 0;
		virtual LiteralTypes GetType() const = 0;
		virtual bool Equals(Object* obj) const = 0;
	};

	class StringObject : public Object
	{
	public:
		StringObject(const std::string& ltrl) : Data(ltrl) {}
		StringObject(const StringObject& other) : Data(other.Data) {}
		std::string ToString() const override { return Data; }
		LiteralTypes GetType() const override { return LiteralTypes::STRING; }
		bool Equals(Object* obj) const
		{
			if (obj == nullptr)
			{
				return false;
			}

			if (obj->GetType() == GetType())
			{
				return Data == reinterpret_cast<StringObject*>(obj)->Data;
			}

			return false;
		}


		const std::string Data;
	};

	class NumberObject : public Object
	{

	public:
		NumberObject(const double ltrl) : Data(ltrl) {}
		NumberObject(const NumberObject& other) : Data(other.Data) {}
		std::string ToString() const override 
		{
			if(IsInt())
			{
				return std::to_string((int)Data);
			}
			else
			{
				return std::to_string(Data);
			}
		}
		LiteralTypes GetType() const override { return LiteralTypes::NUMBER; }

		bool Equals(Object* obj) const
		{
			if (obj == nullptr)
			{
				return false;
			}

			if (obj->GetType() == GetType())
			{
				return Data == reinterpret_cast<NumberObject*>(obj)->Data;
			}

			return false;
		}

		bool IsInt() const
		{
			double intpart;
			return std::modf(Data, &intpart) == 0.0;
		}

		double Data;
	};

	class BoolObject : public Object
	{
	public:
		BoolObject(const bool ltrl) : Data(ltrl) {}
		BoolObject(const BoolObject& other) : Data(other.Data) {}

		std::string ToString() const override { return std::to_string(Data); }
		LiteralTypes GetType() const override { return LiteralTypes::BOOL; }

		bool Equals(Object* obj) const
		{
			if (obj == nullptr)
			{
				return false;
			}

			if (obj->GetType() == GetType())
			{
				return Data == reinterpret_cast<BoolObject*>(obj)->Data;
			}

			return false;
		}

		bool Data;
	};

	class NilObject : public Object
	{
	public:
		NilObject() {}
		NilObject(const NilObject& other) {}

		std::string ToString() const override { return "nil"; }
		LiteralTypes GetType() const override { return LiteralTypes::NIL; }

		bool Equals(Object* obj) const
		{
			if (obj == nullptr)
			{
				return false;
			}

			return obj->GetType() == GetType();
		}
	};
}