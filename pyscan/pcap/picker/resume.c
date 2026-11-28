#include "resume.h"
#include "deser.h"
#include "value/public.h"

#include <stdlib.h>

int IpPicker_LoadBuckets(IpPicker *picker, const char *json)
{
    if (picker == NULL || json == NULL)
        return 0;

    BucketList *loaded = BucketListNew();
    uint32_t bucket_width = 1;
    int valid = loaded != NULL && IpPicker_BucketListFromJson(json, loaded, &bucket_width) &&
        bucket_width == picker->config.bucket_width;
    if (!valid) {
        BucketListFree(loaded);
        return 0;
    }

    for (size_t i = 0; i < loaded->len; i++) {
        Bucket *bucket = &loaded->buckets[i];
        bucket->expires_at = 1;
    }

    BucketListFree(picker->eligible);
    picker->eligible = loaded;
    return 1;
}

char *IpPicker_DumpBuckets(IpPicker *picker)
{
    if (picker == NULL || !(picker->flags & IP_PICKER_RESUMABLE))
        return NULL;

    IpPickerValue *dict = IpPickerValue_New(IPPICKER_VALUE_MAP, NULL);
    IpPickerValue *list = IpPickerValue_New(IPPICKER_VALUE_ARRAY, NULL);
    IpPickerValue *width = IpPickerValue_NewUnsignedInt(picker->config.bucket_width);
    if (dict == NULL || list == NULL || width == NULL) {
        IpPickerValue_Free(dict);
        IpPickerValue_Free(list);
        IpPickerValue_Free(width);
        return NULL;
    }

    if (!IpPicker_BucketListAsValue(picker->eligible, list) ||
        !IpPicker_BucketListAsValue(picker->cooldown, list)) {
        IpPickerValue_Free(dict);
        IpPickerValue_Free(list);
        IpPickerValue_Free(width);
        return NULL;
    }

    IpPickerValue_Set(dict, "bucket_width", width);
    IpPickerValue_Set(dict, "buckets", list);
    free(width);
    free(list);

    char *json = IpPickerValue_ToJson(dict);
    IpPickerValue_Free(dict);
    return json;
}
