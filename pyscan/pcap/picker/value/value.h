#ifndef IPPICKER_VALUE_H
#define IPPICKER_VALUE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    IPPICKER_VALUE_FALSE,
    IPPICKER_VALUE_TRUE
} IpPickerValueBool;

typedef enum {
    IPPICKER_VALUE_NULL,
    IPPICKER_VALUE_BOOL,
    IPPICKER_VALUE_INT,
    IPPICKER_VALUE_UNSIGNED_INT,
    IPPICKER_VALUE_LONG,
    IPPICKER_VALUE_FLOAT,
    IPPICKER_VALUE_DOUBLE,
    IPPICKER_VALUE_STRING,
    IPPICKER_VALUE_POINTER,
    IPPICKER_VALUE_LIST,
    IPPICKER_VALUE_ARRAY,
    IPPICKER_VALUE_MAP
} IpPickerValueKind;

typedef struct IpPickerValue IpPickerValue;
typedef struct IpPickerValueNode IpPickerValueNode;
typedef struct IpPickerValueMap IpPickerValueMap;
typedef struct IpPickerValueArray IpPickerValueArray;

typedef void (*IpPickerValueFreeFunc)(IpPickerValue *v);

struct IpPickerValue {
    IpPickerValueKind kind;
    IpPickerValueFreeFunc free;

    union {
        int _int;
        uint32_t _unsigned_int;
        long _long;
        float _float;
        double _double;
        char *str;
        void *ptr;
        IpPickerValueBool _bool;
        IpPickerValueNode *list;
        IpPickerValueMap *map;
        IpPickerValueArray *array;
    };
};

struct IpPickerValueNode {
    IpPickerValue value;
    IpPickerValueNode *next;
};

struct IpPickerValueArray {
    IpPickerValue *values;
    size_t len;
};

IpPickerValue *IpPickerValue_New(IpPickerValueKind kind, IpPickerValueFreeFunc free_f);
void IpPickerValue_Free(IpPickerValue *value);
void IpPickerValue_DeepFree(IpPickerValue *value);
void IpPickerValue_Clear(IpPickerValue *value);

IpPickerValue *IpPickerValue_Append(IpPickerValue *container, IpPickerValue *element);
size_t IpPickerValue_Len(IpPickerValue *container);

void IpPickerValue_Set(IpPickerValue *map_v, const char *key, IpPickerValue *val);
IpPickerValue *IpPickerValue_Get(IpPickerValue *map_v, const char *key);



IpPickerValue *IpPickerValue_NewInt(int value);
IpPickerValue *IpPickerValue_NewUnsignedInt(uint32_t value);
IpPickerValue *IpPickerValue_NewLong(long value);
IpPickerValue *IpPickerValue_NewFloat(float value);
IpPickerValue *IpPickerValue_NewDouble(double value);
IpPickerValue *IpPickerValue_NewBool(IpPickerValueBool value);
IpPickerValue *IpPickerValue_NewString(char *value);
IpPickerValue *IpPickerValue_NewPointer(void *value);
IpPickerValue *IpPickerValue_NewNull(void);


int IpPickerValue_IsMap(IpPickerValue *value);
int IpPickerValue_IsArray(IpPickerValue *arr);
IpPickerValue *IpPickerValue_At(IpPickerValue *container, size_t index);
#endif
