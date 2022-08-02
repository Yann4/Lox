#include "Parser.h"
#include "Logging.h"

namespace Lox
{
	Parser::Parser(std::vector<Token> tokens) : Tokens(tokens), Current(0)
	{ }

	std::vector<std::shared_ptr<Stmt>> Parser::Parse()
	{
		std::vector<std::shared_ptr<Stmt>> statements;
		while (!IsAtEnd())
		{
			statements.emplace_back(Declaration());
		}

		return statements;
	}

	std::shared_ptr<Expr> Parser::ParseExpression()
	{
		return ParseAssignment();
	}

	std::shared_ptr<Expr> Parser::ParseAssignment()
	{
		std::shared_ptr<Expr> expr = ParseOr();

		if (Match({ TokenType::EQUAL }))
		{
			Token equals = Previous();
			std::shared_ptr<Expr> value = ParseAssignment();

			if (expr->GetType() == ExprType::Variable)
			{
				Token name = std::reinterpret_pointer_cast<Variable, Expr>(expr)->Name;
				return std::make_shared<Assign>(name, value);
			}
			else if (expr->GetType() == ExprType::Get)
			{
				std::shared_ptr<Get> get = std::reinterpret_pointer_cast<Get, Expr>(expr);
				return std::make_shared<Set>(get->Object, get->Name, value);
			}

			Error(equals, "Invalid assignment target.");
		}

		return expr;
	}

	std::shared_ptr<Expr> Parser::ParseOr()
	{
		std::shared_ptr<Expr> expr = ParseAnd();

		while (Match({ TokenType::OR }))
		{
			Token op = Previous();
			std::shared_ptr<Expr> right = ParseAnd();
			expr = std::make_shared<Logical>(expr, op, right);
		}

		return expr;
	}

	std::shared_ptr<Expr> Parser::ParseAnd()
	{
		std::shared_ptr<Expr> expr = ParseEquality();

		while (Match({ TokenType::AND }))
		{
			Token op = Previous();
			std::shared_ptr<Expr> right = ParseEquality();
			expr = std::make_shared<Logical>(expr, op, right);
		}

		return expr;
	}

	std::shared_ptr<Expr> Parser::ParseEquality()
	{
		std::shared_ptr<Expr> expr = ParseComparison();

		while (Match({ TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL }))
		{
			Token op = Previous();
			shared_ptr<Expr> right = ParseComparison();
			expr = std::make_shared<Binary>(expr, op, right);
		}

		return expr;
	}

	std::shared_ptr<Expr> Parser::ParseComparison()
	{
		std::shared_ptr<Expr> expr = ParseTerm();

		while (Match({ TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL }))
		{
			Token op = Previous();
			std::shared_ptr<Expr> right = ParseTerm();
			expr = std::make_shared<Binary>(expr, op, right);
		}

		return expr;
	}

	std::shared_ptr<Expr> Parser::ParseTerm()
	{
		std::shared_ptr<Expr> expr = ParseFactor();

		while (Match({ TokenType::MINUS, TokenType::PLUS }))
		{
			Token op = Previous();
			std::shared_ptr<Expr> right = ParseFactor();
			expr = std::make_shared<Binary>(expr, op, right);
		}

		return expr;
	}

	std::shared_ptr<Expr> Parser::ParseFactor()
	{
		std::shared_ptr<Expr> expr = ParseUnary();

		while (Match({ TokenType::SLASH, TokenType::STAR }))
		{
			Token op = Previous();
			std::shared_ptr<Expr> right = ParseUnary();
			expr = std::make_shared<Binary>(expr, op, right);
		}

		return expr;
	}

	std::shared_ptr<Expr> Parser::ParseUnary()
	{
		if (Match({ TokenType::BANG, TokenType::MINUS }))
		{
			Token op = Previous();
			std::shared_ptr<Expr> right = ParseUnary();
			return std::make_shared<Unary>(op, right);
		}

		return ParseCall();
	}

	std::shared_ptr<Expr> Parser::ParseCall()
	{
		std::shared_ptr<Expr> expr = ParsePrimary();

		while (true)
		{
			if (Match({ TokenType::LEFT_PAREN }))
			{
				expr = FinishCall(expr);
			}
			else if (Match({ TokenType::DOT }))
			{
				Token name = Consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
				expr = std::make_shared<Get>(expr, name);
			}
			else
			{
				break;
			}
		}

		return expr;
	}

	std::shared_ptr<Expr> Parser::FinishCall(std::shared_ptr<Expr> callee)
	{
		std::vector<std::shared_ptr<Expr>> arguments;
		if (!Check(TokenType::RIGHT_PAREN))
		{
			do
			{
				if (arguments.size() >= 255)
				{
					Error(Peek(), "Can't have more than 255 arguments");
				}

				arguments.emplace_back(ParseExpression());
			} while (Match({ TokenType::COMMA }));
		}

		Token paren = Consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
		return std::make_shared<Call>(callee, paren, arguments);
	}

