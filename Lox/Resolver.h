#pragma once
#include "Expr.h"
#include "Stmt.h"
#include "Interpreter.h"

#include <vector>
#include <unordered_map>
#include <string>

namespace Lox
{
	class Resolver : public ExprVisitor, public StmtVisitor
	{
	private :
		enum class FunctionType { NONE, FUNCTION, INITIALISER, METHOD };
		enum class BindState { DECLARED, DEFINED };
		enum class ClassType { NONE, CLASS, SUBCLASS };

	public:
		Resolver(Interpreter& interpreter, std::vector<std::shared_ptr<Stmt>> statements);
		Resolver(Interpreter& interpreter);

		void VisitBlockStmt(Block& stmt);
		void VisitVarStmt(Var& stmt);
		void VisitFunctionStmt(Function& stmt);
		void VisitExpressionStmt(Expression& stmt);
		void VisitIfStmt(If& stmt);
		void VisitPrintStmt(Print& stmt);
		void VisitReturnStmt(Return& stmt);
		void VisitWhileStmt(While& stmt);
		void VisitForStmt(For& stmt);
		void VisitBreakStmt(Break& stmt);
		void VisitContinueStmt(Continue& stmt);
		void VisitClassStmt(Class& stmt);

		void VisitVariableExpr(Variable& expr);
		void VisitAssignExpr(Assign& expr);
		void VisitBinaryExpr(Binary& expr);
		void VisitCallExpr(Call& expr);
		void VisitGroupingExpr(Grouping& expr);
		void VisitLiteralExpr(Literal& expr);
		void VisitLogicalExpr(Logical& expr);
		void VisitUnaryExpr(Unary& expr);
		void VisitGetExpr(Get& expr);
		void VisitSetExpr(Set& expr);
		void VisitThisExpr(This& expr);
		void VisitSuperExpr(Super& expr);

		void Declare(const Token& name);
		void Define(const Token& name);
		void BeginScope();
		void EndScope();
		void Resolve(std::vector<std::shared_ptr<Stmt>> statements);
		void Resolve(std::shared_ptr<Stmt> statement);
		void Resolve(std::shared_ptr<Expr> expression);
		void ResolveLocal(Expr& expr, const Token& name);
		void ResolveFunction(Function& function, const FunctionType type);

	private:
		Interpreter& LoxInterpreter;
		std::vector<std::unordered_map<std::string, BindState>> Scopes;
		FunctionType CurrentFunction = FunctionType::NONE;
		ClassType CurrentClass = ClassType::NONE;
	};
}