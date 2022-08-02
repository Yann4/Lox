#include "Resolver.h"
#include "Logging.h"

namespace Lox
{
	Resolver::Resolver(Interpreter& interpreter, std::vector<std::shared_ptr<Stmt>> statements) :
		LoxInterpreter(interpreter),
		CurrentFunction(FunctionType::NONE),
		CurrentClass(ClassType::NONE)
	{
		Resolve(statements);
	}

	Resolver::Resolver(Interpreter& interpreter) :
		LoxInterpreter(interpreter),
		CurrentFunction(FunctionType::NONE),
		CurrentClass(ClassType::NONE)
	{ }

	void Resolver::Declare(const Token& name)
	{
		if (Scopes.empty())
		{
			return;
		}

		auto& scope = Scopes.back();
		if (scope.find(name.Lexeme) != scope.cend())
		{
			Logging::Error(name, "Already a variable with this name in this scope.");
		}

		scope[name.Lexeme] = BindState::DECLARED;
	}

	void Resolver::Define(const Token& name)
	{
		if (Scopes.empty())
		{
			return;
		}

		Scopes.back()[name.Lexeme] = BindState::DEFINED;
	}

	void Resolver::BeginScope()
	{
		Scopes.push_back({});
	}

	void Resolver::EndScope()
	{
		Scopes.pop_back();
	}

	void Resolver::Resolve(std::vector<std::shared_ptr<Stmt>> statements)
	{
		for (std::shared_ptr<Stmt> statement : statements)
		{
			Resolve(statement);
		}
	}

	void Resolver::Resolve(std::shared_ptr<Stmt> statement)
	{
		statement->Accept(*this);
	}

	void Resolver::Resolve(std::shared_ptr<Expr> expression)
	{
		expression->Accept(*this);
	}

	void Resolver::ResolveLocal(Expr& expr, const Token& name)
	{
		for (int idx = (int)Scopes.size() - 1; idx >= 0; idx--)
		{
			const auto var = Scopes[idx].find(name.Lexeme);
			if (var != Scopes[idx].cend())
			{
				LoxInterpreter.Resolve(expr, Scopes.size() - 1 - idx);
				return;
			}
		}
	}

	void Resolver::VisitBlockStmt(Block& stmt)
	{
		BeginScope();
		Resolve(stmt.Statements);
		EndScope();
	}

	void Resolver::VisitVarStmt(Var& stmt)
	{
		Declare(stmt.Name);

		if (stmt.Initialiser != nullptr)
		{
			Resolve(stmt.Initialiser);
		}

		Define(stmt.Name);
	}

	void Resolver::ResolveFunction(Function& function, const FunctionType type)
	{
		FunctionType enclosingFunction = CurrentFunction;
		CurrentFunction = type;
		BeginScope();
		for (const Token& param : function.Params)
		{
			Declare(param);
			Define(param);
		}

		Resolve(function.Body);
		EndScope();
		CurrentFunction = enclosingFunction;
	}

	void Resolver::VisitExpressionStmt(Expression& stmt)
	{
		Resolve(stmt.Subjectexpression);
	}

	void Resolver::VisitIfStmt(If& stmt)
	{
		Resolve(stmt.Condition);
		Resolve(stmt.Thenbranch);
		if (stmt.Elsebranch != nullptr) { Resolve(stmt.Elsebranch); }
	}

	void Resolver::VisitPrintStmt(Print& stmt)
	{
		Resolve(stmt.Subjectexpression);
	}

	void Resolver::VisitFunctionStmt(Function& stmt)
	{
		Declare(stmt.Name);
		Define(stmt.Name);

		ResolveFunction(stmt, FunctionType::FUNCTION);
	}

	void Resolver::VisitReturnStmt(Return& stmt)
	{
		if (CurrentFunction == FunctionType::NONE)
		{
			Logging::Error(stmt.Keyword, "Can't return from top level code.");
		}

		if (stmt.Value != nullptr)
		{
			if (CurrentFunction == FunctionType::INITIALISER)
			{
				Logging::Error(stmt.Keyword, "Can't return a value from an initialiser");
			}

			Resolve(stmt.Value); 
		}
	}

	void Resolver::VisitWhileStmt(While& stmt)
	{
		Resolve(stmt.Condition);
		Resolve(stmt.Body);
	}