	std::shared_ptr<Expr> Parser::ParsePrimary()
	{
		if (Match({ TokenType::FALSE })) { return std::make_shared<Literal>(std::make_shared<BoolObject>(false)); }
		if (Match({ TokenType::TRUE })) { return std::make_shared<Literal>(std::make_shared<BoolObject>(true)); }
		if (Match({ TokenType::NIL })) { return std::make_shared<Literal>(std::make_shared<NilObject>()); }
		if (Match({ TokenType::NUMBER, TokenType::STRING })) { return std::make_shared<Literal>(Previous().Literal); }

		if (Match({ TokenType::SUPER }))
		{
			Token keyword = Previous();
			Consume(TokenType::DOT, "Expect '.' after 'super'.");
			Token method = Consume(TokenType::IDENTIFIER, "Expect superclass method name");
			return std::make_shared<Super>(keyword, method);
		}

		if (Match({ TokenType::THIS })) { return std::make_shared<This>(Previous()); }
		if (Match({ TokenType::IDENTIFIER })) { return std::make_shared<Variable>(Previous()); }

		if (Match({ TokenType::LEFT_PAREN }))
		{
			std::shared_ptr<Expr> expr = ParseExpression();
			Consume(TokenType::RIGHT_PAREN, "Expect ')' after expression");
			return std::make_shared<Grouping>(expr);
		}

		throw Error(Peek(), "Expect expression");
	}

	std::shared_ptr<Stmt> Parser::Declaration()
	{
		try
		{
			if (Match({ TokenType::CLASS }))
			{
				return ClassDeclaration();
			}

			if (Match({ TokenType::FUN }))
			{
				return ParseFunction("function");
			}

			if (Match({ TokenType::VAR }))
			{
				return VarDeclaration();
			}

			return Statement();
		}
		catch (const ParseError&)
		{
			Synchronise();
			return nullptr;
		}
	}

	std::shared_ptr<Stmt> Parser::Statement()
	{
		if (Match({ TokenType::BREAK }))
		{
			return BreakStatement();
		}

		if (Match({ TokenType::CONTINUE }))
		{
			return ContinueStatement();
		}

		if (Match({ TokenType::FOR }))
		{
			return ForStatement();
		}

		if (Match({ TokenType::IF }))
		{
			return IfStatement();
		}

		if (Match({ TokenType::PRINT }))
		{
			return PrintStatement();
		}

		if (Match({ TokenType::RETURN }))
		{
			return ReturnStatement();
		}

		if (Match({ TokenType::WHILE }))
		{
			return WhileStatement();
		}

		if (Match({ TokenType::LEFT_BRACE }))
		{
			return std::make_shared<Block>(ParseBlock());
		}

		return ExpressionStatement();
	}

	std::shared_ptr<Function> Parser::ParseFunction(const char* kind)
	{
		std::stringstream ss;
		ss << "Expect " << kind << " name.";
		Token name = Consume(TokenType::IDENTIFIER, ss.str().c_str());

		ss.str(std::string());
		ss.clear();
		ss << "Expect '(' after " << kind << " name.";
		Consume(TokenType::LEFT_PAREN, ss.str().c_str());

		std::vector<Token> params;
		if (!Check(TokenType::RIGHT_PAREN))
		{
			do
			{
				if (params.size() >= 255)
				{
					Error(Peek(), "Can't have more than 255 parameters.");
				}

				params.emplace_back(Consume(TokenType::IDENTIFIER, "Expect parameter name"));
			} while (Match({ TokenType::COMMA }));
		}

		Consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters");
		Consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");
		std::vector<std::shared_ptr<Stmt>> body = ParseBlock();
		return std::make_shared<Function>(name, params, body);
	}

	std::shared_ptr<Stmt> Parser::VarDeclaration()
	{
		Token name = Consume(TokenType::IDENTIFIER, "Expect variable name");

		std::shared_ptr<Expr> initialiser = nullptr;
		if (Match({ TokenType::EQUAL }))
		{
			initialiser = ParseExpression();
		}

		Consume(TokenType::SEMICOLON, "Expect ';' after variable declaration");
		return std::make_shared<Var>(name, initialiser);
	}

	std::shared_ptr<Stmt> Parser::ClassDeclaration()
	{
		Token name = Consume(TokenType::IDENTIFIER, "Expect class name.");

		std::shared_ptr<Variable> superClass = nullptr;
		if (Match({ TokenType::LESS }))
		{
			Consume(TokenType::IDENTIFIER, "Expect superclass name.");
			superClass = std::make_shared<Variable>(Previous());
		}

		Consume(TokenType::LEFT_BRACE, "Expect '{' before class body.");

		std::vector<std::shared_ptr<Function>> methods;
		while (!Check(TokenType::RIGHT_BRACE) && !IsAtEnd())
		{
			methods.emplace_back(ParseFunction("method"));
		}

		Consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
		return std::make_shared<Class>(name, superClass, methods);
	}

	std::shared_ptr<Stmt> Parser::PrintStatement()
	{
		std::shared_ptr<Expr> value = ParseExpression();
		Consume(TokenType::SEMICOLON, "Expect ';' after value");
		return std::make_shared<Print>(value);
	}

