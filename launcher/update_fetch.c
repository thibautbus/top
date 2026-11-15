#include "update_fetch.h"

#include <SDL3/SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#elif defined(__ANDROID__)
#include <jni.h>
#endif

#if defined(_WIN32) || defined(__linux__)
/* What the server sent so far, zero-terminated, at most `max` bytes. */
typedef struct received {
    char *data;
    size_t length, capacity, max;
    int too_large;
} received;

static int append(received *r, const char *bytes, size_t count)
{
    if (r->length + count > r->max) { r->too_large = 1; return 0; }
    if (r->length + count + 1 > r->capacity) {
        size_t capacity = r->capacity ? r->capacity : 16384;
        while (capacity < r->length + count + 1) capacity *= 2;
        char *data = SDL_realloc(r->data, capacity);
        if (!data) return 0;
        r->data = data;
        r->capacity = capacity;
    }
    memcpy(r->data + r->length, bytes, count);
    r->length += count;
    r->data[r->length] = 0;
    return 1;
}

/* The answer handed over, an empty body included. */
static int answered(received *r, char **body, size_t *length)
{
    if (!r->data && !append(r, "", 0)) return 0;
    *body = r->data;
    *length = r->length;
    return 1;
}

static void too_large_or_memory(const received *r, char *error, size_t capacity)
{
    if (r->too_large) snprintf(error, capacity, "an answer larger than %lu bytes", (unsigned long)r->max);
    else snprintf(error, capacity, "out of memory");
}
#endif

/* The tests' local server alone, never the network's. */
static int loopback_http(const OraclesHttpRequest *request)
{
    return request->loopback_http && (!strncmp(request->url, "http://127.0.0.1:", 17) || !strncmp(request->url, "http://127.0.0.1/", 17));
}

#if defined(_WIN32)

static void winhttp_error(const char *step, char *error, size_t capacity)
{
    const DWORD code = GetLastError();
    const char *name = code == ERROR_WINHTTP_TIMEOUT ? ", timed out" : code == ERROR_WINHTTP_NAME_NOT_RESOLVED ? ", the name not resolved"
                       : code == ERROR_WINHTTP_CANNOT_CONNECT ? ", no connection" : code == ERROR_WINHTTP_SECURE_FAILURE ? ", the certificate refused"
                       : code == ERROR_WINHTTP_CONNECTION_ERROR ? ", the connection reset" : "";
    snprintf(error, capacity, "WinHTTP, %s: error %lu%s", step, (unsigned long)code, name);
}

/* `text` as UTF-16 into `out`, `capacity` characters; 0 when it does not fit. */
static int wide_text(const char *text, wchar_t *out, int capacity)
{
    return MultiByteToWideChar(CP_UTF8, 0, text, -1, out, capacity) > 0;
}

