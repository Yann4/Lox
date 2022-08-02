#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>

#include "Scanner.h"
#include "Parser.h"
#include "Logging.h"
#include "Interpreter.h"
#include "Resolver.h"

using namespace Lox;
Interpreter LoxInterpreter;

void LogTimespan(std::chrono::microseconds duration, const char* tag)
{
	std::cout << tag << " took " << duration.count() << "ms\n";
}

void Run(const char* source)
{
	auto beforeScan = std::chrono::high_resolution_clock::now();
	Scanner scanner(source);
	std::vector<Token> tokens = scanner.ScanTokens();
	auto afterScan = std::chrono::high_resolution_clock::now();

	auto beforeParse = std::chrono::high_resolution_clock::now();
	Parser parser(tokens);
	std::vector<std::shared_ptr<Stmt>> statements = parser.Parse();
	auto afterParse = std::chrono::high_resolution_clock::now();


	if (Logging::HadError()) { return; }

	auto beforeResolve = std::chrono::high_resolution_clock::now();
	Resolver resolver(LoxInterpreter, statements);
	auto afterResolve = std::chrono::high_resolution_clock::now();

	if (Logging::HadError()) { return; }

	auto beforeInterpret = std::chrono::high_resolution_clock::now();
	LoxInterpreter.Interpret(statements);
	auto afterInterpret = std::chrono::high_resolution_clock::now();

	LogTimespan(std::chrono::duration_cast<std::chrono::microseconds>(afterScan - beforeScan), "Scanning");
	LogTimespan(std::chrono::duration_cast<std::chrono::microseconds>(afterParse - beforeParse), "Parsing");
	LogTimespan(std::chrono::duration_cast<std::chrono::microseconds>(afterResolve - beforeResolve), "Resolving");
	LogTimespan(std::chrono::duration_cast<std::chrono::microseconds>(afterInterpret - beforeInterpret), "Running");
}

int RunFile(const char* path)
{
	//todo: charset?
	std::ifstream file(path);
	std::ostringstream sstr;
	sstr << file.rdbuf();
	Run(sstr.str().c_str());

	if (Logging::HadError())
	{
		return 65;
	}
	else if (Logging::HadRuntimeError())
	{
		return 70;
	}
	else
	{
		return 0;
	}
}

void RunPrompt()
{
	while (true)
	{
		std::string line;
		getline(std::cin, line);
		if (line == "\n")
		{
			break;
		}

		Run(line.c_str());
		Logging::ResetError(); //In REPL, don't kill the session just because the user made a mistake
	}
}

int main(int argc, char** argv)
{
	if (argc > 2)
	{
		std::cout << "Usage: lox [script]\n";
		return 64;
	}
	else if (argc == 2)
	{
		return RunFile(argv[1]);
	}
	else
	{
		RunPrompt();
	}

	return 0;
}