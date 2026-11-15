#ifndef MOOLAH_VALUE_H
#define MOOLAH_VALUE_H

#include <stddef.h>

typedef enum {
    MOOLAH_FALSE,
    MOOLAH_TRUE
} MoolahBool;

typedef enum {
    MOOLAH_VALUE_BOOL,
    MOOLAH_VALUE_INT,
    MOOLAH_VALUE_LONG,
    MOOLAH_VALUE_DOUBLE,
    MOOLAH_VALUE_STRING,
    MOOLAH_VALUE_POINTER,
    MOOLAH_VALUE_LIST,
    MOOLAH_VALUE_NULL,
    MOOLAH_VALUE_UNKNOWN
} MoolahValueKind;

typedef struct MoolahValue MoolahValue;
typedef struct MoolahValueNode MoolahValueNode;
typedef void (*MoolahValueFreeFunc)(MoolahValue *value);

struct MoolahValue {
    MoolahValueKind kind;
    MoolahValueFreeFunc free;
    union {
        int i;
        long l;
        double d;
        char *s;
        void *p;
        MoolahBool b;
        MoolahValueNode *list;
    };
};

struct MoolahValueNode {
    MoolahValue value;
    MoolahValueNode *next;
};

MoolahValue *MoolahValueNew(MoolahValueKind kind);
MoolahValue *MoolahValueNewEx(MoolahValueKind kind, MoolahValueFreeFunc free_func);
void MoolahValueFree(MoolahValue *value);
void MoolahValueDeepFree(MoolahValue *value);
MoolahValue *MoolahValueAppend(MoolahValue *container, MoolahValue *element);
size_t MoolahValueLen(MoolahValue *container);

#endif
