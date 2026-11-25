#include <windows.h>
#include "credit.h"

static Cred creds[30] = {0};
static SRWLOCK cred_lock = SRWLOCK_INIT;
static int creds_count = 0;
static int current_idx = 0;

BOOL GetNextCred(Cred *out_cred) {
    if (out_cred == NULL) {
        return FALSE;
    }

    BOOL success = FALSE;

    AcquireSRWLockExclusive(&cred_lock);

    if (current_idx < creds_count && current_idx < 30) {
        *out_cred = creds[current_idx++];
        success = TRUE;
    } else {
        // Optional: zero out the struct if out of bounds
        out_cred->username = NULL;
        out_cred->password = NULL;
    }

    ReleaseSRWLockExclusive(&cred_lock);
    return success;
}