	void Resolver::VisitForStmt(For& stmt)
	{
		if (stmt.Initialise != nullptr) { Resolve(stmt.Initialise); }
		if (stmt.Condition != nullptr) { Resolve(stmt.Condition); }
		if (stmt.Increment != nullptr) { Resolve(stmt.Increment); }
		Resolve(stmt.Body);
	}

	void Resolver::VisitBreakStmt(Break& stmt)
	{
		//Intentionally empty
	}

	void Resolver::VisitContinueStmt(Continue& stmt)
	{
		//Intentionally empty
	}

	void Resolver::VisitClassStmt(Class& stmt)
	{
		ClassType enclosingClass = CurrentClass;
		CurrentClass = ClassType::CLASS;

		Declare(stmt.Name);
		Define(stmt.Name);

		if (stmt.Superclass != nullptr)
		{
			if (stmt.Name.Lexeme == stmt.Superclass->Name.Lexeme)
			{
				Logging::Error(stmt.Superclass->Name, "A class can't inherit from itself");
			}

			CurrentClass = ClassType::SUBCLASS;
			Resolve(stmt.Superclass);

			BeginScope();
			Scopes.back().emplace("super", BindState::DEFINED);
		}

		BeginScope();
		Scopes.back().emplace("this", BindState::DEFINED);

		for (std::shared_ptr<Function> method : stmt.Methods)
		{
			ResolveFunction(*method, method->Name.Lexeme == "init" ? FunctionType::INITIALISER : FunctionType::METHOD);
		}

		EndScope();
		if (stmt.Superclass != nullptr) { EndScope(); }

		CurrentClass = enclosingClass;
	}

	void Resolver::VisitVariableExpr(Variable& expr)
	{
		if (!Scopes.empty())
		{
			//If we're trying to assign a variable to itself in it's own initialiser e.g. var a = a;
			//don't do that, it doesn't make sense
			const auto foundInScope = Scopes.back().find(expr.Name.Lexeme);
			if (foundInScope != Scopes.back().cend() && foundInScope->second == BindState::DECLARED)
			{
				Logging::Error(expr.Name, "Can't read local variable in it's own initialiser");
			}
		}

		ResolveLocal(expr, expr.Name);
	}

	void Resolver::VisitAssignExpr(Assign& expr)
	{
		Resolve(expr.Value);
		ResolveLocal(expr, expr.Name);
	}

	void Resolver::VisitBinaryExpr(Binary& expr)
	{
		Resolve(expr.Left);
		Resolve(expr.Right);
	}

	void Resolver::VisitCallExpr(Call& expr)
	{
		Resolve(expr.Callee);
		for (std::shared_ptr<Expr> arg : expr.Arguments)
		{
			Resolve(arg);
		}
	}

	void Resolver::VisitGroupingExpr(Grouping& expr)
	{
		Resolve(expr.Expression);
	}

	void Resolver::VisitLiteralExpr(Literal& expr)
	{
		//Deliberately empty
	}

	void Resolver::VisitLogicalExpr(Logical& expr)
	{
		Resolve(expr.Left);
		Resolve(expr.Right);
	}

	void Resolver::VisitUnaryExpr(Unary& expr)
	{
		Resolve(expr.Right);
	}

	void Resolver::VisitGetExpr(Get& expr)
	{
		Resolve(expr.Object);
	}

	void Resolver::VisitSetExpr(Set& expr)
	{
		Resolve(expr.Value);
		Resolve(expr.Object);
	}

	void Resolver::VisitThisExpr(This& expr)
	{
		if (CurrentClass == ClassType::NONE)
		{
			Logging::Error(expr.Keyword, "Can't use 'this' outside of a class");
			return;
		}

		ResolveLocal(expr, expr.Keyword);
	}

	void Resolver::VisitSuperExpr(Super& expr)
	{
		if (CurrentClass == ClassType::NONE)
		{
			Logging::Error(expr.Keyword, "Can't use 'super' outside of a class");
		}
		else if (CurrentClass != ClassType::SUBCLASS)
		{
			Logging::Error(expr.Keyword, "Can't use 'super' in a class with no superclass");
		}

		ResolveLocal(expr, expr.Keyword);
	}
}