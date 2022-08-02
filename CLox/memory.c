#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif //DEBUG_LOC_GC

#define GC_HEAP_GROW_FACTOR 2

void* Reallocate(void* pointer, size_t oldSize, size_t newSize)
{
	vm.bytesAllocated += newSize - oldSize;
#ifdef DEBUG_LOG_GC
	printf_s("Total bytes allocated %zu\n", vm.bytesAllocated);
#endif //#ifdef DEBUG_LOG_GC

	if (newSize > oldSize)
	{
#ifdef DEBUG_STRESS_GC
		CollectGarbage();
#endif //DEBUG_STRESS_GC
	}

	if (vm.bytesAllocated > vm.nextGC)
	{
		CollectGarbage();
	}

	if (newSize == 0)
	{
		free(pointer);
		return NULL;
	}

	void* result = realloc(pointer, newSize);
	//If we literally don't have enough memory, crap out
	if (result == NULL)
	{
		exit(1);
	}

	return result;
}

void FreeObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
	printf_s("%p free type %d\n", (void*)object, object->type);
#endif //DEBUG_LOG_GC

	switch (object->type)
	{
	case OBJ_BOUND_METHOD:
		FREE(ObjBoundMethod, object);
		break;
	case OBJ_CLASS:
	{
		ObjClass* klass = (ObjClass*)object;
		FreeTable(&klass->methods);
		FREE(ObjClass, object);
		break;
	}
	case OBJ_INSTANCE:
		ObjInstance* instance = (ObjInstance*)object;
		FreeTable(&instance->fields);
		FREE(ObjInstance, object);
		break;
	case OBJ_CLOSURE:
		ObjClosure* closure = (ObjClosure*)object;
		FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
		FREE(ObjClosure, object);
		break;
	case OBJ_FUNCTION:
	{
		ObjFunction* function = (ObjFunction*)object;
		FreeChunk(&function->chunk);
		FREE(ObjFunction, object);
		break;
	}
	case OBJ_NATIVE:
		FREE(ObjNative, object);
		break;
	case OBJ_UPVALUE:
		FREE(ObjUpvalue, object);
		break;
	case OBJ_STRING:
	{
		ObjString* string = (ObjString*)object;
		FREE_ARRAY(char, string->chars, string->length + 1);
		FREE(ObjString, object);
		break;
	}
	}
}

void MarkObject(Obj* object)
{
	if (object == NULL || object->isMarked)
	{
		return;
	}

#ifdef DEBUG_LOG_GC
	printf_s("%p mark ", (void*)object);
	PrintValue(OBJ_VAL(object));
	printf_s("\n");
#endif //DEBUG_LOG_GC

	object->isMarked = true;

	if (vm.greyCapacity < vm.greyCount + 1)
	{
		vm.greyCapacity = GROW_CAPACITY(vm.greyCapacity);
		vm.greyStack = (Obj**)realloc(vm.greyStack, sizeof(Obj*) * vm.greyCapacity);

		if (vm.greyStack == NULL)
		{
			exit(1);
		}
	}

	vm.greyStack[vm.greyCount++] = object;
}

void MarkValue(Value value)
{
	if (IS_OBJ(value))
	{
		MarkObject(AS_OBJ(value));
	}
}

static void MarkArray(ValueArray* array)
{
	for (int idx = 0; idx < array->count; idx++)
	{
		MarkValue(array->values[idx]);
	}
}

static void BlackenObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
	printf_s("%p blacken ", (void*)object);
	PrintValue(OBJ_VAL(object));
	printf("\n");
#endif //DEBUG_LOG_GC

	switch (object->type)
	{
	case OBJ_BOUND_METHOD:
	{
		ObjBoundMethod* bound = (ObjBoundMethod*)object;
		MarkValue(bound->receiver);
		MarkObject((Obj*)bound->method);
		break;
	}
	case OBJ_CLASS:
	{
		ObjClass* klass = (ObjClass*)object;
		MarkObject((Obj*)klass->name);
		MarkTable(&klass->methods);
		break;
	}
	case OBJ_INSTANCE:
	{
		ObjInstance* instance = (ObjInstance*)object;
		MarkObject((Obj*)instance->klass);
		MarkTable(&instance->fields);
		break;
	}
	case OBJ_CLOSURE:
	{
		ObjClosure* closure = (ObjClosure*)object;
		MarkObject((Obj*)closure->function);
		for (int idx = 0; idx < closure->upvalueCount; idx++)
		{
			MarkObject((Obj*)closure->upvalues[idx]);
		}
		break;
	}
	case OBJ_FUNCTION:
		ObjFunction* function = (ObjFunction*)object;
		MarkObject((Obj*)function->name);
		MarkArray(&function->chunk.constants);
		break;
	case OBJ_UPVALUE:
		MarkValue(((ObjUpvalue*)object)->closed);
		break;
	case OBJ_NATIVE:
	case OBJ_STRING:
		break;
	}
}

static void MarkRoots()
{
	for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
	{
		MarkValue(*slot);
	}

	for (int idx = 0; idx < vm.frameCount; idx++)
	{
		MarkObject((Obj*)vm.frames[idx].closure);
	}

	for (ObjUpvalue* upval = vm.openUpvalues; upval != NULL; upval = upval->next)
	{
		MarkObject((Obj*)upval);
	}

	MarkTable(&vm.globals);
	MarkCompilerRoots();
	MarkObject((Obj*)vm.initString);
}

static void TraceReferences()
{
	while (vm.greyCount > 0)
	{
		Obj* object = vm.greyStack[--vm.greyCount];
		BlackenObject(object);
	}
}

static void Sweep()
{
	Obj* previous = NULL;
	Obj* object = vm.objects;
	while (object != NULL)
	{
		if (object->isMarked)
		{
			object->isMarked = false;
			previous = object;
			object = object->next;
		}
		else
		{
			Obj* unreached = object;
			object = object->next;

			if (previous != NULL)
			{
				previous->next = object;
			}
			else
			{
				vm.objects = object;
			}

			FreeObject(unreached);
		}
	}
}

void CollectGarbage()
{
#ifdef DEBUG_LOG_GC
	printf_s("-- gc begin\n");
	size_t before = vm.bytesAllocated;
#endif //DEBUG_LOG_GC

	MarkRoots();
	TraceReferences();
	TableRemoveWhite(&vm.strings);
	Sweep();

	vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
	printf_s("-- gc end\n");
	printf_s("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif //DEBUG_LOG_GC
}

void FreeObjects()
{
	Obj* object = vm.objects;
	while (object != NULL)
	{
		Obj* next = object->next;
		FreeObject(object);
		object = next;
	}

	free(vm.greyStack);
}