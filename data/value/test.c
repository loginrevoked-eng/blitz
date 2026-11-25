#include "repr.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void expect(const Value *value, const char *expected)
{
    char *actual = ValueRepr(value);
    assert(actual != NULL);
    assert(strcmp(actual, expected) == 0);
    free(actual);
}

int main(void)
{
    Value *value = ValueNewNull();
    expect(value, "null");
    ValueFree(value);

    value = ValueNewInt(-42);
    expect(value, "-42");
    ValueFree(value);

    value = ValueNewBool(True);
    expect(value, "true");
    ValueFree(value);

    value = ValueNewString("a\n\"b");
    expect(value, "\"a\\n\\\"b\"");
    ValueFree(value);

    value = ValueNew(VALUE_ARRAY, NULL);
    Value *item = ValueNewInt(7);
    assert(ValueAppend(value, item) == value);
    free(item);
    assert(ValueAppend(value, NULL) == value);
    expect(value, "[7, null]");
    ValueFree(value);

    value = ValueNew(VALUE_MAP, NULL);
    item = ValueNewBool(False);
    ValueSet(value, "ready", item);
    free(item);
    expect(value, "{\"ready\": false}");
    ValueFree(value);
    return 0;
}