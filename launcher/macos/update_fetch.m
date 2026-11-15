/* update_fetch.h on macOS: NSURLSession, without a cache or cookies (an ephemeral session), compiled without ARC. */
#include "update_fetch.h"

#import <Foundation/Foundation.h>

#include <SDL3/SDL_stdinc.h>

#include <stdio.h>
#include <string.h>

/* What the completion handler leaves for the check's thread, which waits for it. */
typedef struct fetched {
    char *body;
    size_t length;
    int status;
    char error[256];
} fetched;

/* A redirection to https:// alone. */
@interface OraclesUpdateRedirect : NSObject <NSURLSessionTaskDelegate>
@end

@implementation OraclesUpdateRedirect
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response
        newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    (void)session; (void)task; (void)response;
    completionHandler([[request.URL.scheme lowercaseString] isEqualToString:@"https"] ? request : nil);
}
@end

int oracles_http_request(const OraclesHttpRequest *request, char **body, size_t *length, int *status, char *error, size_t capacity)
{
    fetched *got = SDL_calloc(1, sizeof *got);   /* the handler's, which runs on the session's queue */
    if (!got) { snprintf(error, capacity, "out of memory"); return 0; }
    const size_t max_body = request->max_body;
    @autoreleasepool {
        NSString *text = [NSString stringWithUTF8String:request->url];
        NSURL *address = text ? [NSURL URLWithString:text] : nil;
        NSString *scheme = address ? [address.scheme lowercaseString] : nil;
        const int loopback = request->loopback_http && [scheme isEqualToString:@"http"] && [address.host isEqualToString:@"127.0.0.1"];
        if (!address || !([scheme isEqualToString:@"https"] || loopback)) {
            snprintf(error, capacity, "not an https:// address");
            SDL_free(got);
            return 0;
        }
        NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration.timeoutIntervalForRequest = request->timeout_ms / 1000.0;
        configuration.timeoutIntervalForResource = request->timeout_ms / 1000.0;
        configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        NSMutableDictionary *headers = [NSMutableDictionary dictionary];
        headers[@"User-Agent"] = [NSString stringWithUTF8String:request->user_agent];
        if (request->accept) headers[@"Accept"] = [NSString stringWithUTF8String:request->accept];
        configuration.HTTPAdditionalHeaders = headers;
        NSMutableURLRequest *asked = [NSMutableURLRequest requestWithURL:address];
        asked.timeoutInterval = request->timeout_ms / 1000.0;
        asked.cachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        if (request->body) {
            asked.HTTPMethod = @"POST";
            asked.HTTPBody = [NSData dataWithBytes:request->body length:request->body_length];
            if (request->content_type) [asked setValue:[NSString stringWithUTF8String:request->content_type] forHTTPHeaderField:@"Content-Type"];
        }
        OraclesUpdateRedirect *redirect = [[OraclesUpdateRedirect alloc] init];
        NSURLSession *session = [NSURLSession sessionWithConfiguration:configuration delegate:redirect delegateQueue:nil];
        [redirect release];
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        NSURLSessionDataTask *task = [session dataTaskWithRequest:asked completionHandler:^(NSData *data, NSURLResponse *response, NSError *failure) {
            if (failure) snprintf(got->error, sizeof got->error, "%s", failure.localizedDescription.UTF8String ? failure.localizedDescription.UTF8String : "failed");
            else if (![response isKindOfClass:[NSHTTPURLResponse class]]) snprintf(got->error, sizeof got->error, "not an HTTP answer");
            else if (data.length > max_body) snprintf(got->error, sizeof got->error, "an answer larger than %lu bytes", (unsigned long)max_body);
            else if ((got->body = SDL_malloc(data.length + 1)) != NULL) {
                if (data.length) memcpy(got->body, data.bytes, data.length);
                got->body[data.length] = 0;
                got->length = data.length;
                got->status = (int)((NSHTTPURLResponse *)response).statusCode;
            } else snprintf(got->error, sizeof got->error, "out of memory");
            dispatch_semaphore_signal(done);
        }];
        [task resume];
        /* The session's own timeout ends the task, which always calls its handler. */
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
        dispatch_release(done);
        [session finishTasksAndInvalidate];
    }
    const int ok = got->body != NULL;
    if (ok) {
        *body = got->body;
        *length = got->length;
        *status = got->status;
    } else snprintf(error, capacity, "%s", got->error);
    SDL_free(got);
    return ok;
}