int oracles_http_request(const OraclesHttpRequest *request, char **body, size_t *length, int *status, char *error, size_t capacity)
{
    wchar_t wide[2048], host[256], path[2048], agent[256], header[512];
    URL_COMPONENTS parts;
    memset(&parts, 0, sizeof parts);
    parts.dwStructSize = sizeof parts;
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    if (!wide_text(request->url, wide, 2048) || !WinHttpCrackUrl(wide, 0, 0, &parts)
        || (parts.nScheme != INTERNET_SCHEME_HTTPS && !(parts.nScheme == INTERNET_SCHEME_HTTP && loopback_http(request)))) {
        snprintf(error, capacity, "not an https:// address");
        return 0;
    }
    if (!wide_text(request->user_agent, agent, 256)) { snprintf(error, capacity, "a User-Agent too long"); return 0; }
    /* The automatic proxy from Windows 8.1, the one configured before. */
    HINTERNET session = WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) session = WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connection = NULL, request_handle = NULL;
    received r = { NULL, 0, 0, request->max_body, 0 };
    int ok = 0, headers = 1;
    DWORD code = 0, size = sizeof code;
    const DWORD sent = request->body ? (DWORD)request->body_length : 0;
    if (!session) winhttp_error("open", error, capacity);
    else if (!WinHttpSetTimeouts(session, request->timeout_ms, request->timeout_ms, request->timeout_ms, request->timeout_ms)
             || !(connection = WinHttpConnect(session, host, parts.nPort, 0))) winhttp_error("connect", error, capacity);
    else if (!(request_handle = WinHttpOpenRequest(connection, request->body ? L"POST" : L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                   parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0))) winhttp_error("request", error, capacity);
    else {
        char text[512];
        if (request->accept) {
            snprintf(text, sizeof text, "Accept: %s", request->accept);
            headers = wide_text(text, header, 512) && WinHttpAddRequestHeaders(request_handle, header, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        }
        if (headers && request->body && request->content_type) {
            snprintf(text, sizeof text, "Content-Type: %s", request->content_type);
            headers = wide_text(text, header, 512) && WinHttpAddRequestHeaders(request_handle, header, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        }
        if (!headers || !WinHttpSendRequest(request_handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, request->body ? (LPVOID)request->body : WINHTTP_NO_REQUEST_DATA,
                                            sent, sent, 0)
            || !WinHttpReceiveResponse(request_handle, NULL)) winhttp_error("request", error, capacity);
        else if (!WinHttpQueryHeaders(request_handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &size,
                                      WINHTTP_NO_HEADER_INDEX))
            winhttp_error("status", error, capacity);
        else {
            ok = 1;
            for (;;) {
                char chunk[16384];
                DWORD read = 0;
                if (request->cancel && SDL_GetAtomicInt(request->cancel)) { snprintf(error, capacity, "cancelled"); ok = 0; break; }
                if (!WinHttpReadData(request_handle, chunk, sizeof chunk, &read)) { winhttp_error("read", error, capacity); ok = 0; break; }
                if (!read) break;
                if (!append(&r, chunk, read)) { too_large_or_memory(&r, error, capacity); ok = 0; break; }
            }
        }
    }
    if (request_handle) WinHttpCloseHandle(request_handle);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    if (ok && answered(&r, body, length)) { *status = (int)code; return 1; }
    SDL_free(r.data);
    return 0;
}

#elif defined(__ANDROID__)

static jstring text_or_null(JNIEnv *env, const char *text)
{
    return text ? (*env)->NewStringUTF(env, text) : NULL;
}

int oracles_http_request(const OraclesHttpRequest *request, char **body, size_t *length, int *status, char *error, size_t capacity)
{
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    jobject activity = SDL_GetAndroidActivity();
    int ok = 0;
    snprintf(error, capacity, "the activity cannot be called");
    if (!env || !activity) return 0;
    jclass class = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetStaticMethodID(env, class, "httpRequest",
                                                 "(Ljava/lang/String;[BLjava/lang/String;Ljava/lang/String;Ljava/lang/String;IIZ)[B");
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    jstring url = text_or_null(env, request->url), content_type = text_or_null(env, request->content_type);
    jstring accept = text_or_null(env, request->accept), agent = text_or_null(env, request->user_agent);
    jbyteArray sent = request->body ? (*env)->NewByteArray(env, (jsize)request->body_length) : NULL;
    if (sent) (*env)->SetByteArrayRegion(env, sent, 0, (jsize)request->body_length, (const jbyte *)request->body);
    jbyteArray answer = method && url && agent && (sent || !request->body)
                        ? (*env)->CallStaticObjectMethod(env, class, method, url, sent, content_type, accept, agent, (jint)request->timeout_ms,
                                                         (jint)request->max_body, (jboolean)(loopback_http(request) ? JNI_TRUE : JNI_FALSE))
                        : NULL;
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    /* "STATUS\nBODY", or "0\nREASON". */
    const jsize size = answer ? (*env)->GetArrayLength(env, answer) : 0;
    jbyte *bytes = answer ? (*env)->GetByteArrayElements(env, answer, NULL) : NULL;
    if (bytes) {
        const char *text = (const char *)bytes;
        jsize at = 0;
        long code = 0;
        while (at < size && text[at] >= '0' && text[at] <= '9') code = code * 10 + (text[at++] - '0');
        if (at < size && text[at] == '\n') at++;
        received r = { NULL, 0, 0, request->max_body, 0 };
        if (code <= 0) snprintf(error, capacity, "%.*s", (int)(size - at), text + at);
        else if (!append(&r, text + at, (size_t)(size - at)) || !answered(&r, body, length)) {
            too_large_or_memory(&r, error, capacity);
            SDL_free(r.data);
        } else {
            *status = (int)code;
            ok = 1;
        }
        (*env)->ReleaseByteArrayElements(env, answer, bytes, JNI_ABORT);
    }
    if (answer) (*env)->DeleteLocalRef(env, answer);
    if (sent) (*env)->DeleteLocalRef(env, sent);
    if (agent) (*env)->DeleteLocalRef(env, agent);
    if (accept) (*env)->DeleteLocalRef(env, accept);
    if (content_type) (*env)->DeleteLocalRef(env, content_type);
    if (url) (*env)->DeleteLocalRef(env, url);
    (*env)->DeleteLocalRef(env, class);
    (*env)->DeleteLocalRef(env, activity);
    return ok;
}

#elif defined(__linux__)

/* libcurl's interface as its ABI has it, libcurl.so.4 since 2006: no header, and nothing linked, the library loaded at
 * the check, so that the launcher still runs where there is none. */
typedef struct curl_slist curl_slist;
#define CURLOPT_WRITEDATA 10001
#define CURLOPT_POSTFIELDS 10015
#define CURLOPT_POSTFIELDSIZE_LARGE 30120
#define CURLOPT_URL 10002
#define CURLOPT_WRITEFUNCTION 20011
#define CURLOPT_USERAGENT 10018
#define CURLOPT_HTTPHEADER 10023
#define CURLOPT_NOPROGRESS 43
#define CURLOPT_FOLLOWLOCATION 52
#define CURLOPT_XFERINFODATA 10057
#define CURLOPT_MAXREDIRS 68
#define CURLOPT_NOSIGNAL 99
#define CURLOPT_TIMEOUT_MS 155
#define CURLOPT_CONNECTTIMEOUT_MS 156
#define CURLOPT_PROTOCOLS 181
#define CURLOPT_REDIR_PROTOCOLS 182
#define CURLOPT_XFERINFOFUNCTION 20219
#define CURLINFO_RESPONSE_CODE 0x200002
#define CURLPROTO_HTTP 1L
#define CURLPROTO_HTTPS 2L
#define CURLE_WRITE_ERROR 23
#define CURLE_ABORTED_BY_CALLBACK 42

typedef struct curl_calls {
    int (*global_init)(long);
    void *(*easy_init)(void);
    int (*easy_setopt)(void *, int, ...);
    int (*easy_perform)(void *);
    int (*easy_getinfo)(void *, int, ...);
    void (*easy_cleanup)(void *);
    const char *(*easy_strerror)(int);
    curl_slist *(*slist_append)(curl_slist *, const char *);
    void (*slist_free_all)(curl_slist *);
} curl_calls;

/* Debian's and Ubuntu's libcurl4 (OpenSSL), else their libcurl3-gnutls; every other distribution's libcurl.so.4.  Never
 * unloaded: its TLS library may keep handlers until the process ends. */
static int load_curl(curl_calls *c)
{
    static const char *const names[] = { "libcurl.so.4", "libcurl-gnutls.so.4" };
    SDL_SharedObject *library = NULL;
    for (size_t i = 0; i < sizeof names / sizeof names[0] && !library; i++) library = SDL_LoadObject(names[i]);
    if (!library) return 0;
    c->global_init = (int (*)(long))SDL_LoadFunction(library, "curl_global_init");
    c->easy_init = (void *(*)(void))SDL_LoadFunction(library, "curl_easy_init");
    c->easy_setopt = (int (*)(void *, int, ...))SDL_LoadFunction(library, "curl_easy_setopt");
    c->easy_perform = (int (*)(void *))SDL_LoadFunction(library, "curl_easy_perform");
    c->easy_getinfo = (int (*)(void *, int, ...))SDL_LoadFunction(library, "curl_easy_getinfo");
    c->easy_cleanup = (void (*)(void *))SDL_LoadFunction(library, "curl_easy_cleanup");
    c->easy_strerror = (const char *(*)(int))SDL_LoadFunction(library, "curl_easy_strerror");
    c->slist_append = (curl_slist *(*)(curl_slist *, const char *))SDL_LoadFunction(library, "curl_slist_append");
    c->slist_free_all = (void (*)(curl_slist *))SDL_LoadFunction(library, "curl_slist_free_all");
    return c->global_init && c->easy_init && c->easy_setopt && c->easy_perform && c->easy_getinfo && c->easy_cleanup && c->easy_strerror && c->slist_append
           && c->slist_free_all;
}

static size_t curl_received(char *data, size_t size, size_t count, void *opaque)
{
    return append(opaque, data, size * count) ? size * count : 0;
}

/* Called during the transfer, about once a second at the least: a nonzero answer ends it. */
static int curl_progress(void *opaque, int64_t down_total, int64_t down, int64_t up_total, int64_t up)
{
    (void)down_total; (void)down; (void)up_total; (void)up;
    return SDL_GetAtomicInt(opaque) != 0;
}

/* The cancel flag, or one never set. */
static SDL_AtomicInt never_cancelled;

#define CURL_GLOBAL_DEFAULT 3L   /* CURL_GLOBAL_SSL | CURL_GLOBAL_WIN32 */

/* libcurl loaded and initialised once for every thread that asks (the update check's, the achievements'):
 * curl_global_init is not safe to call from two threads at once before libcurl 7.84. */
static SDL_InitState curl_state;
static curl_calls curl_api;
static int curl_ready;

static const curl_calls *curl_loaded(void)
{
    if (SDL_ShouldInit(&curl_state)) {
        curl_ready = load_curl(&curl_api) && curl_api.global_init(CURL_GLOBAL_DEFAULT) == 0;
        SDL_SetInitialized(&curl_state, true);
    }
    return curl_ready ? &curl_api : NULL;
}

int oracles_http_request(const OraclesHttpRequest *request, char **body, size_t *length, int *status, char *error, size_t capacity)
{
    const curl_calls *loaded = curl_loaded();
    if (!loaded) { snprintf(error, capacity, "no libcurl.so.4 on this system"); return 0; }
    const curl_calls c = *loaded;
    void *curl = c.easy_init();
    char line[512];
    curl_slist *headers = NULL;
    int listed = 1;
    if (request->accept) {
        snprintf(line, sizeof line, "Accept: %s", request->accept);
        listed = (headers = c.slist_append(headers, line)) != NULL;
    }
    if (listed && request->body && request->content_type) {
        snprintf(line, sizeof line, "Content-Type: %s", request->content_type);
        curl_slist *more = c.slist_append(headers, line);
        listed = more != NULL;
        if (more) headers = more;
    }
    if (!curl || !listed) {
        if (curl) c.easy_cleanup(curl);
        if (headers) c.slist_free_all(headers);
        snprintf(error, capacity, "libcurl cannot start");
        return 0;
    }
    received r = { NULL, 0, 0, request->max_body, 0 };
    c.easy_setopt(curl, CURLOPT_URL, request->url);
    if (headers) c.easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    c.easy_setopt(curl, CURLOPT_USERAGENT, request->user_agent);
    if (request->body) {
        c.easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (int64_t)request->body_length);
        c.easy_setopt(curl, CURLOPT_POSTFIELDS, request->body);
    }
    c.easy_setopt(curl, CURLOPT_PROTOCOLS, loopback_http(request) ? CURLPROTO_HTTPS | CURLPROTO_HTTP : CURLPROTO_HTTPS);
    c.easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    c.easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    c.easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    c.easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);   /* a thread of its own: no alarm for the name's resolution */
    c.easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)request->timeout_ms);
    c.easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)request->timeout_ms);
    c.easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_received);
    c.easy_setopt(curl, CURLOPT_WRITEDATA, &r);
    c.easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    c.easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress);
    c.easy_setopt(curl, CURLOPT_XFERINFODATA, request->cancel ? request->cancel : &never_cancelled);
    const int result = c.easy_perform(curl);
    long code = 0;
    c.easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    c.easy_cleanup(curl);
    if (headers) c.slist_free_all(headers);
    if (result == 0 && code > 0 && answered(&r, body, length)) { *status = (int)code; return 1; }
    if (result == CURLE_WRITE_ERROR && r.too_large) too_large_or_memory(&r, error, capacity);
    else if (result == CURLE_ABORTED_BY_CALLBACK) snprintf(error, capacity, "cancelled");
    else snprintf(error, capacity, "libcurl: %s", result ? c.easy_strerror(result) : "no status");
    SDL_free(r.data);
    return 0;
}

#else

int oracles_http_request(const OraclesHttpRequest *request, char **body, size_t *length, int *status, char *error, size_t capacity)
{
    (void)request; (void)body; (void)length; (void)status; (void)loopback_http;
    snprintf(error, capacity, "no HTTP client on this system");
    return 0;
}

#endif
