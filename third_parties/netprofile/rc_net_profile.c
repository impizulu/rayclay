#define _POSIX_C_SOURCE 200809L

#include "rc_net_profile_psa.h"
#include "rc_net_profile.h"

#include <curl/curl.h>
#include <mbedtls/build_info.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <zlib.h>

#include <pthread.h>
#include <stdlib.h>
#if !defined(__ANDROID__)
#include <sys/auxv.h>
#include <sys/stat.h>
#endif

#if defined(TF_PSA_CRYPTO_CONFIG_FILE) || defined(MBEDTLS_CONFIG_FILE) || defined(MBEDTLS_USER_CONFIG_FILE) || \
    !defined(TF_PSA_CRYPTO_USER_CONFIG_FILE)
#error "the network profile's bridge compiles with the profile's TF-PSA-Crypto configuration only"
#endif

static const char rci_net_profile_string[] = "curl/" LIBCURL_VERSION " mbedtls/" MBEDTLS_VERSION_STRING
                                             " zlib/" ZLIB_VERSION;

typedef int (*RCI_NetProfileVerify)(const void *const *der, const size_t *derLength, size_t count,
                                    const char *host);
static pthread_mutex_t rci_net_profile_lock = PTHREAD_MUTEX_INITIALIZER;
static RCI_NetProfileVerify rci_net_profile_verifier;
#if !defined(__ANDROID__)
static mbedtls_x509_crt rci_net_profile_chain;
static int rci_net_profile_loaded;
static int rci_net_profile_have;
static int rci_net_profile_missing;
#endif

#define RCI_NET_PROFILE_CHAIN_MAX MBEDTLS_X509_MAX_VERIFY_CHAIN_SIZE
#define RCI_NET_PROFILE_DER_MAX   MBEDTLS_SSL_IN_CONTENT_LEN

static void *(*rci_net_profile_calloc_cb)(size_t count, size_t size);
static unsigned long rci_net_profile_refused;

#if !defined(__ANDROID__)

static const char *const rci_net_profile_files[] = {
    "/etc/ssl/certs/ca-certificates.crt",
    "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
    "/etc/pki/tls/certs/ca-bundle.crt",
    "/etc/ssl/ca-bundle.pem",
    "/etc/pki/tls/cacert.pem",
    "/etc/ssl/cert.pem",
    NULL
};

static int rci_net_profile_is(const char *path, int dir)
{
    struct stat st;

    if (!path || !path[0] || stat(path, &st) != 0)
        return 0;
    return dir ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode);
}

static int rci_net_profile_has_cert(void)
{
    return rci_net_profile_chain.version != 0;
}

static const char *rci_net_profile_env(const char *name)
{
    const char *value;

    if (getauxval(AT_SECURE) != 0)
        return NULL;
    value = getenv(name);
    return (value && value[0]) ? value : NULL;
}

static int rci_net_profile_parse(const char *const *candidates)
{
    const char *const *list = candidates ? candidates : rci_net_profile_files;
    const char *env;
    size_t i;

    env = rci_net_profile_env("SSL_CERT_FILE");
    if (env) {
        if (rci_net_profile_is(env, 0))
            (void)mbedtls_x509_crt_parse_file(&rci_net_profile_chain, env);
        return rci_net_profile_has_cert();
    }
    env = rci_net_profile_env("SSL_CERT_DIR");
    if (env) {
        if (rci_net_profile_is(env, 1))
            (void)mbedtls_x509_crt_parse_path(&rci_net_profile_chain, env);
        return rci_net_profile_has_cert();
    }
    for (i = 0; list[i]; i++) {
        if (!rci_net_profile_is(list[i], 0))
            continue;
        (void)mbedtls_x509_crt_parse_file(&rci_net_profile_chain, list[i]);
        if (rci_net_profile_has_cert())
            return 1;
    }
    if (!candidates && rci_net_profile_is("/etc/ssl/certs", 1)) {
        (void)mbedtls_x509_crt_parse_path(&rci_net_profile_chain, "/etc/ssl/certs");
        if (rci_net_profile_has_cert())
            return 1;
    }
    return 0;
}

