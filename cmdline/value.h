#ifndef VALUE_H
#define VALUE_H

#include <stddef.h>

typedef enum {
    False,
    True
} Bool;

typedef enum {
    VALUE_BOOL,
    VALUE_INT,
    VALUE_LONG,
    VALUE_DOUBLE,
    VALUE_STRING,
    VALUE_POINTER,
    VALUE_LIST,
    VALUE_NULL,
    VALUE_UNKNOWN,
} ValueKind;

typedef struct Value Value;
typedef struct ValueNode ValueNode;

typedef void (*ValueFreeFunc)(Value *v);

struct Value {
    ValueKind kind;
    ValueFreeFunc free;

    union {
        int i;
        long l;
        double d;
        char *s;
        void *p;
        Bool b;
        ValueNode *list;
    };
};

struct ValueNode {
    Value value;
    ValueNode *next;
};

Value *ValueNew(ValueKind kind);
Value *ValueNewEx(ValueKind kind, ValueFreeFunc free);
void ValueFree(Value *value);
void ValueDeepFree(Value *value);
Value *ValueAppend(Value *container, Value *element);
size_t ValueLen(Value *container);

#endif