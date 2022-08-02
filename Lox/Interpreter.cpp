#include "Interpreter.h"
#include "RuntimeError.h"
#include "Logging.h"
#include "LoxCallable.h"
#include "LoxClass.h"
#include "NativeFunctions.h"
#include <iostream>
#include <sstream>

namespace Lox
{
	using ObjPtr = std::shared_ptr<Object>;

	Interpreter::Interpreter() : 
		Obj(nullptr), Globals(std::make_shared<Environment>()), Env(Globals)
	{
		Globals->Define("clock", std::make_shared<Clock>());
	}


	void Interpreter::Interpret(std::vector<std::shared_ptr<Stmt>>& statements)
	{
		try
		{
			for (std::shared_ptr<Stmt>& statement : statements)
			{
				Execute(statement);
			}
		}
		catch (const RuntimeException& ex)
		{
			Logging::RuntimeError(ex);
		}
	}

	void Interpreter::Execute(std::shared_ptr<Stmt>& statement)
	{
		statement->Accept(*this);
	}

	void Interpreter::ExecuteBlock(std::vector<std::shared_ptr<Stmt>> statements, std::shared_ptr<Environment> environment)
	{
		std::shared_ptr<Environment> previous = Env;
		try
		{
			Env = environment;
			for (std::shared_ptr<Stmt> statement : statements)
			{
				Execute(statement);
			}
		}
		catch (const std::exception&)
		{
			Env = previous;
			throw; //We're just treating this as a 'finally' block, actually deal with the exception upwards
		}

		Env = previous;
	}

	void Interpreter::Resolve(Expr& expr, const size_t depth)
	{
		Locals.insert_or_assign(&expr, depth);
	}

	std::shared_ptr<Object> Interpreter::LookUpVariable(const Token& name, Expr& expr) const
	{
		auto iterator = Locals.find(&expr);
		if (iterator != Locals.end())
		{
			return Env->GetAt(iterator->second, name);
		}
		else
		{
			return Globals->Get(name);
		}
	}

	void Interpreter::VisitLiteralExpr(Literal& expr)
	{
		Obj = expr.Value;
	}

	void Interpreter::VisitGroupingExpr(Grouping& expr)
	{
		Obj = Evaluate(expr.Expression);
	}

	void Interpreter::VisitUnaryExpr(Unary& expr)
	{
		ObjPtr right = Evaluate(expr.Right);

		switch (expr.Op.Type)
		{
		case TokenType::BANG:
			CheckNumberOperand(expr.Op, right);
			Obj = std::make_shared<BoolObject>(!IsTruthy(right));
			break;
		case TokenType::MINUS:
			Obj = std::make_shared<NumberObject>(-ObjectAs<NumberObject>(right)->Data);
			break;
		}

		//Unreachable
		Obj = std::make_shared<NilObject>();
	}

	void Interpreter::VisitBinaryExpr(Binary& expr)
	{
		ObjPtr left = Evaluate(expr.Left);
		ObjPtr right = Evaluate(expr.Right);

		switch (expr.Op.Type)
		{
		case TokenType::MINUS:
			Obj = std::make_shared<NumberObject>(ObjectAs<NumberObject>(left)->Data - ObjectAs<NumberObject>(right)->Data);
			break;
		case TokenType::PLUS:
			if (left->GetType() == LiteralTypes::NUMBER && right->GetType() == LiteralTypes::NUMBER)
			{
				Obj = std::make_shared<NumberObject>(ObjectAs<NumberObject>(left)->Data + ObjectAs<NumberObject>(right)->Data);
			}
			else if (left->GetType() == LiteralTypes::STRING && right->GetType() == LiteralTypes::STRING)
			{
				Obj = std::make_shared<StringObject>(ObjectAs<StringObject>(left)->Data + ObjectAs<StringObject>(right)->Data);
			}
			else
			{
				throw RuntimeException(expr.Op, "Operands must be two numbers or two strings");
			}
			break;
		case TokenType::SLASH:
			CheckNumberOperands(expr.Op, left, right);
			Obj = std::make_shared<NumberObject>(ObjectAs<NumberObject>(left)->Data / ObjectAs<NumberObject>(right)->Data);
			break;
		case TokenType::STAR:
			CheckNumberOperands(expr.Op, left, right);
			Obj = std::make_shared<NumberObject>(ObjectAs<NumberObject>(left)->Data * ObjectAs<NumberObject>(right)->Data);
			break;
		case TokenType::GREATER:
			CheckNumberOperands(expr.Op, left, right);
			Obj = std::make_shared<BoolObject>(ObjectAs<NumberObject>(left)->Data > ObjectAs<NumberObject>(right)->Data);
			break;
		case TokenType::GREATER_EQUAL:
			CheckNumberOperands(expr.Op, left, right);
			Obj = std::make_shared<BoolObject>(ObjectAs<NumberObject>(left)->Data >= ObjectAs<NumberObject>(right)->Data);
			break;
		case TokenType::LESS:
			CheckNumberOperands(expr.Op, left, right);
			Obj = std::make_shared<BoolObject>(ObjectAs<NumberObject>(left)->Data < ObjectAs<NumberObject>(right)->Data);
			break;
		case TokenType::LESS_EQUAL:
			CheckNumberOperands(expr.Op, left, right);
			Obj = std::make_shared<BoolObject>(ObjectAs<NumberObject>(left)->Data <= ObjectAs<NumberObject>(right)->Data);
			break;
		case TokenType::BANG_EQUAL:
			Obj = std::make_shared<BoolObject>(!IsEqual(left, right));
			break;
		case TokenType::EQUAL_EQUAL:
			Obj = std::make_shared<BoolObject>(IsEqual(left, right));
			break;
		}
	}