static int rci_net_profile_load_locked(const char *const *candidates)
{
    unsigned long refused = __atomic_load_n(&rci_net_profile_refused, __ATOMIC_SEQ_CST);
    int have;

    if (rci_net_profile_loaded)
        return rci_net_profile_have;
    mbedtls_x509_crt_init(&rci_net_profile_chain);
    have = rci_net_profile_parse(candidates);
    if (__atomic_load_n(&rci_net_profile_refused, __ATOMIC_SEQ_CST) != refused) {
        mbedtls_x509_crt_free(&rci_net_profile_chain);
        return -1;
    }
    rci_net_profile_have   = have;
    rci_net_profile_loaded = 1;
    return have;
}

static int rci_net_profile_trust_load(const char *const *candidates)
{
    int have;

    pthread_mutex_lock(&rci_net_profile_lock);
    have = rci_net_profile_load_locked(candidates);
    pthread_mutex_unlock(&rci_net_profile_lock);
    return have > 0;
}

static int rci_net_profile_no_store(void)
{
    int missing;

    pthread_mutex_lock(&rci_net_profile_lock);
    missing = rci_net_profile_missing;
    pthread_mutex_unlock(&rci_net_profile_lock);
    return missing;
}
#else

static int rci_net_profile_trust_load(const char *const *candidates)
{
    (void)candidates;
    return 0;
}

static int rci_net_profile_no_store(void)
{
    return 0;
}
#endif

static int rci_net_profile_set_verifier(RCI_NetProfileVerify verify)
{
    pthread_mutex_lock(&rci_net_profile_lock);
    rci_net_profile_verifier = verify;
    pthread_mutex_unlock(&rci_net_profile_lock);
    return 0;
}

