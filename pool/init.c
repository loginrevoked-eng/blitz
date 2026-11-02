#include "init.h"
#include "resume.h"
#include "str/str.h"
#include "picker.h"
#include <stdlib.h>

static int IpPicker_InitConfig(IpPicker *picker, char *cidr_text, uint32_t bucket_width, uint32_t cooldown_period, int exclude_private_ranges, char *resume_file, int resumable)
{
    if (picker == NULL || cidr_text == NULL || bucket_width == 0)
        return 0;

    picker->config.cidr_text = cidr_text;
    picker->config.bucket_width = bucket_width;
    picker->config.cooldown_period = cooldown_period;
    picker->config.exclude_private_ranges = exclude_private_ranges;
    picker->config.resume_file = resume_file;
    picker->config.resumeable = resumable;
    return 1;
}

static int IpPicker_InitCidr(IpPicker *picker)
{
    picker->universe = CidrRangeNew(picker->config.cidr_text);
    return picker->universe != NULL;
}

static int IpPicker_InitBucketLists(IpPicker *picker)
{
    picker->cooldown = BucketListNew();
    return picker->cooldown != NULL && InitEligibleList(picker) && IpPicker_ResumeBuckets(picker);
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

    if (appendf(&highlight, "cidr=%s, bucket_width=%u, cooldown_period=%u, exclude_private_ranges=%s, resumable=%s, resume_file=%s)",
                picker->config.cidr_text != NULL ? picker->config.cidr_text : "(null)",
                (unsigned)picker->config.bucket_width,
                (unsigned)picker->config.cooldown_period,
                picker->config.exclude_private_ranges ? "true" : "false",
                picker->config.resumeable ? "true" : "false",
                picker->config.resume_file != NULL ? picker->config.resume_file : "(null)") == NULL) {
        free(highlight);
        return NULL;
    }

    return highlight;
}

static void IpPicker_InitFailureCleanup(IpPicker *picker)
{
    BucketListFree(picker->cooldown);
    BucketListFree(picker->eligible);
    free(picker->universe);
    free(picker);
}

IpPicker *IpPicker_Initialize(char *cidr_text, uint32_t bucket_width, uint32_t cooldown_period, int exclude_private_ranges, char *resume_file, int resumable)
{
    IpPicker *picker = calloc(1, sizeof *picker);
    if (picker == NULL)
        return NULL;

    if (!IpPicker_InitConfig(picker, cidr_text, bucket_width, cooldown_period, exclude_private_ranges, resume_file, resumable))
    {
        IpPicker_InitFailureCleanup(picker);
        return NULL;
    }
    if (!IpPicker_InitCidr(picker) || !IpPicker_InitBucketLists(picker) || !IpPicker_InitCooldownThread(picker))
    {
        IpPicker_InitFailureCleanup(picker);
        return NULL;
    }
    return picker;

}
