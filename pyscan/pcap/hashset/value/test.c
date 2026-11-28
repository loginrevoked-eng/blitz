#include "repr.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void expect(const HashSetValue *value, const char *expected)
{
    char *actual = HashSetValue_Repr(value);
    assert(actual != NULL);
    assert(strcmp(actual, expected) == 0);
    free(actual);
}

int main(void)
{
    HashSetValue *value = HashSetValue_NewNull();
    expect(value, "null");
    HashSetValue_Free(value);

    value = HashSetValue_NewInt(-42);
    expect(value, "-42");
    HashSetValue_Free(value);

    value = HashSetValue_NewBool(HASHSET_VALUE_TRUE);
    expect(value, "true");
    HashSetValue_Free(value);

    value = HashSetValue_NewString("a\n\"b");
    expect(value, "\"a\\n\\\"b\"");
    HashSetValue_Free(value);

    value = HashSetValue_New(HASHSET_VALUE_ARRAY, NULL);
    HashSetValue *item = HashSetValue_NewInt(7);
    assert(HashSetValue_Append(value, item) == value);
    free(item);
    assert(HashSetValue_Append(value, NULL) == value);
    expect(value, "[7, null]");
    HashSetValue_Free(value);

    value = HashSetValue_New(HASHSET_VALUE_MAP, NULL);
    item = HashSetValue_NewBool(HASHSET_VALUE_FALSE);
    HashSetValue_Set(value, "ready", item);
    free(item);
    expect(value, "{\"ready\": false}");
    HashSetValue_Free(value);
    return 0;
}