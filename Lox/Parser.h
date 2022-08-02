#pragma once

#include <vector>
#include <list>
#include <memory>

#include "Token.h"
#include "Expr.h"
#include "Stmt.h"

namespace Lox
{
	class Parser
	{
	private:
		struct ParseError : public std::exception
		{
		public:
			ParseError(Token token, const char* message) : ErrorToken(token), Message(message) {}

			const char* what() const throw()
			{
				return Message;
			}

		private:
			Token ErrorToken;
			const char* Message;
		};

	public:
		Parser(std::vector<Token> tokens);
		std::vector<std::shared_ptr<Stmt>> Parse();

	private:
		std::shared_ptr<Expr> ParseExpression();
		std::shared_ptr<Expr> ParseAssignment();
		std::shared_ptr<Expr> ParseOr();
		std::shared_ptr<Expr> ParseAnd();
		std::shared_ptr<Expr> ParseEquality();
		std::shared_ptr<Expr> ParseComparison();
		std::shared_ptr<Expr> ParseTerm();
		std::shared_ptr<Expr> ParseFactor();
		std::shared_ptr<Expr> ParseUnary();
		std::shared_ptr<Expr> ParseCall();
		std::shared_ptr<Expr> FinishCall(std::shared_ptr<Expr> callee);
		std::shared_ptr<Expr> ParsePrimary();

		std::shared_ptr<Stmt> Statement();
		std::shared_ptr<Stmt> Declaration();
		std::shared_ptr<Function> ParseFunction(const char* kind);
		std::shared_ptr<Stmt> VarDeclaration();
		std::shared_ptr<Stmt> ClassDeclaration();
		std::shared_ptr<Stmt> PrintStatement();
		std::shared_ptr<Stmt> ReturnStatement();
		std::shared_ptr<Stmt> WhileStatement();
		std::shared_ptr<Stmt> ForStatement();
		std::shared_ptr<Stmt> IfStatement();
		std::shared_ptr<Stmt> ExpressionStatement();
		std::shared_ptr<Stmt> BreakStatement();
		std::shared_ptr<Stmt> ContinueStatement();

		std::vector<std::shared_ptr<Stmt>> ParseBlock();

		bool Match(std::list<TokenType>&& types);

		const Token& Advance();
		const Token& Peek() const { return Tokens[Current]; }
		const Token& Previous() const { return Tokens[Current - 1]; }
		bool Check(TokenType type) const { return IsAtEnd() ? false : Peek().Type == type; }
		bool IsAtEnd() const { return Peek().Type == TokenType::END_OF_FILE; }

		const Token& Consume(TokenType type, const char* message);
		ParseError Error(Token token, const char* message);
		void Synchronise();
	private:
		std::vector<Token> Tokens;
		size_t Current = 0;

		size_t LoopDepth = 0;
	};
}