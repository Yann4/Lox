#ifndef clox_table_h
#define clox_table_h
#include "common.h"
#include "value.h"

typedef struct
{
	ObjString* key;
	Value value;
} Entry;

typedef struct
{
	int count;
	int capacity;
	Entry* entries;
} Table;

void InitTable(Table* table);
void FreeTable(Table* table);

bool TableSet(Table* table, ObjString* key, Value value);
bool TableGet(Table* table, ObjString* key, Value* value);
bool TableDelete(Table* table, ObjString* key);

void TableAddAll(Table* source, Table* dest);
ObjString* TableFindString(Table* table, const char* chars, int length, uint32_t hash);

void TableRemoveWhite(Table* table);
void MarkTable(Table* table);
#endif
