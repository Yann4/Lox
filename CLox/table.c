#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void InitTable(Table* table)
{
	table->capacity = 0;
	table->count = 0;
	table->entries = NULL;
}

void FreeTable(Table* table)
{
	FREE_ARRAY(Entry, table->entries, table->capacity);
	InitTable(table);
}

static Entry* FindEntry(Entry* entries, int capacity, ObjString* key)
{
	uint32_t index = key->hash & (capacity - 1);
	Entry* tombstone = NULL;

	for (;;)
	{
		Entry* entry = &entries[index];
		if (entry->key == NULL)
		{
			//empty entry
			if (IS_NIL(entry->value))
			{
				return tombstone != NULL ? tombstone : entry;
			}
			else
			{
				//we found a tombstone
				if (tombstone == NULL)
				{
					tombstone = entry;
				}
			}
		}
		else if (entry->key == key)
		{
			return entry;
		}

		index = (index + 1) & (capacity - 1);
	}
}

static void AdjustCapacity(Table* table, int newCapacity)
{
	Entry* entries = ALLOCATE(Entry, newCapacity);
	for (int idx = 0; idx < newCapacity; idx++)
	{
		entries[idx].key = NULL;
		entries[idx].value = NIL_VAL;
	}

	table->count = 0;
	for (int idx = 0; idx < table->capacity; idx++)
	{
		Entry* entry = &table->entries[idx];
		if (entry->key == NULL)
		{
			continue;
		}

		Entry* dest = FindEntry(entries, newCapacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count;
	}

	FREE_ARRAY(Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = newCapacity;
}

bool TableSet(Table* table, ObjString* key, Value value)
{
	if ((double)table->count + 1 > table->capacity * TABLE_MAX_LOAD)
	{
		int capacity = GROW_CAPACITY(table->capacity);
		AdjustCapacity(table, capacity);
	}

	Entry* entry = FindEntry(table->entries, table->capacity, key);
	bool isNewKey = entry->key == NULL;
	if (isNewKey && IS_NIL(entry->value))
	{
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	return isNewKey;
}

bool TableGet(Table* table, ObjString* key, Value* value)
{
	if (table->count == 0)
	{
		return false;
	}

	Entry* entry = FindEntry(table->entries, table->capacity, key);
	if (entry->key != NULL)
	{
		*value = entry->value;
		return true;
	}

	return false;
}

bool TableDelete(Table* table, ObjString* key)
{
	if (table->count == 0)
	{
		return false;
	}

	Entry* entry = FindEntry(table->entries, table->capacity, key);
	if (entry->key == NULL)
	{
		return false;
	}

	entry->key = NULL;
	entry->value = BOOL_VAL(false);
	return true;
}

void TableAddAll(Table* source, Table* dest)
{
	for (int idx = 0; idx < source->capacity; idx++)
	{
		Entry* entry = &source->entries[idx];
		if (entry->key != NULL)
		{
			TableSet(dest, entry->key, entry->value);
		}
	}
}

ObjString* TableFindString(Table* table, const char* chars, int length, uint32_t hash)
{
	if (table->count == 0)
	{
		return NULL;
	}

	uint32_t index = hash & (table->capacity - 1);
	for (;;)
	{
		Entry* entry = &table->entries[index];
		if (entry->key == NULL)
		{
			//stop if we find an empty non-tombstone
			if (IS_NIL(entry->value))
			{
				return NULL;
			}
		}
		else if (entry->key->length == length && entry->key->hash == hash &&
			memcmp(entry->key->chars, chars, length) == 0)
		{
			return entry->key;
		}

		index = (index + 1) & (table->capacity - 1);
	}
}

void TableRemoveWhite(Table* table)
{
	for (int idx = 0; idx < table->capacity; idx++)
	{
		Entry* entry = &table->entries[idx];
		if (entry->key != NULL && !entry->key->obj.isMarked)
		{
			TableDelete(table, entry->key);
		}
	}
}

void MarkTable(Table* table)
{
	for (int idx = 0; idx < table->capacity; idx++)
	{
		Entry* entry = &table->entries[idx];
		MarkObject((Obj*)entry->key);
		MarkValue(entry->value);
	}
}