	std::shared_ptr<Stmt> Parser::ReturnStatement()
	{
		Token keyword = Previous();
		std::shared_ptr<Expr> value = nullptr;
		if (!Check(TokenType::SEMICOLON))
		{
			value = ParseExpression();
		}

		Consume(TokenType::SEMICOLON, "Expect ';' after return value");
		return std::make_shared<Return>(keyword, value);
	}

	std::shared_ptr<Stmt> Parser::WhileStatement()
	{
		LoopDepth++;
		Consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
		std::shared_ptr<Expr> condition = ParseExpression();
		Consume(TokenType::RIGHT_PAREN, "Expect ')' after 'while' condition.");

		std::shared_ptr<Stmt> body = Statement();
		LoopDepth--;
		return std::make_shared<While>(condition, body);
	}

	std::shared_ptr<Stmt> Parser::ForStatement()
	{
		LoopDepth++;

		Consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");

		std::shared_ptr<Stmt> initialiser;
		if (Match({ TokenType::SEMICOLON }))
		{
			initialiser = nullptr;
		}
		else if (Match({ TokenType::VAR }))
		{
			initialiser = VarDeclaration();
		}
		else
		{
			initialiser = ExpressionStatement();
		}

		std::shared_ptr<Expr> condition = nullptr;
		if (!Check({ TokenType::SEMICOLON }))
		{
			condition = ParseExpression();
		}

		Consume(TokenType::SEMICOLON, "Expect ';' after loop condition.");

		std::shared_ptr<Expr> increment = nullptr;
		if (!Check(TokenType::RIGHT_PAREN))
		{
			increment = ParseExpression();
		}

		Consume(TokenType::RIGHT_PAREN, "Expect ')' after 'for' clauses");

		std::shared_ptr<Stmt> body = Statement();

		if (condition == nullptr)
		{
			condition = std::make_shared<Literal>(std::make_shared<BoolObject>(true));
		}

		LoopDepth--;
		return std::make_shared<For>(initialiser, condition, increment, body);
	}

	std::shared_ptr<Stmt> Parser::IfStatement()
	{
		Consume(TokenType::LEFT_PAREN, "Expect '(' after if.");
		std::shared_ptr<Expr> condition = ParseExpression();
		Consume(TokenType::RIGHT_PAREN, "Expect ')' after if condition.");

		std::shared_ptr<Stmt> thenBranch = Statement();
		std::shared_ptr<Stmt> elseBranch = nullptr;
		if (Match({ TokenType::ELSE }))
		{
			elseBranch = Statement();
		}

		return std::make_shared<If>(condition, thenBranch, elseBranch);
	}

	std::shared_ptr<Stmt> Parser::BreakStatement()
	{
		if (LoopDepth == 0)
		{
			throw Error(Previous(), "Expect enclosing 'while' or 'for' loop with 'break'");
		}

		Consume(TokenType::SEMICOLON, "Expect ';' after value");
		return std::make_shared<Break>(Previous());
	}

	std::shared_ptr<Stmt> Parser::ContinueStatement()
	{
		if (LoopDepth == 0)
		{
			throw Error(Previous(), "Expect enclosing 'while' or 'for' loop with 'Continue'");
		}

		Consume(TokenType::SEMICOLON, "Expect ';' after value");
		return std::make_shared<Continue>(Previous());
	}

	std::shared_ptr<Stmt> Parser::ExpressionStatement()
	{
		std::shared_ptr<Expr> expression = ParseExpression();
		Consume(TokenType::SEMICOLON, "Expect ';' after value");
		return std::make_shared<Expression>(expression);
	}

	std::vector<std::shared_ptr<Stmt>> Parser::ParseBlock()
	{
		std::vector<std::shared_ptr<Stmt>> statements;
		while (!Check({ TokenType::RIGHT_BRACE }) && !IsAtEnd())
		{
			statements.emplace_back(Declaration());
		}

		Consume(TokenType::RIGHT_BRACE, "Expect '}' after block");
		return statements;
	}

	bool Parser::Match(std::list<TokenType>&& types)
	{
		for (const TokenType& type : types)
		{
			if (Check(type))
			{
				Advance();
				return true;
			}
		}

		return false;
	}

	const Token& Parser::Advance()
	{
		if (!IsAtEnd())
		{
			Current++;
		}

		return Previous();
	}

	const Token& Parser::Consume(TokenType type, const char* message)
	{
		if (Check(type))
		{
			return Advance();
		}

		throw Error(Peek(), message);
	}

	void Parser::Synchronise()
	{
		Advance();

		//We want to discard tokens until we're at the boundary of the next statement
		//Cases here are what we use to define a statement boundary
		while (!IsAtEnd())
		{
			if (Previous().Type == TokenType::SEMICOLON)
			{
				return;
			}

			switch (Peek().Type)
			{
			case TokenType::CLASS:
			case TokenType::FUN:
			case TokenType::VAR:
			case TokenType::FOR:
			case TokenType::IF:
			case TokenType::WHILE:
			case TokenType::PRINT:
			case TokenType::RETURN:
				return;
			}

			Advance();
		}
	}

	Parser::ParseError Parser::Error(Token token, const char* message)
	{
		Logging::Error(token, message);
		return Parser::ParseError(token, message);
	}
}