#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "memory.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif //DEBUG_PRINT_CODE

typedef struct
{
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

typedef enum
{
	PREC_NONE,
	PREC_ASSIGNMENT,
	PREC_OR,
	PREC_AND,
	PREC_EQUALITY,
	PREC_COMPARISON,
	PREC_TERM,
	PREC_FACTOR,
	PREC_UNARY,
	PREC_CALL,
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct
{
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

typedef struct
{
	Token name;
	int depth;
	bool isCaptured;
} Local;

typedef struct
{
	uint8_t index;
	bool isLocal;
} Upvalue;

typedef enum
{
	TYPE_FUNCTION,
	TYPE_INITIALISER,
	TYPE_METHOD,
	TYPE_SCRIPT
} FunctionType;

typedef struct Compiler
{
	struct Compiler* enclosing;
	ObjFunction* function;
	FunctionType type;

	Local locals[UINT8_COUNT];
	int localsCount;
	Upvalue upvalues[UINT8_COUNT];
	int scopeDepth;
} Compiler;

typedef struct ClassCompiler
{
	struct ClassCompiler* enclosing;
	bool hasSuperclass;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;
Chunk* compilingChunk;

static Chunk* CurrentChunk()
{
	return &current->function->chunk;
}

static void ErrorAt(Token* token, const char* message)
{
	parser.panicMode = true;
	fprintf_s(stderr, "[line %d] Error", token->line);

	if (token->type == TOKEN_EOF)
	{
		fprintf_s(stderr, " at end");
	}
	else if (token->type == TOKEN_ERROR)
	{
		//Nothing
	}
	else
	{
		fprintf_s(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf_s(stderr, ": %s\n", message);
	parser.hadError = true;
}

static void Error(const char* message)
{
	ErrorAt(&parser.previous, message);
}

static void ErrorAtCurrent(const char* message)
{
	ErrorAt(&parser.current, message);
}

static void Advance()
{
	parser.previous = parser.current;

	for (;;)
	{
		parser.current = ScanToken();
		if (parser.current.type != TOKEN_ERROR) { break; }

		ErrorAtCurrent(parser.current.start);
	}
}

static void Consume(TokenType type, const char* message)
{
	if (parser.current.type == type)
	{
		Advance();
		return;
	}

	ErrorAtCurrent(message);
}

static bool Check(TokenType type)
{
	return parser.current.type == type;
}

static bool Match(TokenType type)
{
	if (!Check(type))
	{
		return false;
	}

	Advance();
	return true;
}

static void EmitByte(uint8_t byte)
{
	WriteChunk(CurrentChunk(), byte, parser.previous.line);
}

static void EmitBytes(uint8_t byte1, uint8_t byte2)
{
	EmitByte(byte1);
	EmitByte(byte2);
}

static void EmitLoop(int loopStart)
{
	EmitByte(OP_LOOP);

	int offset = CurrentChunk()->count - loopStart + 2;
	if (offset > UINT16_MAX) { Error("Loop body too large"); }

	EmitBytes((offset >> 8) & 0xff, offset & 0xff);
}

static int EmitJump(uint8_t instruction)
{
	EmitByte(instruction);
	EmitBytes(0xff, 0xff); //placeholders
	return CurrentChunk()->count - 2;
}

static uint8_t MakeConstant(Value value)
{
	int constant = AddConstant(CurrentChunk(), value);
	if (constant > UINT8_MAX)
	{
		Error("Too many constants in one chunk");
		return 0;
	}

	return (uint8_t)constant;
}

static void EmitConstant(Value value)
{
	EmitBytes(OP_CONSTANT, MakeConstant(value));
}

static void EmitReturn()
{
	if (current->type == TYPE_INITIALISER)
	{
		EmitBytes(OP_GET_LOCAL, 0);
	}
	else
	{
		EmitByte(OP_NIL);
	}

	EmitByte(OP_RETURN);
}

static void PatchJump(int offset)
{
	int jump = CurrentChunk()->count - offset - 2;

	if (jump > UINT16_MAX)
	{
		Error("Too much code to jump over.");
	}

	CurrentChunk()->code[offset] = (jump >> 8) & 0xff;
	CurrentChunk()->code[offset + 1] = jump & 0xff;
}

static void InitCompiler(Compiler* compiler, FunctionType type)
{
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->localsCount = 0;
	compiler->scopeDepth = 0;
	compiler->function = NewFunction();

	current = compiler;
	if (type != TYPE_SCRIPT)
	{
		current->function->name = CopyString(parser.previous.start, parser.previous.length);
	}

	Local* local = &current->locals[current->localsCount++];
	local->depth = 0;
	local->isCaptured = false;

	if (type != TYPE_FUNCTION)
	{
		local->name.start = "this";
		local->name.length = 4;
	}
	else
	{
		local->name.start = "";
		local->name.length = 0;
	}
}

static ObjFunction* EndCompiler()
{
	EmitReturn();
	ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError)
	{
		DisassembleChunk(CurrentChunk(), function->name != NULL ? function->name->chars : "<script>");
	}
#endif //DEBUG_PRINT_CODE

	current = current->enclosing;
	return function;
}

static void BeginScope()
{
	current->scopeDepth++;
}

static void EndScope()
{
	current->scopeDepth--;

	while (current->localsCount > 0 &&
		current->locals[current->localsCount - 1].depth > current->scopeDepth)
	{
		if (current->locals[current->localsCount - 1].isCaptured)
		{
			EmitByte(OP_CLOSE_UPVAL);
		}
		else
		{
			EmitByte(OP_POP);
		}
		current->localsCount--;
	}
}

static void Statement();
static void Declaration();
static void Expression();
static ParseRule* GetRule(TokenType type);
static void ParsePrecedence(Precedence precendece);

static bool IdentifiersEqual(Token* a, Token* b)
{
	return (a->length == b->length) && memcmp(a->start, b->start, a->length) == 0;
}

static int ResolveLocal(Compiler* compiler, Token* name)
{
	for (int idx = compiler->localsCount - 1; idx >= 0; idx--)
	{
		Local* local = &compiler->locals[idx];
		if (IdentifiersEqual(local, name))
		{
			if (local->depth == -1)
			{
				Error("Can't read local variable in it's own initialiser");
			}

			return idx;
		}
	}

	return -1;
}


static uint8_t AddUpvalue(Compiler* compiler, uint8_t index, bool isLocal)
{
	int upvalueCount = compiler->function->upvalueCount;

	for (int idx = 0; idx < upvalueCount; idx++)
	{
		Upvalue* upvalue = &compiler->upvalues[idx];
		if (upvalue->index == index && upvalue->isLocal == isLocal)
		{
			return idx;
		}
	}

	if (upvalueCount == UINT8_COUNT)
	{
		Error("Too many closure variables in function.");
		return 0;
	}

	compiler->upvalues[upvalueCount].isLocal = isLocal;
	compiler->upvalues[upvalueCount].index = index;

	return compiler->function->upvalueCount++;
}

static int ResolveUpvalue(Compiler* compiler, Token* name)
{
	if (compiler->enclosing == NULL)
	{
		return -1;
	}

	int local = ResolveLocal(compiler->enclosing, name);
	if(local != -1)
	{
		compiler->enclosing->locals[local].isCaptured = true;
		return AddUpvalue(compiler, (uint8_t)local, true);
	}

	int upvalue = ResolveUpvalue(compiler->enclosing, name);
	if (upvalue != -1)
	{
		return AddUpvalue(compiler, upvalue, false);
	}

	return -1;
}

static uint8_t IdentifierConstant(Token* name)
{
	//Ensure we're re-using constants
	for (int idx = 0; idx < CurrentChunk()->constants.count; idx++)
	{
		//We can only have string identifiers, right?
		if (IS_STRING(CurrentChunk()->constants.values[idx]))
		{
			ObjString* str = AS_STRING(CurrentChunk()->constants.values[idx]);
			if (name->length == str->length && memcmp(name->start, str->chars, name->length) == 0)
			{
				return idx;
			}
		}
	}

	//This bit is what the book gave us - just assign a new constant every time we encounter an identifier,
	//even if we've already used it
	return MakeConstant(OBJ_VAL(CopyString(name->start, name->length)));
}

static void AddLocal(Token name)
{
	if (current->localsCount == UINT8_COUNT)
	{
		Error("Too many local variables in function.");
		return;
	}

	for (int idx = 0; idx < current->localsCount; idx++)
	{
		Local* local = &current->locals[idx];
		if (local->depth != -1 && local->depth < current->scopeDepth)
		{
			break;
		}

		if (IdentifiersEqual(&name, &local->name))
		{
			Error("Already a variable with this name in this scope");
		}
	}

	Local* local = &current->locals[current->localsCount++];
	local->name = name;
	local->depth = -1;
	local->isCaptured = false;
}

static void DeclareVariable()
{
	if (current->scopeDepth == 0)
	{
		return;
	}

	Token* name = &parser.previous;
	AddLocal(*name);
}

static uint8_t ParseVariable(const char* errorMessage)
{
	Consume(TOKEN_IDENTIFIER, errorMessage);

	DeclareVariable();
	if (current->scopeDepth > 0) { return 0; }
	return IdentifierConstant(&parser.previous);
}

static void MarkInitialised()
{
	if (current->scopeDepth == 0) { return; }
	current->locals[current->localsCount - 1].depth = current->scopeDepth;
}

static uint8_t ArgumentList()
{
	uint8_t count = 0;
	if (!Check(TOKEN_RIGHT_PAREN))
	{
		do
		{
			Expression();
			if (count == 255)
			{
				Error("Can't have more than 255 arguments.");
			}

			count++;
		} while (Match(TOKEN_COMMA));
	}

	Consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
	return count;
}

static void Binary(bool canAssign)
{
	TokenType operatorType = parser.previous.type;
	ParseRule* rule = GetRule(operatorType);
	ParsePrecedence((Precedence)rule->precedence + 1);

	switch (operatorType)
	{
	case TOKEN_BANG_EQUAL:		EmitBytes(OP_EQUAL, OP_NOT); break;
	case TOKEN_EQUAL_EQUAL:		EmitByte(OP_EQUAL); break;
	case TOKEN_GREATER:			EmitByte(OP_GREATER); break;
	case TOKEN_GREATER_EQUAL:	EmitBytes(OP_LESS, OP_NOT); break;
	case TOKEN_LESS:			EmitByte(OP_LESS); break;
	case TOKEN_LESS_EQUAL:		EmitBytes(OP_GREATER, OP_NOT); break;
	case TOKEN_PLUS:			EmitByte(OP_ADD); break;
	case TOKEN_MINUS:			EmitByte(OP_SUBTRACT); break;
	case TOKEN_STAR:			EmitByte(OP_MULTIPLY); break;
	case TOKEN_SLASH:			EmitByte(OP_DIVIDE); break;
	default: return; //Unreachable
	}
}

static void Call(bool canAssign)
{
	uint8_t arity = ArgumentList();
	EmitBytes(OP_CALL, arity);
}

static void Dot(bool canAssign)
{
	Consume(TOKEN_IDENTIFIER, "Expect property name after'.'.");
	uint8_t name = IdentifierConstant(&parser.previous);

	if (canAssign && Match(TOKEN_EQUAL))
	{
		Expression();
		EmitBytes(OP_SET_PROPERTY, name);
	}
	else if (Match(TOKEN_LEFT_PAREN))
	{
		uint8_t argCount = ArgumentList();
		EmitBytes(OP_INVOKE, name);
		EmitByte(argCount);
	}
	else
	{
		EmitBytes(OP_GET_PROPERTY, name);
	}
}

static void Literal(bool canAssign)
{
	switch (parser.previous.type)
	{
	case TOKEN_FALSE:	EmitByte(OP_FALSE); break;
	case TOKEN_NIL:		EmitByte(OP_NIL); break;
	case TOKEN_TRUE:	EmitByte(OP_TRUE); break;
	default:			return; //Unreachable
	}
}

static void Grouping(bool canAssign)
{
	Expression();
	Consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void Number(bool canAssign)
{
	double value = strtod(parser.previous.start, NULL);
	EmitConstant(NUMBER_VAL(value));
}

static void And(bool canAssign)
{
	int endJump = EmitJump(OP_JUMP_IF_FALSE);

	EmitByte(OP_POP);
	ParsePrecedence(PREC_AND);

	PatchJump(endJump);
}

static void Or(bool canAssign)
{
	int elseJump = EmitJump(OP_JUMP_IF_FALSE);
	int endJump = EmitJump(OP_JUMP);

	PatchJump(elseJump);
	EmitByte(OP_POP);

	ParsePrecedence(PREC_OR);
	PatchJump(endJump);
}

static void String(bool canAssign)
{
	EmitConstant(OBJ_VAL(CopyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void NamedVariable(Token name, bool canAssign)
{
	uint8_t getOp, setOp;
	int arg = ResolveLocal(current, &name);
	if (arg != -1)
	{
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else if ((arg = ResolveUpvalue(current, &name)) != -1)
	{
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	}
	else
	{
		arg = IdentifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}
	
	if (canAssign && Match(TOKEN_EQUAL))
	{
		Expression();
		EmitBytes(setOp, (uint8_t)arg);
	}
	else
	{
		EmitBytes(getOp, (uint8_t)arg);
	}
}

static void Variable(bool canAssign)
{
	NamedVariable(parser.previous, canAssign);
}

static Token SyntheticToken(const char* name)
{
	Token token;
	token.start = name;
	token.length = (int)strlen(name);
	return token;
}

static void Super(bool canAssign)
{
	if (currentClass == NULL)
	{
		Error("Can't use 'super' outside a class.");
	}
	else if (!currentClass->hasSuperclass)
	{
		Error("Can't use 'super' in a class with no superclass.");
	}
	Consume(TOKEN_DOT, "Expect '.' after 'super'.");
	Consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
	uint8_t name = IdentifierConstant(&parser.previous);

	NamedVariable(SyntheticToken("this"), false);

	if (Match(TOKEN_LEFT_PAREN))
	{
		uint8_t argCount = ArgumentList();
		NamedVariable(SyntheticToken("super"), false);
		EmitBytes(OP_SUPER_INVOKE, name);
		EmitByte(argCount);
	}
	else
	{
		NamedVariable(SyntheticToken("super"), false);
		EmitBytes(OP_GET_SUPER, name);
	}
}

static void This(bool canAssign)
{
	if (currentClass == NULL)
	{
		Error("Can't use 'this' outside of a class.");
		return;
	}

	Variable(false);
}

static void Unary(bool canAssign)
{
	TokenType operatorType = parser.previous.type;

	//compile the operand
	ParsePrecedence(PREC_UNARY);

	switch (operatorType)
	{
	case TOKEN_BANG:	EmitByte(OP_NOT); break;
	case TOKEN_MINUS:	EmitByte(OP_NEGATE); break;
	default:
		return;
	}
}

ParseRule rules[] = 
{
  [TOKEN_LEFT_PAREN]	= {Grouping, Call,   PREC_CALL},
  [TOKEN_RIGHT_PAREN]	= {Grouping, NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]	= {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]	= {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]			= {NULL,     Dot,    PREC_CALL},
  [TOKEN_MINUS]			= {Unary,    Binary, PREC_TERM},
  [TOKEN_PLUS]			= {NULL,     Binary, PREC_TERM},
  [TOKEN_SEMICOLON]		= {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]			= {NULL,     Binary, PREC_FACTOR},
  [TOKEN_STAR]			= {NULL,     Binary, PREC_FACTOR},
  [TOKEN_BANG]			= {Unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]	= {NULL,     Binary, PREC_EQUALITY},
  [TOKEN_EQUAL]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]	= {NULL,     Binary, PREC_EQUALITY},
  [TOKEN_GREATER]		= {NULL,     Binary, PREC_EQUALITY},
  [TOKEN_GREATER_EQUAL] = {NULL,     Binary, PREC_EQUALITY},
  [TOKEN_LESS]			= {NULL,     Binary, PREC_EQUALITY},
  [TOKEN_LESS_EQUAL]	= {NULL,     Binary, PREC_EQUALITY},
  [TOKEN_IDENTIFIER]	= {Variable, NULL,   PREC_NONE},
  [TOKEN_STRING]		= {String,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]		= {Number,   NULL,   PREC_NONE},
  [TOKEN_AND]			= {NULL,     And,    PREC_AND},
  [TOKEN_CLASS]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]			= {Literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]			= {Literal,  NULL,   PREC_NONE},
  [TOKEN_OR]			= {NULL,     Or,     PREC_OR},
  [TOKEN_PRINT]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]		= {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]			= {Super,    NULL,   PREC_NONE},
  [TOKEN_THIS]			= {This,     NULL,   PREC_NONE},
  [TOKEN_TRUE]			= {Literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]			= {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]			= {NULL,     NULL,   PREC_NONE},
};

static ParseRule* GetRule(TokenType type)
{
	return &rules[type];
}

static void ParsePrecedence(Precedence precedence)
{
	Advance();
	ParseFn prefixRule = GetRule(parser.previous.type)->prefix;
	if (prefixRule == NULL)
	{
		Error("Expect expression");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);

	while (precedence < GetRule(parser.current.type)->precedence)
	{
		Advance();
		ParseFn infixRule = GetRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

	if (canAssign && Match(TOKEN_EQUAL))
	{
		Error("Invalid assignment target");
	}
}

static void DefineVariable(uint8_t global)
{
	if (current->scopeDepth > 0) 
	{
		MarkInitialised();
		return;
	}

	EmitBytes(OP_DEFINE_GLOBAL, global);
}

static void Expression()
{
	ParsePrecedence(PREC_ASSIGNMENT);
}

static void VarDeclaration()
{
	uint8_t global = ParseVariable("Expect variable name");

	if (Match(TOKEN_EQUAL))
	{
		Expression();
	}
	else
	{
		EmitByte(OP_NIL);
	}

	Consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");
	DefineVariable(global);
}

static void ExpressionStatement()
{
	Expression();
	Consume(TOKEN_SEMICOLON, "Expect ';' after expression");
	EmitByte(OP_POP);
}

static void ForStatement()
{
	BeginScope();
	Consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
	//Initialiser
	if (Match(TOKEN_SEMICOLON))
	{
		//No initialiser
	}
	else if (Match(TOKEN_VAR))
	{
		VarDeclaration();
	}
	else
	{
		ExpressionStatement();
	}

	int loopStart = CurrentChunk()->count;
	//Condition
	int exitJump = -1;
	if (!Match(TOKEN_SEMICOLON))
	{
		Expression();

		Consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

		exitJump = EmitJump(OP_JUMP_IF_FALSE);
		EmitByte(OP_POP);
	}

	//Increment
	if (!Match(TOKEN_SEMICOLON))
	{
		int bodyJump = EmitJump(OP_JUMP);
		int incrementStart = CurrentChunk()->count;
		Expression();
		EmitByte(OP_POP);
		Consume(TOKEN_RIGHT_PAREN, "Expect ')' after 'for' clauses.");

		EmitLoop(loopStart);
		loopStart = incrementStart;
		PatchJump(bodyJump);
	}

	Statement();
	EmitLoop(loopStart);

	if (exitJump != -1)
	{
		PatchJump(exitJump);
		EmitByte(OP_POP);
	}

	EndScope();
}

static void IfStatement()
{
	Consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	Expression();
	Consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int thenJump = EmitJump(OP_JUMP_IF_FALSE);
	EmitByte(OP_POP);
	Statement();

	int elseJump = EmitJump(OP_JUMP);

	PatchJump(thenJump);
	EmitByte(OP_POP);

	if (Match(TOKEN_ELSE))
	{
		Statement();
	}

	PatchJump(elseJump);
}

static void PrintStatement()
{
	Expression();

	Consume(TOKEN_SEMICOLON, "Expect ';' after variable");
	EmitByte(OP_PRINT);
}

static void ReturnStatement()
{
	if (current->type == TYPE_SCRIPT)
	{
		Error("Can't return from top-level code");
	}

	if (Match(TOKEN_SEMICOLON))
	{
		EmitReturn();
	}
	else
	{
		if (current->type == TYPE_INITIALISER)
		{
			Error("Can't return a value from an initialiser.");
		}

		Expression();
		Consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
		EmitByte(OP_RETURN);
	}
}

static void WhileStatement()
{
	int loopStart = CurrentChunk()->count;
	Consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
	Expression();
	Consume(TOKEN_RIGHT_PAREN, "Expect ')' after 'while' condition.");

	int exitJump = EmitJump(OP_JUMP_IF_FALSE);
	EmitByte(OP_POP);
	Statement();
	EmitLoop(loopStart);

	PatchJump(exitJump);
	EmitByte(OP_POP);
}

static void Synchronise()
{
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF)
	{
		if (parser.previous.type == TOKEN_SEMICOLON)
		{
			return;
		}

		switch (parser.current.type)
		{
		case TOKEN_CLASS:
		case TOKEN_FUN:
		case TOKEN_VAR:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_PRINT:
		case TOKEN_RETURN:
			return;
		default:
			; //do nothing
		}

		Advance();
	}
}

static void Block()
{
	while (!Check(TOKEN_RIGHT_BRACE) && !Check(TOKEN_EOF))
	{
		Declaration();
	}

	Consume(TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void Function(FunctionType type)
{
	Compiler compiler;
	InitCompiler(&compiler, type);
	BeginScope();

	Consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
	if (!Check((TOKEN_RIGHT_PAREN)))
	{
		do 
		{
			current->function->arity++;
			if (current->function->arity > 255)
			{
				ErrorAtCurrent("Can't have more than 255 parameters.");
			}


			uint8_t constant = ParseVariable("Expect parameter name.");
			DefineVariable(constant);
		} while (Match(TOKEN_COMMA));
	}

	Consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
	Consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	Block();

	ObjFunction* function = EndCompiler();
	EmitBytes(OP_CLOSURE, MakeConstant(OBJ_VAL(function)));
	for (int idx = 0; idx < function->upvalueCount; idx++)
	{
		EmitByte(compiler.upvalues[idx].isLocal ? 1 : 0);
		EmitByte(compiler.upvalues[idx].index);
	}
}

static void Method()
{
	Consume(TOKEN_IDENTIFIER, "Expect method name.");
	uint8_t constant = IdentifierConstant(&parser.previous);

	FunctionType type = TYPE_METHOD;
	if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0)
	{
		type = TYPE_INITIALISER;
	}

	Function(type);

	EmitBytes(OP_METHOD, constant);
}

static void ClassDeclaration()
{
	Consume(TOKEN_IDENTIFIER, "Expect class name.");
	Token className = parser.previous;
	uint8_t nameConstant = IdentifierConstant(&parser.previous);
	DeclareVariable();

	EmitBytes(OP_CLASS, nameConstant);
	DefineVariable(nameConstant);

	ClassCompiler classCompiler;
	classCompiler.enclosing = currentClass;
	classCompiler.hasSuperclass = false;
	currentClass = &classCompiler;

	if (Match(TOKEN_LESS))
	{
		Consume(TOKEN_IDENTIFIER, "Expect superclass name.");
		Variable(false);

		if (IdentifiersEqual(&className, &parser.previous))
		{
			Error("A class cannot inherit from itself");
		}

		BeginScope();
		AddLocal(SyntheticToken("super"));
		DefineVariable(0);

		NamedVariable(className, false);
		EmitByte(OP_INHERIT);
		classCompiler.hasSuperclass = true;
	}

	NamedVariable(className, false);
	Consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
	while (!Check(TOKEN_RIGHT_BRACE) && !Check(TOKEN_EOF))
	{
		Method();
	}

	Consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
	EmitByte(OP_POP);

	if (currentClass->hasSuperclass)
	{
		EndScope();
	}

	currentClass = currentClass->enclosing;
}

static void FunDeclaration()
{
	uint8_t global = ParseVariable("Expect function name");
	MarkInitialised();
	Function(TYPE_FUNCTION);
	DefineVariable(global);
}

static void Declaration()
{
	if (Match(TOKEN_CLASS))
	{
		ClassDeclaration();
	}
	else if (Match(TOKEN_FUN))
	{
		FunDeclaration();
	}
	else if (Match(TOKEN_VAR))
	{
		VarDeclaration();
	}
	else
	{
		Statement();
	}

	if(parser.panicMode)
	{
		Synchronise();
	}
}

static void Statement()
{
	if (Match(TOKEN_PRINT))
	{
		PrintStatement();
	}
	else if (Match(TOKEN_FOR))
	{
		ForStatement();
	}
	else if (Match(TOKEN_IF))
	{
		IfStatement();
	}
	else if (Match(TOKEN_RETURN))
	{
		ReturnStatement();
	}
	else if (Match(TOKEN_WHILE))
	{
		WhileStatement();
	}
	else if (Match(TOKEN_LEFT_BRACE))
	{
		BeginScope();
		Block();
		EndScope();
	}
	else
	{
		ExpressionStatement();
	}
}

ObjFunction* Compile(const char* source)
{
	InitScanner(source);
	Compiler compiler;
	InitCompiler(&compiler, TYPE_SCRIPT);

	parser.hadError = false;
	parser.panicMode = false;

	Advance();

	while (!Match(TOKEN_EOF))
	{
		Declaration();
	}

	ObjFunction* function = EndCompiler();
	return parser.hadError ? NULL : function;
}

void MarkCompilerRoots()
{
	Compiler* compiler = current;
	while (compiler != NULL)
	{
		MarkObject((Obj*)compiler->function);
		compiler = compiler->enclosing;
	}
}