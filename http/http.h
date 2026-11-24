#ifndef HTTP_HTTP_H
#define HTTP_HTTP_H

#include <stddef.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

typedef struct HttpFuture HttpFuture;

typedef enum {
    HTTP_SUCCESS = 0,
    HTTP_INVALID_ARGUMENT = 0x20000,
    HTTP_OUT_OF_MEMORY,
    HTTP_UNSUPPORTED_SCHEME,
    HTTP_INVALID_URL,
    HTTP_NAME_RESOLUTION_FAILED,
    HTTP_RESPONSE_TOO_LARGE,
    HTTP_INVALID_RESPONSE
} HttpStatus;

typedef struct {
    DWORD error;
    unsigned int status_code;
    char *data;
    size_t length;
} HttpResponse;

HttpFuture *http_get(const char *url);
HttpResponse http_await(HttpFuture *future);
HttpResponse http_get_sync(const char *url);
void http_future_destroy(HttpFuture *future);
void http_response_destroy(HttpResponse *response);

#endif
