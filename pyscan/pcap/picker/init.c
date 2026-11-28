#include "init.h"
#include "resume.h"
#include "str.h"
#include "picker.h"
#include <stdlib.h>

static int IpPicker_InitConfig(IpPicker *picker, char *cidr_text,
                               uint32_t bucket_width,
                               uint32_t cooldown_period, IpPickerFlags flags)
{
    if (picker == NULL || cidr_text == NULL || bucket_width == 0)
        return 0;

    picker->config.cidr_text = cidr_text;
    picker->config.bucket_width = bucket_width;
    picker->config.cooldown_period = cooldown_period;
    picker->config.exclude_private_ranges = (flags & IP_PICKER_EXCLUDE_PRIVATE_RANGES) != 0;
    picker->flags = flags;
    return 1;
}

static int IpPicker_InitCidr(IpPicker *picker)
{
    picker->universe = CidrRangeNew(picker->config.cidr_text);
    return picker->universe != NULL;
}

static int IpPicker_InitBucketLists(IpPicker *picker, const char *resume_json)
{
    picker->cooldown = BucketListNew();
    if (picker->cooldown == NULL)
        return 0;

    if (resume_json != NULL) {
        if (!IpPicker_LoadBuckets(picker, resume_json))
            return 0;
        return 1;
    }
    return InitEligibleList(picker);
}

static int IpPicker_InitCooldownThread(IpPicker *picker)
{
    InitializeSRWLock(&picker->threading.lock);
    InitializeConditionVariable(&picker->threading.cooldown_nonempty);
    InitializeConditionVariable(&picker->threading.eligible_nonempty);
    picker->threading.stop = 0;

    picker->coowldown_worker = CreateThread(NULL, 0, IpPicker_CooldownWorker, picker, 0, NULL);
    return picker->coowldown_worker != NULL;
}

char *IpPicker_ConfigHighlight(IpPicker *picker)
{
    if (picker == NULL)
        return NULL;

    char *highlight = strf("IpPickerConfig(");
    if (highlight == NULL)
        return NULL;

    if (appendf(&highlight, "cidr=%s, bucket_width=%u, cooldown_period=%u, exclude_private_ranges=%s, resumable=%s)",
                picker->config.cidr_text != NULL ? picker->config.cidr_text : "(null)",
                (unsigned)picker->config.bucket_width,
                (unsigned)picker->config.cooldown_period,
                picker->config.exclude_private_ranges ? "true" : "false",
                (picker->flags & IP_PICKER_RESUMABLE) ? "true" : "false") == NULL) {
        free(highlight);
        return NULL;
    }

    return highlight;
}

static void IpPicker_InitFailureCleanup(IpPicker *picker, InitCallbackHandle *callback_handle)
{
    if (picker != NULL) {
        BucketListFree(picker->cooldown);
        BucketListFree(picker->eligible);
        free(picker->universe);
        free(picker);
    }
    InitCallbackRequestStop(callback_handle);
    InitCallbackWait(callback_handle);
}

IpPicker *IpPicker_Initialize(char *cidr_text, uint32_t bucket_width,
                              uint32_t cooldown_period,
                              const char *resume_json, IpPickerFlags flags,
                              InitCallback callback, void *callback_arg)
{
    InitCallbackHandle *callback_handle = InitCallbackStart(callback, callback_arg);
    if (callback_handle == NULL)
        return NULL;

    IpPicker *picker = calloc(1, sizeof *picker);
    if (picker == NULL)
    {
        IpPicker_InitFailureCleanup(NULL, callback_handle);
        return NULL;
    }

    if (!IpPicker_InitConfig(picker, cidr_text, bucket_width,
                             cooldown_period, flags))
    {
        IpPicker_InitFailureCleanup(picker, callback_handle);
        return NULL;
    }
    if (!IpPicker_InitCidr(picker) ||
        !IpPicker_InitBucketLists(picker, resume_json) ||
        !IpPicker_InitCooldownThread(picker))
    {
        IpPicker_InitFailureCleanup(picker, callback_handle);
        return NULL;
    }
    InitCallbackRequestStop(callback_handle);
    InitCallbackWait(callback_handle);
    return picker;

}


IpPicker *IpPicker_InitSimple(char *cidr, uint32_t max_hosts, uint32_t rate)
{
    return IpPicker_Initialize(cidr, max_hosts, rate, NULL, 0, NULL, NULL);
}
