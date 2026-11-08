#ifndef VALUE_H
#define VALUE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    False,
    True
} Bool;

typedef enum {
    VALUE_NULL,
    VALUE_BOOL,
    VALUE_INT,
    VALUE_UNSIGNED_INT,
    VALUE_LONG,
    VALUE_FLOAT,
    VALUE_DOUBLE,
    VALUE_STRING,
    VALUE_POINTER,
    VALUE_LIST,
    VALUE_ARRAY,
    VALUE_MAP
} ValueKind;

typedef struct Value Value;
typedef struct ValueNode ValueNode;
typedef struct HashMap HashMap;
typedef struct ValueArray ValueArray;

typedef void (*ValueFreeFunc)(Value *v);

struct Value {
    ValueKind kind;
    ValueFreeFunc free;

    union {
        int _int;
        uint32_t _unsigned_int;
        long _long;
        float _float;
        double _double;
        char *str;
        void *ptr;
        Bool _bool;
        ValueNode *list;
        HashMap *map;
        ValueArray *array;
    };
};

struct ValueNode {
    Value value;
    ValueNode *next;
};

struct ValueArray {
    Value *values;
    size_t len;
};

Value *ValueNew(ValueKind kind, ValueFreeFunc free_f);
void ValueFree(Value *value);
void ValueDeepFree(Value *value);
void ValueClear(Value *value);

Value *ValueAppend(Value *container, Value *element);
size_t ValueLen(Value *container);

void ValueSet(Value *map_v, const char *key, Value *val);
Value *ValueGet(Value *map_v, const char *key);



Value *ValueNewInt(int value);
Value *ValueNewUnsignedInt(uint32_t value);
Value *ValueNewLong(long value);
Value *ValueNewFloat(float value);
Value *ValueNewDouble(double value);
Value *ValueNewBool(Bool value);
Value *ValueNewString(char *value);
Value *ValueNewPointer(void *value);
Value *ValueNewNull(void);


int ValueIsMap(Value *value);
int ValueIsArray(Value *arr);
Value *ValueAt(Value *container, size_t index);
#endif
