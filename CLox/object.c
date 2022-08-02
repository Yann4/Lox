#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJ(type, objectType) \
	(type*)AllocateObject(sizeof(type), objectType)

static Obj* AllocateObject(size_t size, ObjType type)
{
	Obj* object = (Obj*)Reallocate(NULL, 0, size);
	object->type = type;
	object->isMarked = false;
	object->next = vm.objects;
	vm.objects = object;

#ifdef DEBUG_LOG_GC
	printf_s("%p allocate %zu for %d\n", (void*)object, size, type);
#endif //DEBUG_LOG_GC

	return object;
}

ObjClass* NewClass(ObjString* name)
{
	ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
	klass->name = name;
	InitTable(&klass->methods);
	return klass;
}

ObjInstance* NewInstance(ObjClass* klass)
{
	ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
	instance->klass = klass;
	InitTable(&instance->fields);
	return instance;
}

ObjClosure* NewClosure(ObjFunction* function)
{
	ObjUpvalue** upvals = ALLOCATE(ObjUpvalue*, function->upvalueCount);
	for (int idx = 0; idx < function->upvalueCount; idx++)
	{
		upvals[idx] = NULL;
	}

	ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
	closure->function = function;
	closure->upvalues = upvals;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

ObjBoundMethod* NewBoundMethod(Value receiver, ObjClosure* method)
{
	ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
	bound->receiver = receiver;
	bound->method = method;
	return bound;
}

static ObjString* AllocateString(char* chars, int length, uint32_t hash)
{
	ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;
	Push(OBJ_VAL(string));
	TableSet(&vm.strings, string, NIL_VAL);
	Pop(1);
	return string;
}

static uint32_t HashString(const char* chars, int length)
{
	//FNV-1a hash algorithm
	uint32_t hash = 2166136261u;

	for (int i = 0; i < length; i++) 
	{
		hash ^= (uint8_t)chars[i];
		hash *= 16777619;
	}

	return hash;
}

ObjString* TakeString(char* chars, int length)
{
	uint32_t hash = HashString(chars, length);
	ObjString* interned = TableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL)
	{
		FREE_ARRAY(char, chars, length);
		return interned;
	}

	return AllocateString(chars, length, hash);
}

ObjString* CopyString(const char* chars, int length)
{
	uint32_t hash = HashString(chars, length);
	ObjString* interned = TableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL)
	{
		return interned;
	}

	char* heapChars = ALLOCATE(char, (rsize_t)length + 1);
	memcpy_s(heapChars, (rsize_t)length + 1, chars, length);
	heapChars[length] = '\0';
	return AllocateString(heapChars, length, hash);
}

ObjUpvalue* NewUpvalue(Value* slot)
{
	ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
	upvalue->location = slot;
	upvalue->next = NULL;
	upvalue->closed = NIL_VAL;
	return upvalue;
}

static void PrintFunction(ObjFunction* function)
{
	if (function->name == NULL)
	{
		printf_s("<script>");
	}
	else
	{
		printf_s("<fn %s>", function->name->chars);
	}
}

ObjFunction* NewFunction()
{
	ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
	function->arity = 0;
	function->upvalueCount = 0;
	function->name = NULL;
	InitChunk(&function->chunk);
	return function;
}

ObjNative* NewNative(NativeFn function)
{
	ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
	native->function = function;
	return native;
}

void PrintObject(Value value)
{
	switch (OBJ_TYPE(value))
	{
	case OBJ_BOUND_METHOD:
		PrintFunction(AS_BOUND_METHOD(value)->method->function);
		break;
	case OBJ_CLASS:
		printf_s("%s", AS_CLASS(value)->name->chars);
		break;
	case OBJ_INSTANCE:
		printf_s("%s instance", AS_INSTANCE(value)->klass->name->chars);
		break;
	case OBJ_CLOSURE:
		PrintFunction(AS_CLOSURE(value)->function);
		break;
	case OBJ_FUNCTION:
		PrintFunction(AS_FUNCTION(value));
		break;
	case OBJ_NATIVE:
		printf_s("<native fn>");
		break;
	case OBJ_UPVALUE:
		printf_s("upvalue");
		break;
	case OBJ_STRING:
		printf_s("%s", AS_CSTRING(value));
		break;
	}
}