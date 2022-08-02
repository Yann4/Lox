#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional> //reference_wrapper
#include "Environment.h"
#include "Expr.h"
#include "Object.h"
#include "Stmt.h"

namespace Lox
{
	class Interpreter : public ExprVisitor, public StmtVisitor
	{
	public:
		Interpreter();

		void Interpret(std::vector<std::shared_ptr<Stmt>>& statements);
		void Execute(std::shared_ptr<Stmt>& statement);
		void ExecuteBlock(std::vector<std::shared_ptr<Stmt>> statements, std::shared_ptr<Environment> environment);
		void Resolve(Expr& expr, const size_t depth);
		std::shared_ptr<Object> LookUpVariable(const Token& name, Expr& expr) const;

		void VisitLiteralExpr(Literal& expr);
		void VisitGroupingExpr(Grouping& expr);
		void VisitUnaryExpr(Unary& expr);
		void VisitBinaryExpr(Binary& expr);
		void VisitVariableExpr(Variable& expr);
		void VisitAssignExpr(Assign& expr);
		void VisitLogicalExpr(Logical& expr);
		void VisitCallExpr(Call& expr);
		void VisitGetExpr(Get& expr);
		void VisitSetExpr(Set& expr);
		void VisitThisExpr(This& expr);
		void VisitSuperExpr(Super& expr);

		void VisitExpressionStmt(Expression& stmt);
		void VisitBlockStmt(Block& stmt);
		void VisitPrintStmt(Print& stmt);
		void VisitVarStmt(Var& stmt);
		void VisitIfStmt(If& stmt);
		void VisitWhileStmt(While& stmt);
		void VisitForStmt(For& stmt);
		void VisitBreakStmt(Break& stmt);
		void VisitContinueStmt(Continue& stmt);
		void VisitFunctionStmt(Function& stmt);
		void VisitReturnStmt(Return& stmt);
		void VisitClassStmt(Class& stmt);
	private:
		std::shared_ptr<Object> Evaluate(std::shared_ptr<Expr> expr);

		bool IsTruthy(std::shared_ptr<Object> object) const;
		bool IsEqual(std::shared_ptr<Object> left, std::shared_ptr<Object> right) const;

		void CheckNumberOperand(Token op, std::shared_ptr<Object> operand) const;
		void CheckNumberOperands(Token op, std::shared_ptr<Object> left, std::shared_ptr<Object> right) const;

		template <typename T>
		std::shared_ptr<T> ObjectAs(std::shared_ptr<Object> obj) const
		{
			return std::reinterpret_pointer_cast<T, Object>(obj);
		}

	private:
		std::shared_ptr<Object> Obj;
		std::shared_ptr<Environment> Globals;
		std::shared_ptr<Environment> Env;
		std::unordered_map<Expr*, size_t> Locals;
	private:
		class FlowControl : public std::exception
		{
		public:
			FlowControl(Stmt& controlStatement) : Control(controlStatement) {}
			Stmt& Control;
		};
	};

	class ReturnStatement : public std::exception
	{
	public:
		ReturnStatement(std::shared_ptr<Object> value) : Value(value) {}
		std::shared_ptr<Object> Value;
	};
}