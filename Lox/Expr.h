/*
* This file is generated, please do not edit
*/

#pragma once
#include <memory>
#include "Token.h"
#include "Object.h"
#include <vector>

namespace Lox
{
	template <typename T> using shared_ptr = std::shared_ptr<T>;
	template <typename T> using vector = std::vector<T>;

	class Assign;
	class Binary;
	class Call;
	class Get;
	class Grouping;
	class Literal;
	class Logical;
	class Set;
	class Super;
	class This;
	class Unary;
	class Variable;

	class ExprVisitor
	{
	public:
		virtual void VisitAssignExpr(Assign& expr) = 0;
		virtual void VisitBinaryExpr(Binary& expr) = 0;
		virtual void VisitCallExpr(Call& expr) = 0;
		virtual void VisitGetExpr(Get& expr) = 0;
		virtual void VisitGroupingExpr(Grouping& expr) = 0;
		virtual void VisitLiteralExpr(Literal& expr) = 0;
		virtual void VisitLogicalExpr(Logical& expr) = 0;
		virtual void VisitSetExpr(Set& expr) = 0;
		virtual void VisitSuperExpr(Super& expr) = 0;
		virtual void VisitThisExpr(This& expr) = 0;
		virtual void VisitUnaryExpr(Unary& expr) = 0;
		virtual void VisitVariableExpr(Variable& expr) = 0;
	};

	enum class ExprType
	{
		Assign,
		Binary,
		Call,
		Get,
		Grouping,
		Literal,
		Logical,
		Set,
		Super,
		This,
		Unary,
		Variable,
	};

	class Expr 
	{
	public:
		virtual void Accept(ExprVisitor& visitor) = 0;
		virtual ExprType GetType() const = 0;
	};

	class Assign : public Expr
	{
	public:
		Assign(Token name, shared_ptr<Expr> value) :
			Name(name), Value(value)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitAssignExpr(*this);
		}

		ExprType GetType() const { return ExprType::Assign; }
	public:
		Token Name;
		shared_ptr<Expr> Value;
	};

	class Binary : public Expr
	{
	public:
		Binary(shared_ptr<Expr> left, Token op, shared_ptr<Expr> right) :
			Left(left), Op(op), Right(right)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitBinaryExpr(*this);
		}

		ExprType GetType() const { return ExprType::Binary; }
	public:
		shared_ptr<Expr> Left;
		Token Op;
		shared_ptr<Expr> Right;
	};

	class Call : public Expr
	{
	public:
		Call(shared_ptr<Expr> callee, Token paren, vector<shared_ptr<Expr>> arguments) :
			Callee(callee), Paren(paren), Arguments(arguments)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitCallExpr(*this);
		}

		ExprType GetType() const { return ExprType::Call; }
	public:
		shared_ptr<Expr> Callee;
		Token Paren;
		vector<shared_ptr<Expr>> Arguments;
	};

	class Get : public Expr
	{
	public:
		Get(shared_ptr<Expr> object, Token name) :
			Object(object), Name(name)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitGetExpr(*this);
		}

		ExprType GetType() const { return ExprType::Get; }
	public:
		shared_ptr<Expr> Object;
		Token Name;
	};

	class Grouping : public Expr
	{
	public:
		Grouping(shared_ptr<Expr> expression) :
			Expression(expression)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitGroupingExpr(*this);
		}

		ExprType GetType() const { return ExprType::Grouping; }
	public:
		shared_ptr<Expr> Expression;
	};

	class Literal : public Expr
	{
	public:
		Literal(shared_ptr<Object> value) :
			Value(value)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitLiteralExpr(*this);
		}

		ExprType GetType() const { return ExprType::Literal; }
	public:
		shared_ptr<Object> Value;
	};

	class Logical : public Expr
	{
	public:
		Logical(shared_ptr<Expr> left, Token op, shared_ptr<Expr> right) :
			Left(left), Op(op), Right(right)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitLogicalExpr(*this);
		}

		ExprType GetType() const { return ExprType::Logical; }
	public:
		shared_ptr<Expr> Left;
		Token Op;
		shared_ptr<Expr> Right;
	};

	class Set : public Expr
	{
	public:
		Set(shared_ptr<Expr> object, Token name, shared_ptr<Expr> value) :
			Object(object), Name(name), Value(value)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitSetExpr(*this);
		}

		ExprType GetType() const { return ExprType::Set; }
	public:
		shared_ptr<Expr> Object;
		Token Name;
		shared_ptr<Expr> Value;
	};

	class Super : public Expr
	{
	public:
		Super(Token keyword, Token method) :
			Keyword(keyword), Method(method)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitSuperExpr(*this);
		}

		ExprType GetType() const { return ExprType::Super; }
	public:
		Token Keyword;
		Token Method;
	};

	class This : public Expr
	{
	public:
		This(Token keyword) :
			Keyword(keyword)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitThisExpr(*this);
		}

		ExprType GetType() const { return ExprType::This; }
	public:
		Token Keyword;
	};

	class Unary : public Expr
	{
	public:
		Unary(Token op, shared_ptr<Expr> right) :
			Op(op), Right(right)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitUnaryExpr(*this);
		}

		ExprType GetType() const { return ExprType::Unary; }
	public:
		Token Op;
		shared_ptr<Expr> Right;
	};

	class Variable : public Expr
	{
	public:
		Variable(Token name) :
			Name(name)
		{ }

		void Accept (ExprVisitor& visitor) override
		{
			return visitor.VisitVariableExpr(*this);
		}

		ExprType GetType() const { return ExprType::Variable; }
	public:
		Token Name;
	};
}