static int rci_net_profile_verify(void *user, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
{
    const void *der[RCI_NET_PROFILE_CHAIN_MAX];
    size_t length[RCI_NET_PROFILE_CHAIN_MAX];
    size_t count = 0;
    RCI_NetProfileVerify verify;
    const mbedtls_x509_crt *c;

    if (depth > 0) {
        *flags = 0;
        return 0;
    }
    *flags &= MBEDTLS_X509_BADCERT_CN_MISMATCH;
    pthread_mutex_lock(&rci_net_profile_lock);
    verify = rci_net_profile_verifier;
    pthread_mutex_unlock(&rci_net_profile_lock);
    for (c = crt; c; c = c->next) {
        if (count == RCI_NET_PROFILE_CHAIN_MAX || c->raw.len == 0 || c->raw.len > RCI_NET_PROFILE_DER_MAX) {
            *flags |= MBEDTLS_X509_BADCERT_NOT_TRUSTED;
            return 0;
        }
        der[count]    = c->raw.p;
        length[count] = c->raw.len;
        count++;
    }
    if (!verify || !user || verify(der, length, count, (const char *)user) != 0)
        *flags |= MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    return 0;
}

static unsigned int rci_net_profile_ssl_ctx(void *curl, void *sslCtx, void *user)
{
    mbedtls_ssl_config *conf = (mbedtls_ssl_config *)sslCtx;
    RCI_NetProfileVerify verify;
#if !defined(__ANDROID__)
    int have;
#endif

    (void)curl;
    pthread_mutex_lock(&rci_net_profile_lock);
    verify = rci_net_profile_verifier;
    pthread_mutex_unlock(&rci_net_profile_lock);
    if (verify) {
        mbedtls_ssl_conf_verify(conf, rci_net_profile_verify, user);
#if defined(MBEDTLS_SSL_RENEGOTIATION)
        mbedtls_ssl_conf_renegotiation(conf, MBEDTLS_SSL_RENEGOTIATION_DISABLED);
#endif
        return (unsigned int)CURLE_OK;
    }
#if defined(__ANDROID__)
    (void)conf;
    (void)user;
    return (unsigned int)CURLE_PEER_FAILED_VERIFICATION;
#else
    pthread_mutex_lock(&rci_net_profile_lock);
    have = rci_net_profile_load_locked((const char *const *)user);
    if (have == 0)
        rci_net_profile_missing = 1;
    pthread_mutex_unlock(&rci_net_profile_lock);
    if (have <= 0)
        return (unsigned int)CURLE_SSL_CACERT_BADFILE;
    mbedtls_ssl_conf_ca_chain(conf, &rci_net_profile_chain, NULL);
    return (unsigned int)CURLE_OK;
#endif
}

static void *rci_net_profile_calloc(size_t count, size_t size)
{
    void *p = rci_net_profile_calloc_cb(count, size);

    if (!p)
        (void)__atomic_add_fetch(&rci_net_profile_refused, 1, __ATOMIC_SEQ_CST);
    return p;
}

static int rci_net_profile_set_alloc(void *(*callocCallback)(size_t, size_t), void (*freeCallback)(void *))
{
    rci_net_profile_calloc_cb = callocCallback;
    return mbedtls_platform_set_calloc_free(rci_net_profile_calloc, freeCallback);
}

static const void *rci_net_profile_version_info(unsigned int age)
{
    return curl_version_info((CURLversion)age);
}

static void *rci_net_profile_multi_info_read(void *multi, int *left)
{
    return curl_multi_info_read((CURLM *)multi, left);
}

static int rci_net_profile_multi_poll(void *multi, void *extraFds, unsigned int extraCount, int timeoutMs,
                                      int *ready)
{
    return (int)curl_multi_poll((CURLM *)multi, (struct curl_waitfd *)extraFds, extraCount, timeoutMs, ready);
}

static void *rci_net_profile_slist_append(void *list, const char *line)
{
    return curl_slist_append((struct curl_slist *)list, line);
}

static void rci_net_profile_slist_free_all(void *list)
{
    curl_slist_free_all((struct curl_slist *)list);
}

static const void *rci_net_profile_ws_meta(void *easy)
{
    return curl_ws_meta((CURL *)easy);
}

static unsigned int rci_net_profile_ws_start_frame(void *easy, unsigned int flags, size_t frameLength)
{
    return (unsigned int)curl_ws_start_frame((CURL *)easy, flags, (curl_off_t)frameLength);
}

__attribute__((visibility("hidden"))) const struct RCI_NetProfile RCI_NET_PROFILE_ANCHOR = {
    .easyInit          = curl_easy_init,
    .easyCleanup       = curl_easy_cleanup,
    .easySetopt        = curl_easy_setopt,
    .easyGetinfo       = curl_easy_getinfo,
    .easyPause         = curl_easy_pause,
    .multiInit         = curl_multi_init,
    .multiSetopt       = curl_multi_setopt,
    .multiAddHandle    = curl_multi_add_handle,
    .multiRemoveHandle = curl_multi_remove_handle,
    .multiPerform      = curl_multi_perform,
    .multiWakeup       = curl_multi_wakeup,
    .globalInitMem     = curl_global_init_mem,
    .versionInfo       = rci_net_profile_version_info,
    .multiInfoRead     = rci_net_profile_multi_info_read,
    .multiPoll         = rci_net_profile_multi_poll,
    .slistAppend       = rci_net_profile_slist_append,
    .slistFreeAll      = rci_net_profile_slist_free_all,
    .wsMeta            = rci_net_profile_ws_meta,
    .wsStartFrame      = rci_net_profile_ws_start_frame,
    .setAlloc          = rci_net_profile_set_alloc,
    .sslCtx            = rci_net_profile_ssl_ctx,
    .trustLoad         = rci_net_profile_trust_load,
    .noStore           = rci_net_profile_no_store,
    .setVerifier       = rci_net_profile_set_verifier,
    .profile           = rci_net_profile_string,
};