	void Interpreter::VisitVariableExpr(Variable& expr)
	{
		Obj = LookUpVariable(expr.Name, expr);
	}

	void Interpreter::VisitAssignExpr(Assign& expr)
	{
		Obj = Evaluate(expr.Value);

		auto iterator = Locals.find(&expr);
		if (iterator != Locals.end())
		{
			Env->AssignAt(iterator->second, expr.Name, Obj);
		}
		else
		{
			Globals->Assign(expr.Name, Obj);
		}
	}

	void Interpreter::VisitLogicalExpr(Logical& expr)
	{
		ObjPtr left = Evaluate(expr.Left);

		if (expr.Op.Type == TokenType::OR)
		{
			if (IsTruthy(left))
			{
				Obj = left;
			}
		}
		else if(!IsTruthy(left))
		{
			Obj = left;
		}

		Obj = Evaluate(expr.Right);
	}

	void Interpreter::VisitCallExpr(Call& expr)
	{
		ObjPtr callee = Evaluate(expr.Callee);

		std::vector<ObjPtr> args;
		for (std::shared_ptr<Expr> arg : expr.Arguments)
		{
			args.emplace_back(Evaluate(arg));
		}

		std::shared_ptr<LoxCallable> function = std::static_pointer_cast<LoxCallable, Object>(callee);
		if (function == nullptr)
		{
			throw RuntimeException(expr.Paren, "Can only call functions & classes.");
		}
		if (args.size() != function->Arity())
		{
			std::stringstream ss;
			ss << "Expected " << function->Arity() << " arguments but got " << args.size() << ".";
			throw RuntimeException(expr.Paren, ss.str());
		}

		Obj = function->Call(*this, args);
	}

	void Interpreter::VisitGetExpr(Get& expr)
	{
		ObjPtr object = Evaluate(expr.Object);
		if (object->GetType() == LiteralTypes::INSTANCE)
		{
			Obj = std::reinterpret_pointer_cast<LoxInstance, Object>(object)->Get(expr.Name);
			return;
		}

		throw RuntimeException(expr.Name, "Only instances have properties");
	}

	void Interpreter::VisitSetExpr(Set& expr)
	{
		ObjPtr object = Evaluate(expr.Object);

		if (object->GetType() != LiteralTypes::INSTANCE)
		{
			throw RuntimeException(expr.Name, "Only instances have fields");
		}

		ObjPtr value = Evaluate(expr.Value);

		std::shared_ptr<LoxInstance> instance = std::reinterpret_pointer_cast<LoxInstance, Object>(object);
		instance->Set(expr.Name, value);

		Obj = value;
	}

	void Interpreter::VisitThisExpr(This& expr)
	{
		Obj = LookUpVariable(expr.Keyword, expr);
	}

	void Interpreter::VisitSuperExpr(Super& expr)
	{
		size_t distance = Locals.find(&expr)->second;
		std::shared_ptr<LoxClass> super = std::reinterpret_pointer_cast<LoxClass, Object>(Env->GetAt(distance, "super"));
		std::shared_ptr<LoxInstance> object = std::reinterpret_pointer_cast<LoxInstance, Object>(Env->GetAt(distance - 1, "this"));
		std::shared_ptr<LoxFunction> method = super->FindMethod(expr.Method.Lexeme);
		if (method == nullptr)
		{
			throw RuntimeException(expr.Method, "Undefined property '" + expr.Method.Lexeme + "'.");
		}
		else
		{
			Obj = method->Bind(object);
		}
	}

	void Interpreter::VisitExpressionStmt(Expression& stmt)
	{
		Evaluate(stmt.Subjectexpression);
	}

	void Interpreter::VisitBlockStmt(Block& stmt)
	{
		ExecuteBlock(stmt.Statements, std::make_shared<Environment>(Env));
	}

	void Interpreter::VisitPrintStmt(Print& stmt)
	{
		ObjPtr value = Evaluate(stmt.Subjectexpression);
		std::cout << value->ToString() << "\n";
	}

