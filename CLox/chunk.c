#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h"

static void InitLinesArray(Lines* lines)
{
	lines->capacity = 0;
	lines->count = 0;
	lines->lines = NULL;
}

static void WriteLine(Lines* lines, int line)
{
	//Simple run-length encoding.
	//[count, value] pairs
	if (lines->capacity < lines->count + 2)
	{
		int oldCapacity = lines->capacity;
		lines->capacity = GROW_CAPACITY(oldCapacity);
		lines->lines = GROW_ARRAY(int, lines->lines, oldCapacity, lines->capacity);
	}

	if (lines->count != 0 && lines->lines[lines->count - 1] == line)
	{
		lines->lines[lines->count - 2]++;
	}
	else
	{
		lines->lines[lines->count] = 1;
		lines->lines[lines->count + 1] = line;
		lines->count += 2;
	}
}

void InitChunk(Chunk* chunk)
{
	chunk->capacity = 0;
	chunk->count = 0;
	chunk->code = NULL;
	InitLinesArray(&chunk->lines);
	InitValueArray(&chunk->constants);
}

void FreeChunk(Chunk* chunk)
{
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(int, chunk->lines.lines, chunk->lines.capacity);
	FreeValueArray(&chunk->constants);
	InitChunk(chunk);
}

void WriteChunk(Chunk* chunk, uint8_t byte, int line)
{
	if (chunk->capacity < chunk->count + 1)
	{
		int oldCapacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(oldCapacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
	}

	chunk->code[chunk->count] = byte;
	WriteLine(&chunk->lines, line);
	chunk->count++;
}

int AddConstant(Chunk* chunk, Value value)
{
	Push(value);
	WriteValueArray(&chunk->constants, value);
	Pop(1);
	return chunk->constants.count - 1;
}

void WriteConstant(Chunk* chunk, Value value, int line)
{
	WriteChunk(chunk, OP_CONSTANT, line);
	WriteChunk(chunk, AddConstant(chunk, value), line);
}

int GetLine(Chunk* chunk, int instructionIdx)
{
	int idx = 0;
	int current = 0;
	while (idx < chunk->lines.count)
	{
		current += chunk->lines.lines[idx];
		if (current > instructionIdx)
		{
			break;
		}
		else
		{
			idx += 2;
		}
	}

	return chunk->lines.lines[idx + 1];
}