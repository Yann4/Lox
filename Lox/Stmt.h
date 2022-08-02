/*
* This file is generated, please do not edit
*/

#pragma once
#include <memory>
#include "Token.h"
#include "Expr.h"
#include <vector>

namespace Lox
{
	template <typename T> using shared_ptr = std::shared_ptr<T>;
	template <typename T> using vector = std::vector<T>;

	class Block;
	class Expression;
	class Function;
	class Class;
	class If;
	class Print;
	class Return;
	class Var;
	class While;
	class For;
	class Break;
	class Continue;

	class StmtVisitor
	{
	public:
		virtual void VisitBlockStmt(Block& stmt) = 0;
		virtual void VisitExpressionStmt(Expression& stmt) = 0;
		virtual void VisitFunctionStmt(Function& stmt) = 0;
		virtual void VisitClassStmt(Class& stmt) = 0;
		virtual void VisitIfStmt(If& stmt) = 0;
		virtual void VisitPrintStmt(Print& stmt) = 0;
		virtual void VisitReturnStmt(Return& stmt) = 0;
		virtual void VisitVarStmt(Var& stmt) = 0;
		virtual void VisitWhileStmt(While& stmt) = 0;
		virtual void VisitForStmt(For& stmt) = 0;
		virtual void VisitBreakStmt(Break& stmt) = 0;
		virtual void VisitContinueStmt(Continue& stmt) = 0;
	};

	enum class StmtType
	{
		Block,
		Expression,
		Function,
		Class,
		If,
		Print,
		Return,
		Var,
		While,
		For,
		Break,
		Continue,
	};

	class Stmt 
	{
	public:
		virtual void Accept(StmtVisitor& visitor) = 0;
		virtual StmtType GetType() const = 0;
	};

	class Block : public Stmt
	{
	public:
		Block(vector<shared_ptr<Stmt>> statements) :
			Statements(statements)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitBlockStmt(*this);
		}

		StmtType GetType() const { return StmtType::Block; }
	public:
		vector<shared_ptr<Stmt>> Statements;
	};

	class Expression : public Stmt
	{
	public:
		Expression(shared_ptr<Expr> subjectExpression) :
			Subjectexpression(subjectExpression)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitExpressionStmt(*this);
		}

		StmtType GetType() const { return StmtType::Expression; }
	public:
		shared_ptr<Expr> Subjectexpression;
	};

	class Function : public Stmt
	{
	public:
		Function(Token name, vector<Token> params, vector<shared_ptr<Stmt>> body) :
			Name(name), Params(params), Body(body)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitFunctionStmt(*this);
		}

		StmtType GetType() const { return StmtType::Function; }
	public:
		Token Name;
		vector<Token> Params;
		vector<shared_ptr<Stmt>> Body;
	};

	class Class : public Stmt
	{
	public:
		Class(Token name, shared_ptr<Variable> superclass, vector<shared_ptr<Function>> methods) :
			Name(name), Superclass(superclass), Methods(methods)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitClassStmt(*this);
		}

		StmtType GetType() const { return StmtType::Class; }
	public:
		Token Name;
		shared_ptr<Variable> Superclass;
		vector<shared_ptr<Function>> Methods;
	};

	class If : public Stmt
	{
	public:
		If(shared_ptr<Expr> condition, shared_ptr<Stmt> thenBranch, shared_ptr<Stmt> elseBranch) :
			Condition(condition), Thenbranch(thenBranch), Elsebranch(elseBranch)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitIfStmt(*this);
		}

		StmtType GetType() const { return StmtType::If; }
	public:
		shared_ptr<Expr> Condition;
		shared_ptr<Stmt> Thenbranch;
		shared_ptr<Stmt> Elsebranch;
	};

	class Print : public Stmt
	{
	public:
		Print(shared_ptr<Expr> subjectExpression) :
			Subjectexpression(subjectExpression)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitPrintStmt(*this);
		}

		StmtType GetType() const { return StmtType::Print; }
	public:
		shared_ptr<Expr> Subjectexpression;
	};

	class Return : public Stmt
	{
	public:
		Return(Token keyword, shared_ptr<Expr> value) :
			Keyword(keyword), Value(value)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitReturnStmt(*this);
		}

		StmtType GetType() const { return StmtType::Return; }
	public:
		Token Keyword;
		shared_ptr<Expr> Value;
	};

	class Var : public Stmt
	{
	public:
		Var(Token name, shared_ptr<Expr> initialiser) :
			Name(name), Initialiser(initialiser)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitVarStmt(*this);
		}

		StmtType GetType() const { return StmtType::Var; }
	public:
		Token Name;
		shared_ptr<Expr> Initialiser;
	};

	class While : public Stmt
	{
	public:
		While(shared_ptr<Expr> condition, shared_ptr<Stmt> body) :
			Condition(condition), Body(body)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitWhileStmt(*this);
		}

		StmtType GetType() const { return StmtType::While; }
	public:
		shared_ptr<Expr> Condition;
		shared_ptr<Stmt> Body;
	};

	class For : public Stmt
	{
	public:
		For(shared_ptr<Stmt> initialise, shared_ptr<Expr> condition, shared_ptr<Expr> increment, shared_ptr<Stmt> body) :
			Initialise(initialise), Condition(condition), Increment(increment), Body(body)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitForStmt(*this);
		}

		StmtType GetType() const { return StmtType::For; }
	public:
		shared_ptr<Stmt> Initialise;
		shared_ptr<Expr> Condition;
		shared_ptr<Expr> Increment;
		shared_ptr<Stmt> Body;
	};

	class Break : public Stmt
	{
	public:
		Break(Token keyword) :
			Keyword(keyword)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitBreakStmt(*this);
		}

		StmtType GetType() const { return StmtType::Break; }
	public:
		Token Keyword;
	};

	class Continue : public Stmt
	{
	public:
		Continue(Token keyword) :
			Keyword(keyword)
		{ }

		void Accept (StmtVisitor& visitor) override
		{
			return visitor.VisitContinueStmt(*this);
		}

		StmtType GetType() const { return StmtType::Continue; }
	public:
		Token Keyword;
	};
}