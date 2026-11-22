/*
    server.h - a real server's sign-in, beside main.c's pretend one

    Off by default: with no server named, the form answers from main.c's pretend server,
    with nothing on a network. A server named in signin-server.txt, in the folder the app
    starts in, makes a password sign-in a real request, a session the server grants and ends.
    dev-server.py, in the example's folder, is one to run, and writes that file while it runs:

        python3 examples/ex12_signin_form/dev-server.py       (then start the app here)

        POST   /session  {"email", "password"}  200 {"token", "expiresIn"}, or 401
        GET    /me       Authorization: Bearer   200 {"email"}, or 401
        DELETE /session  Authorization: Bearer   204: the server forgets the token

    The token lives in this struct only, for as long as the server said: a token written
    to disk would outlive Sign out. Its expiry signs the user out, and every request after
    it would be refused. A native request sends no cookies, so the session travels in the
    Authorization header, which every host sends, the web's too. An identity provider's
    redirect back to the app arrives as RC_FILE_DIALOG_OPENED_BY_OS (docs/mobile.md).
*/
#ifndef APP_SERVER_H
#define APP_SERVER_H

#include "rayclay.h"
#include "signin.h"

#define SIGNIN_SERVER_FILE "signin-server.txt"

typedef enum { CALL_NONE, CALL_SIGN_IN, CALL_ME, CALL_SIGN_OUT } ServerCall;

typedef struct {
    const char *url;          /* the server, or NULL: the pretend one answers     */
    char        named[128];   /* what signin-server.txt says                     */
    RC_Fetch    call;         /* the request in flight, 0 for none               */
    ServerCall  kind;         /* what it asks                                    */
    char        token[96];    /* the bearer token, in memory only; "" signed out */
    double      until;        /* rcAppTime when the server ends the session      */
    char        via[96];      /* the account line the app shows                  */
} Server;

/* The server named in SIGNIN_SERVER_FILE, its first line: none on a phone or the web, whose
   app folders hold no such file, so the pretend server answers there. */
static inline void server_init(Server *sv) {
    void  *bytes = NULL;
    size_t length = 0, n = 0;

    if (rcLoadFile(SIGNIN_SERVER_FILE, &bytes, &length) == RC_FILE_OK) {
        const char *text = (const char *)bytes;

        while (n < length && n < sizeof sv->named - 1 && text[n] > ' ') n++;
        rcStrCopy(sv->named, text, n + 1);
        rcFreeFile(bytes);
    }
    sv->url = sv->named[0] ? sv->named : NULL;
}

/* One request to the server, the token in its Authorization header while there is one.
   rcFetch copies every string it is given, so the header can live on the stack. */
static inline void server_ask(Server *sv, RC_Arena *a, ServerCall kind, const char *method,
                              const char *path, const char *json) {
    const char        *url = rcFormat(a, "%s%s", sv->url, path).chars;
    RC_FetchHeaderPair auth[2] = { { "Authorization", rcFormat(a, "Bearer %s", sv->token).chars },
                                   { 0 } };

    rcFetchFree(sv->call);
    sv->kind = kind;
    sv->call = rcFetch(url, .method = method, .json = json, .timeoutSeconds = 10,
                       .headerPairs = sv->token[0] ? auth : NULL);
}

/* Sized for the fields at their caps with every byte escaped (\u00XX is six bytes); the
   writer is still checked, and a body it refuses is never sent. Returns false then. */
static inline bool server_sign_in(Server *sv, RC_Arena *a, const char *email,
                                  const char *password) {
    char          body[6 * (SIGNIN_EMAIL_CAP + SIGNIN_PASSWORD_CAP) + 32];
    RC_JsonWriter w = rcJsonWriter(body, sizeof body);
    const char   *json;

    rcJsonBeginObject(&w, NULL);
    rcJsonWriteString(&w, "email", email);
    rcJsonWriteString(&w, "password", password);
    rcJsonEnd(&w);
    sv->token[0] = '\0';
    json = rcJsonWriterFinish(&w);
    if (!json)
        return false;
    server_ask(sv, a, CALL_SIGN_IN, "POST", "/session", json);
    return true;
}

/* Sign out tells the server, so the token is dead there too, and forgets it here at once:
   the answer is not waited for. */
static inline void server_sign_out(Server *sv, RC_Arena *a) {
    if (sv->token[0]) server_ask(sv, a, CALL_SIGN_OUT, "DELETE", "/session", NULL);
    sv->token[0] = '\0';
}

/* Cancel: the request goes, and so does any answer it would have had. */
static inline void server_cancel(Server *sv) {
    rcFetchFree(sv->call);
    sv->call = 0;
    sv->kind = CALL_NONE;
}

/* Read the answer to the request in flight, once it is in. Returns what was asked, or
   CALL_NONE while it is still out; *refusal is NULL on success, else what to tell the user. */
static inline ServerCall server_answer(Server *sv, RC_Arena *a, double now,
                                       const char **refusal) {
    RC_FetchResult r = rcFetchResult(sv->call);
    ServerCall     kind = sv->kind;
    bool           got = r.state == RC_FETCH_DONE && r.ok;
    RC_Json        doc;

    *refusal = NULL;
    if (!sv->call || r.state == RC_FETCH_PENDING) return CALL_NONE;
    doc = rcJsonParse(got ? r.text : "", got ? r.length : 0);
    got = got && rcJsonType(doc) == RC_JSON_OBJECT;
    if (kind == CALL_SIGN_IN && got) {
        rcStrCopy(sv->token, rcJsonString(doc, "token", ""), sizeof sv->token);
        sv->until = now + rcJsonNumber(doc, "expiresIn", 0.0);
    } else if (kind == CALL_ME && got) {
        rcStrCopy(sv->via, rcFormat(a, "Signed in to %s as %s", sv->url,
                                    rcJsonString(doc, "email", "?")).chars, sizeof sv->via);
    }
    if (kind != CALL_SIGN_OUT && !(got && (kind == CALL_ME || sv->token[0]))) {
        *refusal = r.state != RC_FETCH_DONE ? r.message
                 : r.status != 401          ? "The server could not sign you in. Try again."
                 : kind == CALL_ME          ? "The server ended your session. Sign in again."
                                            : "That email and password don't match.";
        sv->token[0] = '\0';
    }
    rcJsonFree(doc);
    if (*refusal && *refusal == r.message) {   /* the request's own: keep it past the free */
        static char kept[160];

        rcStrCopy(kept, r.message ? r.message : "No answer from the server.", sizeof kept);
        *refusal = kept;
    }
    server_cancel(sv);
    return kind;
}

#endif /* APP_SERVER_H */
