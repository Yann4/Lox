#include <sstream>
#include "AstPrinter.h"

namespace Lox
{
	std::string AstPrinter::Print(Expr& expr)
	{
		expr.Accept(*this);
		return ss.str();
	}

	void AstPrinter::VisitBinaryExpr(Binary& expr)
	{
		return Parenthesize(expr.Op.Lexeme, { expr.Left.get(), expr.Right.get() });
	}

	void AstPrinter::VisitGroupingExpr(Grouping& expr)
	{
		return Parenthesize("group", { expr.Expression.get() });
	}

	void AstPrinter::VisitLiteralExpr(Literal& expr)
	{
		if (expr.Value->GetType() == LiteralTypes::NIL)
		{
			ss << "nil";
		}

		ss << expr.Value->ToString();
	}

	void AstPrinter::VisitUnaryExpr(Unary& expr)
	{
		Parenthesize(expr.Op.Lexeme, { expr.Right.get() });
	}

	void AstPrinter::VisitVariableExpr(Variable& expr)
	{
		ss << "var " << expr.Name.Lexeme;
	}

	void AstPrinter::Parenthesize(std::string name, std::list<Expr*>&& exprs)
	{
		ss << "(" << name;
		for (Expr* expr : exprs)
		{
			ss << " ";
			expr->Accept(*this);
		}

		ss << ")";
	}
}