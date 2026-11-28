#ifndef HASHSET_VALUE_H
#define HASHSET_VALUE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    HASHSET_VALUE_FALSE,
    HASHSET_VALUE_TRUE
} HashSetValueBool;

typedef enum {
    HASHSET_VALUE_NULL,
    HASHSET_VALUE_UINT64,
    HASHSET_VALUE_BOOL,
    HASHSET_VALUE_INT,
    HASHSET_VALUE_UNSIGNED_INT,
    HASHSET_VALUE_LONG,
    HASHSET_VALUE_FLOAT,
    HASHSET_VALUE_DOUBLE,
    HASHSET_VALUE_STRING,
    HASHSET_VALUE_POINTER,
    HASHSET_VALUE_LIST,
    HASHSET_VALUE_ARRAY,
    HASHSET_VALUE_MAP
} HashSetValueKind;

typedef struct HashSetValue HashSetValue;
typedef struct HashSetValueNode HashSetValueNode;
typedef struct HashSetValueMap HashSetValueMap;
typedef struct HashSetValueArray HashSetValueArray;

typedef void (*HashSetValueFreeFunc)(HashSetValue *v);

struct HashSetValue {
    HashSetValueKind kind;
    HashSetValueFreeFunc free;

    union {
        int _int;
        uint32_t _unsigned_int;
        uint64_t _uint64;
        long _long;
        float _float;
        double _double;
        char *str;
        void *ptr;
        HashSetValueBool _bool;
        HashSetValueNode *list;
        HashSetValueMap *map;
        HashSetValueArray *array;
    };
};

struct HashSetValueNode {
    HashSetValue value;
    HashSetValueNode *next;
};

struct HashSetValueArray {
    HashSetValue *values;
    size_t len;
};

HashSetValue *HashSetValue_New(HashSetValueKind kind, HashSetValueFreeFunc free_f);
void HashSetValue_Free(HashSetValue *value);
void HashSetValue_DeepFree(HashSetValue *value);
void HashSetValue_Clear(HashSetValue *value);

HashSetValue *HashSetValue_Append(HashSetValue *container, HashSetValue *element);
size_t HashSetValue_Len(HashSetValue *container);

void HashSetValue_Set(HashSetValue *map_v, const char *key, HashSetValue *val);
HashSetValue *HashSetValue_Get(HashSetValue *map_v, const char *key);



HashSetValue *HashSetValue_NewInt(int value);
HashSetValue *HashSetValue_NewUnsignedInt(uint32_t value);
HashSetValue *HashSetValue_NewUInt64(uint64_t value);
HashSetValue *HashSetValue_NewLong(long value);
HashSetValue *HashSetValue_NewFloat(float value);
HashSetValue *HashSetValue_NewDouble(double value);
HashSetValue *HashSetValue_NewBool(HashSetValueBool value);
HashSetValue *HashSetValue_NewString(char *value);
HashSetValue *HashSetValue_NewPointer(void *value);
HashSetValue *HashSetValue_NewNull(void);


int HashSetValue_IsMap(HashSetValue *value);
int HashSetValue_IsArray(HashSetValue *arr);
HashSetValue *HashSetValue_At(HashSetValue *container, size_t index);
#endif
