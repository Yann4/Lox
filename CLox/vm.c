#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "vm.h"
#include "compiler.h"
#include "object.h"
#include "memory.h"
#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif //DEBUG_TRACE_EXECUTION

VM vm;

static Value NAT_clock(int argCount, Value* args)
{
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void ResetStack()
{
	vm.stackTop = vm.stack;
	vm.frameCount = 0;
	vm.openUpvalues = NULL;
}

static void RuntimeError(uint8_t* ip, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf_s(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	for (int idx = vm.frameCount - 1; idx >= 0; idx--)
	{
		CallFrame* frame = &vm.frames[idx];
		ObjFunction* function = frame->closure->function;
		size_t instruction = ip - function->chunk.code - 1;
		int line = GetLine(&frame->closure->function->chunk, (int)instruction);

		fprintf_s(stderr, "[line %d] in ", line);
		if (function->name == NULL)
		{
			fprintf_s(stderr, "script\n");
		}
		else
		{
			fprintf_s(stderr, "%s()\n", function->name->chars);
		}
	}

	ResetStack();
}

static void DefineNative(const char* name, NativeFn function)
{
	Push(OBJ_VAL(CopyString(name, (int)(strlen(name)))));
	Push(OBJ_VAL(NewNative(function)));
	TableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
	Pop(1);
	Pop(1);
}

void InitVM()
{
	ResetStack();
	vm.bytesAllocated = 0;
	vm.nextGC = 1024 * 1024;
	vm.greyCount = 0;
	vm.greyCapacity = 0;
	vm.greyStack = NULL;

	vm.objects = NULL;
	InitTable(&vm.strings);

	vm.initString = NULL;
	vm.initString = CopyString("init", 4);

	InitTable(&vm.globals);

	DefineNative("clock", NAT_clock);
}

void FreeVM()
{
	FreeTable(&vm.globals);
	FreeTable(&vm.strings);
	vm.initString = NULL;
	FreeObjects();
}

void Push(Value value)
{
	*vm.stackTop = value;
	vm.stackTop++;
}

Value Pop(int n)
{
	vm.stackTop -= n;
	return *vm.stackTop;
}

Value* Peek(int distance)
{
	if (vm.stack == vm.stackTop)
	{
		return NULL;
	}

	return (vm.stackTop - 1 - distance);
}

static bool Call(ObjClosure* closure, uint8_t argCount)
{
	if (argCount != closure->function->arity)
	{
		RuntimeError(vm.frames[vm.frameCount - 1].ip, "Expected %d arguments but got %d", closure->function->arity, argCount);
		return false;
	}

	if (vm.frameCount == FRAMES_MAX)
	{
		RuntimeError(vm.frames[vm.frameCount - 1].ip, "Stack overflow");
		return false;
	}

	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = vm.stackTop - argCount - 1;

	return true;
}

static bool CallValue(Value callee, uint8_t argCount, uint8_t* currentIp, bool* changesFrame)
{
	if (IS_OBJ(callee))
	{
		switch (OBJ_TYPE(callee))
		{
		case OBJ_BOUND_METHOD:
			*changesFrame = true;
			vm.frames[vm.frameCount - 1].ip = currentIp;
			ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
			vm.stackTop[-argCount - 1] = bound->receiver; //Assign "this" pointer
			return Call(bound->method, argCount);
		case OBJ_CLASS:
		{
			ObjClass* klass = AS_CLASS(callee);
			vm.stackTop[-argCount - 1] = OBJ_VAL(NewInstance(klass));
			Value initialiser;
			if (TableGet(&klass->methods, vm.initString, &initialiser))
			{
				*changesFrame = true;
				vm.frames[vm.frameCount - 1].ip = currentIp;
				return Call(AS_CLOSURE(initialiser), argCount);
			}
			else if (argCount != 0)
			{
				RuntimeError(currentIp, "Expected 0 arguments but got %d.", argCount);
				return false;
			}

			return true;
		}
		case OBJ_CLOSURE:
			*changesFrame = true;
			vm.frames[vm.frameCount - 1].ip = currentIp;
			return Call(AS_CLOSURE(callee), argCount);
		case OBJ_NATIVE:
		{
			NativeFn native = AS_NATIVE(callee);
			Value result = native(argCount, vm.stackTop - argCount);
			vm.stackTop -= argCount + 1;
			Push(result);
			return true;
		}
		default:
			break;
		}
	}

	RuntimeError(currentIp, "Can only call functions and classes");
	return false;
}

static bool InvokeFromClass(ObjClass* klass, ObjString* name, int argcount, uint8_t* currentIP)
{
	Value method;
	if (!TableGet(&klass->methods, name, &method))
	{
		RuntimeError(currentIP, "Undefined property '%s'.", name->chars);
		return false;
	}

	vm.frames[vm.frameCount - 1].ip = currentIP;
	return Call(AS_CLOSURE(method), argcount);
}

static bool Invoke(ObjString* name, int argCount, uint8_t* currentIp, bool* changesFrame)
{
	Value receiver = *Peek(argCount);
	if (!IS_INSTANCE(receiver))
	{
		RuntimeError(currentIp, "Only instances have methods.");
		return false;
	}

	ObjInstance* instance = AS_INSTANCE(receiver);
	Value value;
	if (TableGet(&instance->fields, name, &value))
	{
		vm.stackTop[-argCount - 1] = value;
		return CallValue(value, argCount, currentIp, &changesFrame);
	}

	*changesFrame = true;
	return InvokeFromClass(instance->klass, name, argCount, currentIp);
}

static bool BindMethod(ObjClass* klass, ObjString* name)
{
	Value method;
	if (!TableGet(&klass->methods, name, &method))
	{
		RuntimeError("Undefined property '%s'.", name->chars);
		return false;
	}

	ObjBoundMethod* bound = NewBoundMethod(*Peek(0), AS_CLOSURE(method));
	Pop(0);
	Push(OBJ_VAL(bound));
	return true;
}

static ObjUpvalue* CaptureUpvalue(Value* local)
{
	ObjUpvalue* prevUpvalue = NULL;
	ObjUpvalue* upvalue = vm.openUpvalues;
	while (upvalue != NULL && upvalue->location > local)
	{
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}

	if (upvalue != NULL && upvalue->location == local)
	{
		return upvalue;
	}

	ObjUpvalue* createdUpvalue = NewUpvalue(local);
	createdUpvalue->next = upvalue;
	if (prevUpvalue == NULL)
	{
		vm.openUpvalues = createdUpvalue;
	}
	else
	{
		prevUpvalue->next = createdUpvalue;
	}

	return createdUpvalue;
}

static void CloseUpvalues(Value* last)
{
	while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last)
	{
		ObjUpvalue* upvalue = vm.openUpvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm.openUpvalues = upvalue->next;
	}
}

static void DefineMethod(ObjString* name)
{
	Value method = *Peek(0);
	ObjClass* klass = AS_CLASS(*Peek(1));
	TableSet(&klass->methods, name, method);
	Pop(1);
}

static bool IsFalsey(Value value)
{
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void Concatenate()
{
	ObjString* b = AS_STRING(*Peek(0));
	ObjString* a = AS_STRING(*Peek(1));

	int length = a->length + b->length;
	char* chars = ALLOCATE(char, (rsize_t)length + 1);
	memcpy_s(chars, (rsize_t)length + 1, a->chars, a->length);
	memcpy_s(chars + a->length, (rsize_t)length + 1, b->chars, b->length);
	chars[length] = '\0';

	ObjString* result = TakeString(chars, length);
	Pop(2);
	Push(OBJ_VAL(result));
}

static InterpretResult Run()
{
	CallFrame* frame = &vm.frames[vm.frameCount - 1];
	register uint8_t* ip = frame->ip;

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() (AS_STRING(READ_CONSTANT()))
#define BINARY_OP(valueType, op) \
	do {\
		if(!IS_NUMBER(*Peek(0)) || !IS_NUMBER(*Peek(1))) { \
			RuntimeError(ip, "Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		} \
		double b = AS_NUMBER(Pop(1)); \
		double a = AS_NUMBER(Pop(1)); \
		Push(valueType(a op b)); \
	} while(false)

#ifdef DEBUG_TRACE_EXECUTION
	printf_s("\n\n");
#endif //DEBUG_TRACE_EXECUTION

	for (;;)
	{
#ifdef DEBUG_TRACE_EXECUTION
		printf_s("          ");
		for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
		{
			printf_s("[ ");
			PrintValue(*slot);
			printf_s(" ]");
		}
		printf("\n");

		DisassembleInstruction(&frame->closure->function->chunk, (int)(ip - frame->closure->function->chunk.code));
#endif //DEBUG_TRACE_EXECUTION
		uint8_t instruction;
		switch (instruction = READ_BYTE())
		{
		case OP_CONSTANT:
		{
			Value constant = READ_CONSTANT();
			Push(constant);
			break;
		}
		case OP_NIL:		Push(NIL_VAL); break;
		case OP_TRUE:		Push(BOOL_VAL(true)); break;
		case OP_FALSE:		Push(BOOL_VAL(false)); break;
		case OP_POP:		Pop(1); break;
		case OP_POPN:		Pop(READ_BYTE()); break;
		case OP_GET_LOCAL:
		{
			uint8_t slot = READ_BYTE();
			Push(frame->slots[slot]);
			break;
		}
		case OP_SET_LOCAL:
			uint8_t slot = READ_BYTE();
			frame->slots[slot] = *Peek(0);
			break;
		case OP_GET_GLOBAL:
		{
			ObjString* name = READ_STRING();
			Value val;
			if (!TableGet(&vm.globals, name, &val))
			{
				RuntimeError(ip, "Undefined global variable '%s'.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}

			Push(val);
			break;
		}
		case OP_DEFINE_GLOBAL:
		{
			ObjString* name = READ_STRING();
			TableSet(&vm.globals, name, *Peek(0));
			Pop(1);
			break;
		}
		case OP_SET_GLOBAL:
		{
			ObjString* name = READ_STRING();
			if (TableSet(&vm.globals, name, *Peek(0)))
			{
				TableDelete(&vm.globals, name);
				RuntimeError(ip, "Undefined global variable '%s'.", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}
			break;

		}
		case OP_GET_UPVALUE:
		{
			uint8_t slot = READ_BYTE();
			Push(*frame->closure->upvalues[slot]->location);
			break;
		}
		case OP_SET_UPVALUE:
		{
			uint8_t slot = READ_BYTE();
			*frame->closure->upvalues[slot]->location = *Peek(0);
			break;
		}
		case OP_GET_PROPERTY:
		{
			if (!IS_INSTANCE(*Peek(0)))
			{
				RuntimeError(ip, "Only instances have properties.");
				return INTERPRET_RUNTIME_ERROR;
			}

			ObjInstance* instance = AS_INSTANCE(*Peek(0));
			ObjString* name = READ_STRING();

			Value value;
			if (TableGet(&instance->fields, name, &value))
			{
				Pop(1); //Instance
				Push(value);
				break;
			}

			if (!BindMethod(instance->klass, name))
			{
				return INTERPRET_RUNTIME_ERROR;
			}

			break;
		}
		case OP_SET_PROPERTY:
		{
			if (!IS_INSTANCE(*Peek(1)))
			{
				RuntimeError(ip, "Only instances have properties.");
				return INTERPRET_RUNTIME_ERROR;
			}

			ObjInstance* instance = AS_INSTANCE(*Peek(1));
			TableSet(&instance->fields, READ_STRING(), *Peek(0));
			Value value = Pop(1);
			Push(value);
			break;
		}
		case OP_GET_SUPER:
		{
			ObjString* name = READ_STRING();
			ObjClass* superclass = AS_CLASS(Pop(1));

			if (!BindMethod(superclass, name))
			{
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_EQUAL:
		{
			Value a = Pop(1);
			Value b = Pop(1);
			Push(BOOL_VAL(ValuesEqual(a, b)));
			break;
		}
		case OP_GREATER:	BINARY_OP(BOOL_VAL, >); break;
		case OP_LESS:		BINARY_OP(BOOL_VAL, <); break;
		case OP_ADD:
		{
			if (IS_STRING(*Peek(0)) && IS_STRING(*Peek(1)))
			{
				Concatenate();
			}
			else if (IS_NUMBER(*Peek(0)) && IS_NUMBER(*Peek(1)))
			{
				double b = AS_NUMBER(Pop(1));
				double a = AS_NUMBER(Pop(1));
				Push(NUMBER_VAL(a + b));
			}
			else
			{
				RuntimeError(ip, "Operands must be two numbers or two strings");
				return INTERPRET_RUNTIME_ERROR;
			}

			break;
		}
		case OP_SUBTRACT:	BINARY_OP(NUMBER_VAL, -); break;
		case OP_MULTIPLY:	BINARY_OP(NUMBER_VAL, *); break;
		case OP_DIVIDE:		BINARY_OP(NUMBER_VAL, /); break;
		case OP_NOT:		Push(BOOL_VAL(IsFalsey(Pop(1)))); break;
		case OP_NEGATE:

			if (!IS_NUMBER(*Peek(0)))
			{
				RuntimeError(ip, "Operand must be a number");
				return INTERPRET_RUNTIME_ERROR;
			}

			*Peek(0) = NUMBER_VAL(-AS_NUMBER(*Peek(0)));
			break;
		case OP_PRINT:
			PrintValue(Pop(1));
			printf_s("\n");
			break;
		case OP_JUMP:
		{
			uint16_t offset = READ_SHORT();
			ip += offset;
			break;
		}
		case OP_JUMP_IF_FALSE:
		{
			uint16_t offset = READ_SHORT();
			if (IsFalsey(*Peek(0)))
			{
				ip += offset;
			}

			break;
		}
		case OP_LOOP:
		{
			uint16_t offset = READ_SHORT();
			ip -= offset;
			break;
		}
		case OP_CALL:
		{
			uint8_t argCount = READ_BYTE();

			bool changesFrame = false;
			if (!CallValue(*Peek(argCount), argCount, ip, &changesFrame))
			{
				return INTERPRET_RUNTIME_ERROR;
			}

			frame = &vm.frames[vm.frameCount - 1];
			if (changesFrame)
			{
				ip = frame->ip;
			}
			break;
		}
		case OP_INVOKE:
		{
			ObjString* method = READ_STRING();
			int argCount = READ_BYTE();
			bool changesFrame = false;
			if (!Invoke(method, argCount, ip, &changesFrame))
			{
				return INTERPRET_RUNTIME_ERROR;
			}

			if (changesFrame)
			{
				frame = &vm.frames[vm.frameCount - 1];
				ip = frame->ip;
			}
			break;
		}
		case OP_SUPER_INVOKE:
		{
			ObjString* method = READ_STRING();
			int argCount = READ_BYTE();
			ObjClass* superclass = AS_CLASS(Pop(1));

			if (!InvokeFromClass(superclass, method, argCount, ip))
			{
				return INTERPRET_RUNTIME_ERROR;
			}

			frame = &vm.frames[vm.frameCount - 1];
			ip = frame->ip;
			break;
		}
		case OP_CLOSURE:
		{
			ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
			ObjClosure* closure = NewClosure(function);
			Push(OBJ_VAL(closure));

			for (int idx = 0; idx < function->upvalueCount; idx++)
			{
				uint8_t isLocal = READ_BYTE();
				uint8_t index = READ_BYTE();

				if (isLocal)
				{
					closure->upvalues[idx] = CaptureUpvalue(frame->slots + index);
				}
				else
				{
					closure->upvalues[idx] = frame->closure->upvalues[idx];
				}
			}

			break;
		}
		case OP_CLOSE_UPVAL:
		{
			CloseUpvalues(vm.stackTop - 1);
			break;
		}
		case OP_RETURN:
			Value result = Pop(1);
			CloseUpvalues(frame->slots);
			vm.frameCount--;
			if (vm.frameCount == 0)
			{
				Pop(1);
				return INTERPRET_OK;
			}

			vm.stackTop = frame->slots;
			Push(result);
			frame = &vm.frames[vm.frameCount - 1];
			ip = frame->ip;
			break;
		case OP_CLASS:
			Push(OBJ_VAL(NewClass(READ_STRING())));
			break;
		case OP_INHERIT:
			Value superclass = *Peek(1);
			if (!IS_CLASS(superclass))
			{
				RuntimeError(ip, "Superclass must be a class.");
				return INTERPRET_RUNTIME_ERROR;
			}
			ObjClass* subclass = AS_CLASS(*Peek(0));
			TableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
			Pop(1);
			break;
		case OP_METHOD:
			DefineMethod(READ_STRING());
		}
	}

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef BINARY_OP
#undef READ_STRING
}

InterpretResult Interpret(const char* source)
{
	ObjFunction* function = Compile(source);
	if (function == NULL)
	{
		return INTERPRET_COMPILE_ERROR;
	}

	Push(OBJ_VAL(function));
	ObjClosure* closure = NewClosure(function);
	Pop(1);
	Push(OBJ_VAL(closure));
	Call(closure, 0);

	return Run();
}