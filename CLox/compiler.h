#ifndef clox_compiler_h
#define clox_compiler_h

#include "vm.h"
#include "object.h"

ObjFunction* Compile(const char* source);
void MarkCompilerRoots();

#endif
