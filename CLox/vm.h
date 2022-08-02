#ifndef clox_vm_h
#define clox_vm_h
#include "object.h"
#include "value.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
	ObjClosure* closure;
	uint8_t* ip;
	Value* slots;
} CallFrame;

typedef struct
{
	CallFrame frames[FRAMES_MAX];
	int frameCount;
	Value stack[STACK_MAX];
	Value* stackTop;
	Table strings;
	ObjString* initString;
	ObjUpvalue* openUpvalues;
	Table globals;
	Obj* objects;

	size_t bytesAllocated;
	size_t nextGC;
	int greyCount;
	int greyCapacity;
	Obj** greyStack;
} VM;

typedef enum
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void InitVM();
void FreeVM();
void Push(Value value);
Value Pop(int n);
Value* Peek();

InterpretResult Interpret(const char* source);
#endif
