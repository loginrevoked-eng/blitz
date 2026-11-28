#include "repr.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void expect(const IpPickerValue *value, const char *expected)
{
    char *actual = IpPickerValue_Repr(value);
    assert(actual != NULL);
    assert(strcmp(actual, expected) == 0);
    free(actual);
}

int main(void)
{
    IpPickerValue *value = IpPickerValue_NewNull();
    expect(value, "null");
    IpPickerValue_Free(value);

    value = IpPickerValue_NewInt(-42);
    expect(value, "-42");
    IpPickerValue_Free(value);

    value = IpPickerValue_NewBool(IPPICKER_VALUE_TRUE);
    expect(value, "true");
    IpPickerValue_Free(value);

    value = IpPickerValue_NewString("a\n\"b");
    expect(value, "\"a\\n\\\"b\"");
    IpPickerValue_Free(value);

    value = IpPickerValue_New(IPPICKER_VALUE_ARRAY, NULL);
    IpPickerValue *item = IpPickerValue_NewInt(7);
    assert(IpPickerValue_Append(value, item) == value);
    free(item);
    assert(IpPickerValue_Append(value, NULL) == value);
    expect(value, "[7, null]");
    IpPickerValue_Free(value);

    value = IpPickerValue_New(IPPICKER_VALUE_MAP, NULL);
    item = IpPickerValue_NewBool(IPPICKER_VALUE_FALSE);
    IpPickerValue_Set(value, "ready", item);
    free(item);
    expect(value, "{\"ready\": false}");
    IpPickerValue_Free(value);
    return 0;
}