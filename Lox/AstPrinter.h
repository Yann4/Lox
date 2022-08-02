#pragma once

#include <string>
#include <list>

#include "Expr.h"

namespace Lox
{
	class AstPrinter : ExprVisitor
	{
	public:
		std::string Print(Expr& expr);

		void VisitBinaryExpr(Binary& expr) override;
		void VisitGroupingExpr(Grouping& expr) override;
		void VisitLiteralExpr(Literal& expr) override;
		void VisitUnaryExpr(Unary& expr) override;
		void VisitVariableExpr(Variable& expr) override;

	private:
		void Parenthesize(std::string name, std::list<Expr*>&& exprs);

		std::stringstream ss;
	};
}