	void Interpreter::VisitVarStmt(Var& stmt)
	{
		ObjPtr value = nullptr;
		if (stmt.Initialiser != nullptr)
		{
			value = Evaluate(stmt.Initialiser);
		}

		Env->Define(stmt.Name.Lexeme, value);
	}

	void Interpreter::VisitIfStmt(If& stmt)
	{
		if (IsTruthy(Evaluate(stmt.Condition)))
		{
			Execute(stmt.Thenbranch);
		}
		else if (stmt.Elsebranch != nullptr)
		{
			Execute(stmt.Elsebranch);
		}
	}

	void Interpreter::VisitWhileStmt(While& stmt)
	{
		while (IsTruthy(Evaluate(stmt.Condition)))
		{
			try
			{
				Execute(stmt.Body);
			}
			catch (const FlowControl& flow)
			{
				if (flow.Control.GetType() == StmtType::Break)
				{
					break;
				}
				else if (flow.Control.GetType() == StmtType::Continue)
				{
					continue;
				}
			}
		}
	}

	void Interpreter::VisitForStmt(For& stmt)
	{
		Execute(stmt.Initialise);

		while (IsTruthy(Evaluate(stmt.Condition)))
		{
			try
			{
				Execute(stmt.Body);
				Evaluate(stmt.Increment);
			}
			catch (const FlowControl& flow)
			{
				if (flow.Control.GetType() == StmtType::Break)
				{
					break;
				}
				else if (flow.Control.GetType() == StmtType::Continue)
				{
					Evaluate(stmt.Increment);
					continue;
				}
			}
		}
	}

	void Interpreter::VisitBreakStmt(Break& stmt)
	{
		throw FlowControl(stmt);
	}
	
	void Interpreter::VisitContinueStmt(Continue& stmt)
	{
		throw FlowControl(stmt);
	}

	void Interpreter::VisitFunctionStmt(Function& stmt)
	{
		Env->Define(stmt.Name.Lexeme, std::make_shared<LoxFunction>(stmt, Env, false));
	}

	void Interpreter::VisitReturnStmt(Return& stmt)
	{
		ObjPtr value = nullptr;
		if (stmt.Value != nullptr)
		{
			value = Evaluate(stmt.Value);
		}

		throw ReturnStatement(value);
	}

	void Interpreter::VisitClassStmt(Class& stmt)
	{
		std::shared_ptr<LoxClass> superclass = nullptr;
		if (stmt.Superclass != nullptr)
		{
			ObjPtr super = Evaluate(stmt.Superclass);
			if (super->GetType() != LiteralTypes::CLASS)
			{
				throw RuntimeException(stmt.Superclass->Name, "Superclass must be a class.");
			}
			else
			{
				superclass = std::reinterpret_pointer_cast<LoxClass, Object>(super);
			}
		}

		Env->Define(stmt.Name.Lexeme, nullptr);

		if (superclass != nullptr)
		{
			Env = std::make_shared<Environment>(Env);
			Env->Define("super", superclass);
		}

		std::unordered_map<std::string, LoxFunction> methods;
		for (std::shared_ptr<Function>& method : stmt.Methods)
		{
			LoxFunction function(*method, Env, method->Name.Lexeme == "init");
			methods.emplace(method->Name.Lexeme, function);
		}

		std::shared_ptr<LoxClass> klass = std::make_shared<LoxClass>(stmt.Name.Lexeme, superclass, methods);
		if (superclass != nullptr) { Env = Env->EnclosingEnvironment(); }

		Env->Assign(stmt.Name, klass);
	}

	std::shared_ptr<Object> Interpreter::Evaluate(std::shared_ptr<Expr> expr)
	{
		expr->Accept(*this);
		return Obj;
	}

	bool Interpreter::IsTruthy(ObjPtr object) const
	{
		if (object.get() == nullptr || object->GetType() == LiteralTypes::NIL)
		{
			return false;
		}
		else if (object->GetType() == LiteralTypes::BOOL)
		{
			return ObjectAs<BoolObject>(object)->Data;
		}

		return true;
	}

	bool Interpreter::IsEqual(std::shared_ptr<Object> left, std::shared_ptr<Object> right) const
	{
		if (left->GetType() == LiteralTypes::NIL && right->GetType() == LiteralTypes::NIL)
		{
			return true;
		}


		if (left->GetType() == LiteralTypes::NIL)
		{
			return false;
		}

		return left->Equals(right.get());
	}

	void Interpreter::CheckNumberOperand(Token op, ObjPtr operand) const
	{
		if (operand->GetType() == LiteralTypes::NUMBER)
		{
			return;
		}

		throw RuntimeException(op, "Operand must be a Number.");
	}

	void Interpreter::CheckNumberOperands(Token op, ObjPtr left, ObjPtr right) const
	{
		if (left->GetType() == LiteralTypes::NUMBER && right->GetType() == LiteralTypes::NUMBER)
		{
			return;
		}

		throw RuntimeException(op, "Operand must be a Number.");
	}
}