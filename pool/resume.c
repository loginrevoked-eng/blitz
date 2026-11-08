#include "value/value.h"
#include "resume.h"

#include "deser.h"
#include "fs.h"
#include "json.h"
#include "value.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int IpPicker_LoadBuckets(IpPicker *picker)
{
    if (picker == NULL)
        return 0;
    if (picker->config.resume_file == NULL)
        return 0;

    char *json = Fs_ReadFile(picker->config.resume_file, NULL);
    if (json == NULL)
        return 0;

    BucketList *loaded = BucketListNew();
    uint32_t bucket_width = 1;
    int valid = loaded != NULL && BucketListFromJson(json, loaded, &bucket_width) &&
        bucket_width == picker->config.bucket_width;
    free(json);
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

int IpPicker_DumpBuckets(IpPicker *picker)
{
    if (picker == NULL)
        return 0;
    if (!(picker->flags & IP_PICKER_RESUMABLE) || picker->dump_file == NULL)
        return 1;
    Value *dict = ValueNew(VALUE_MAP, NULL);
    Value *list = ValueNew(VALUE_ARRAY, NULL);
    Value *width = ValueNewUnsignedInt(picker->config.bucket_width);
    if (dict == NULL || list == NULL || width == NULL) {
        ValueFree(dict);
        ValueFree(list);
        ValueFree(width);
        return 0;
    }

    if (!BucketListAsValue(picker->eligible, list) ||
        !BucketListAsValue(picker->cooldown, list)) {
        ValueFree(dict);
        ValueFree(list);
        ValueFree(width);
        return 0;
    }

    ValueSet(dict, "bucket_width", width);
    ValueSet(dict, "buckets", list);
    free(width);
    free(list);

    char *json = ValueToJson(dict);
    ValueFree(dict);
    if (json == NULL)
        return 0;

    int written = Fs_WriteFile(picker->dump_file, json, strlen(json));
    free(json);
    return written;
}
