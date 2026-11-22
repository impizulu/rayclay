#ifndef RC_NET_PROFILE_H
#define RC_NET_PROFILE_H

#include <stddef.h>

#define RCI_NET_PROFILE_ANCHOR rci_net_profile_link_rayclay_netprofile_f8c066f91756

struct RCI_NetProfile {

    void         *(*easyInit)(void);
    void          (*easyCleanup)(void *easy);
    unsigned int  (*easySetopt)(void *easy, unsigned int option, ...);
    unsigned int  (*easyGetinfo)(void *easy, unsigned int info, ...);
    unsigned int  (*easyPause)(void *easy, int action);
    void         *(*multiInit)(void);
    int           (*multiSetopt)(void *multi, unsigned int option, ...);
    int           (*multiAddHandle)(void *multi, void *easy);
    int           (*multiRemoveHandle)(void *multi, void *easy);
    int           (*multiPerform)(void *multi, int *running);
    int           (*multiWakeup)(void *multi);

    unsigned int  (*globalInitMem)(long flags, void *(*mallocCallback)(size_t), void (*freeCallback)(void *),
                                   void *(*reallocCallback)(void *, size_t),
                                   char *(*strdupCallback)(const char *),
                                   void *(*callocCallback)(size_t, size_t));

    const void   *(*versionInfo)(unsigned int age);
    void         *(*multiInfoRead)(void *multi, int *left);
    int           (*multiPoll)(void *multi, void *extraFds, unsigned int extraCount, int timeoutMs,
                               int *ready);
    void         *(*slistAppend)(void *list, const char *line);
    void          (*slistFreeAll)(void *list);

    const void   *(*wsMeta)(void *easy);
    unsigned int  (*wsStartFrame)(void *easy, unsigned int flags, size_t frameLength);

    int           (*setAlloc)(void *(*callocCallback)(size_t, size_t), void (*freeCallback)(void *));

    unsigned int  (*sslCtx)(void *curl, void *sslCtx, void *user);

    int           (*trustLoad)(const char *const *candidates);
    int           (*noStore)(void);

    int           (*setVerifier)(int (*verify)(const void *const *der, const size_t *derLength, size_t count,
                                               const char *host));

    const char    *profile;
};

#endif
