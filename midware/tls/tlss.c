/*
    RExOS - embedded RTOS
    Copyright (c) 2011-2015, Alexey Kramarenko
    All rights reserved.
*/

#include "tls_private.h"
#include "sys_config.h"
#include "../../userspace/process.h"
#include "../../userspace/stdio.h"
#include "../../userspace/sys.h"
#include "../../userspace/io.h"
#include "../../userspace/so.h"
#include "../../userspace/tcp.h"
#include "../../userspace/endian.h"
#include <string.h>

void tlss_main();

typedef enum {
    //TODO: more states to go
    TLSS_STATE_CLIENT_HELLO,
    TLSS_STATE_SERVER_HELLO,
    TLSS_STATE_ESTABLISHED,
    TLSS_STATE_CLOSING
} TLSS_STATE;

typedef struct {
    TLS_PROTOCOL_VERSION version;
    TLS_RANDOM client_random;
    HANDLE handle;
    IO* rx;
    IO* tx;
    TLSS_STATE state;
} TLSS_TCB;

typedef struct {
    HANDLE tcpip, user;
    IO* io;
    IO* tx;
    SO tcbs;
    bool tx_busy;
} TLSS;

const REX __TLSS = {
    //name
    "TLS Server",
    //size
    TLS_PROCESS_SIZE,
    //priority - midware priority
    TLS_PROCESS_PRIORITY,
    //flags
    PROCESS_FLAGS_ACTIVE | REX_FLAG_PERSISTENT_NAME,
    //function
    tlss_main
};

#if (TLS_DEBUG_REQUESTS)
typedef enum {
    TLS_KEY_EXCHANGE_NULL,
    TLS_KEY_EXCHANGE_RSA,
    TLS_KEY_EXCHANGE_DH_DSS,
    TLS_KEY_EXCHANGE_DH_RSA,
    TLS_KEY_EXCHANGE_DHE_DSS,
    TLS_KEY_EXCHANGE_DHE_RSA,
    TLS_KEY_EXCHANGE_DH_anon,
    TLS_KEY_EXCHANGE_KRB5,
    TLS_KEY_EXCHANGE_PSK,
    TLS_KEY_EXCHANGE_DHE_PSK,
    TLS_KEY_EXCHANGE_RSA_PSK,
    TLS_KEY_EXCHANGE_UNKNOWN
} TLS_KEY_EXCHANGE;

typedef enum {
    TLS_CIPHER_NULL,
    TLS_CIPHER_RC4_40,
    TLS_CIPHER_RC4_128,
    TLS_CIPHER_RC2_CBC_40,
    TLS_CIPHER_IDEA_CBC,
    TLS_CIPHER_DES40_CBC,
    TLS_CIPHER_DES_CBC_40,
    TLS_CIPHER_DES_CBC,
    TLS_CIPHER_3DES_EDE_CBC,
    TLS_CIPHER_AES_128_CBC,
    TLS_CIPHER_AES_256_CBC,
    TLS_CIPHER_UNKNOWN
} TLS_CIPHER;

typedef enum {
    TLS_HASH_NULL,
    TLS_HASH_MD5,
    TLS_HASH_SHA,
    TLS_HASH_UNKNOWN
} TLS_HASH;

static const char* const __TLS_KEY_ECHANGE[] =                     {"NULL",
                                                                    "RSA",
                                                                    "DH_DSS",
                                                                    "DH_RSA",
                                                                    "DHE_DSS",
                                                                    "DHE_RSA",
                                                                    "DH_anon",
                                                                    "KRB5",
                                                                    "PSK",
                                                                    "DHE_PSK",
                                                                    "RSA_PSK"};


static const char* const __TLS_CIPHER[] =                          {"NULL",
                                                                    "RC4_40",
                                                                    "RC4_128",
                                                                    "RC2_CBC_40",
                                                                    "IDEA_CBC",
                                                                    "DES40_CBC",
                                                                    "DES_CBC_40",
                                                                    "DES_CBC",
                                                                    "3DES_EDE_CBC",
                                                                    "AES_128_CBC",
                                                                    "AES_256_CBC"};

static const char* const __TLS_HASH[] =                            {"NULL",
                                                                    "MD5",
                                                                    "SHA"};

static void tlss_dump(void* data, unsigned int len)
{
    int i;
    for (i = 0; i < len; ++i)
        printf("%02x", ((uint8_t*)data)[i]);
}

static void tlss_print_cipher_suite(uint16_t cypher_suite)
{
    TLS_KEY_EXCHANGE key_echange;
    TLS_CIPHER cipher;
    TLS_HASH hash;
    bool exported;

    switch (cypher_suite)
    {
    case TLS_RSA_EXPORT_WITH_RC4_40_MD5:
    case TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
    case TLS_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_anon_EXPORT_WITH_RC4_40_MD5:
    case TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_KRB5_EXPORT_WITH_DES_CBC_40_SHA:
    case TLS_KRB5_EXPORT_WITH_RC2_CBC_40_SHA:
    case TLS_KRB5_EXPORT_WITH_RC4_40_SHA:
    case TLS_KRB5_EXPORT_WITH_DES_CBC_40_MD5:
    case TLS_KRB5_EXPORT_WITH_RC2_CBC_40_MD5:
    case TLS_KRB5_EXPORT_WITH_RC4_40_MD5:
        exported = true;
        break;
    default:
        exported = false;
    }

    switch (cypher_suite)
    {
    case TLS_NULL_WITH_NULL_NULL:
        key_echange = TLS_KEY_EXCHANGE_NULL;
        break;
    case TLS_RSA_WITH_NULL_MD5:
    case TLS_RSA_WITH_NULL_SHA:
    case TLS_RSA_WITH_RC4_128_MD5:
    case TLS_RSA_WITH_RC4_128_SHA:
    case TLS_RSA_WITH_IDEA_CBC_SHA:
    case TLS_RSA_WITH_DES_CBC_SHA:
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_RSA_WITH_AES_128_CBC_SHA:
    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_RSA_EXPORT_WITH_RC4_40_MD5:
    case TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
    case TLS_RSA_EXPORT_WITH_DES40_CBC_SHA:
        key_echange = TLS_KEY_EXCHANGE_RSA;
        break;
    case TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_DSS_WITH_DES_CBC_SHA:
    case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
        key_echange = TLS_KEY_EXCHANGE_DH_DSS;
        break;
    case TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_RSA_WITH_DES_CBC_SHA:
    case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
        key_echange = TLS_KEY_EXCHANGE_DH_RSA;
        break;
    case TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DHE_DSS_WITH_DES_CBC_SHA:
    case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
        key_echange = TLS_KEY_EXCHANGE_DHE_DSS;
        break;
    case TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DHE_RSA_WITH_DES_CBC_SHA:
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        key_echange = TLS_KEY_EXCHANGE_DHE_RSA;
        break;
    case TLS_DH_anon_EXPORT_WITH_RC4_40_MD5:
    case TLS_DH_anon_WITH_RC4_128_MD5:
    case TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_anon_WITH_DES_CBC_SHA:
    case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_anon_WITH_AES_128_CBC_SHA:
    case TLS_DH_anon_WITH_AES_256_CBC_SHA:
        key_echange = TLS_KEY_EXCHANGE_DH_anon;
        break;
    case TLS_KRB5_WITH_DES_CBC_SHA:
    case TLS_KRB5_WITH_3DES_EDE_CBC_SHA:
    case TLS_KRB5_WITH_RC4_128_SHA:
    case TLS_KRB5_WITH_IDEA_CBC_SHA:
    case TLS_KRB5_WITH_DES_CBC_MD5:
    case TLS_KRB5_WITH_3DES_EDE_CBC_MD5:
    case TLS_KRB5_WITH_RC4_128_MD5:
    case TLS_KRB5_WITH_IDEA_CBC_MD5:
    case TLS_KRB5_EXPORT_WITH_DES_CBC_40_SHA:
    case TLS_KRB5_EXPORT_WITH_RC2_CBC_40_SHA:
    case TLS_KRB5_EXPORT_WITH_RC4_40_SHA:
    case TLS_KRB5_EXPORT_WITH_DES_CBC_40_MD5:
    case TLS_KRB5_EXPORT_WITH_RC2_CBC_40_MD5:
    case TLS_KRB5_EXPORT_WITH_RC4_40_MD5:
        key_echange = TLS_KEY_EXCHANGE_KRB5;
        break;
    case TLS_PSK_WITH_NULL_SHA:
        key_echange = TLS_KEY_EXCHANGE_PSK;
        break;
    case TLS_DHE_PSK_WITH_NULL_SHA:
        key_echange = TLS_KEY_EXCHANGE_DHE_PSK;
        break;
    case TLS_RSA_PSK_WITH_NULL_SHA:
        key_echange = TLS_KEY_EXCHANGE_RSA_PSK;
        break;
    default:
        key_echange = TLS_KEY_EXCHANGE_UNKNOWN;
    }

    switch (cypher_suite)
    {
    case TLS_NULL_WITH_NULL_NULL:
    case TLS_RSA_WITH_NULL_MD5:
    case TLS_RSA_WITH_NULL_SHA:
    case TLS_PSK_WITH_NULL_SHA:
    case TLS_DHE_PSK_WITH_NULL_SHA:
    case TLS_RSA_PSK_WITH_NULL_SHA:
        cipher = TLS_CIPHER_NULL;
        break;
    case TLS_RSA_EXPORT_WITH_RC4_40_MD5:
    case TLS_DH_anon_EXPORT_WITH_RC4_40_MD5:
    case TLS_KRB5_EXPORT_WITH_RC4_40_SHA:
    case TLS_KRB5_EXPORT_WITH_RC4_40_MD5:
        cipher = TLS_CIPHER_RC4_40;
        break;
    case TLS_RSA_WITH_RC4_128_MD5:
    case TLS_RSA_WITH_RC4_128_SHA:
    case TLS_DH_anon_WITH_RC4_128_MD5:
    case TLS_KRB5_WITH_RC4_128_SHA:
    case TLS_KRB5_WITH_RC4_128_MD5:
        cipher = TLS_CIPHER_RC4_128;
        break;
    case TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
    case TLS_KRB5_EXPORT_WITH_RC2_CBC_40_SHA:
    case TLS_KRB5_EXPORT_WITH_RC2_CBC_40_MD5:
        cipher = TLS_CIPHER_RC2_CBC_40;
        break;
    case TLS_RSA_WITH_IDEA_CBC_SHA:
    case TLS_KRB5_WITH_IDEA_CBC_SHA:
    case TLS_KRB5_WITH_IDEA_CBC_MD5:
        cipher = TLS_CIPHER_IDEA_CBC;
        break;
    case TLS_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
        cipher = TLS_CIPHER_DES40_CBC;
        break;
    case TLS_KRB5_EXPORT_WITH_DES_CBC_40_SHA:
    case TLS_KRB5_EXPORT_WITH_DES_CBC_40_MD5:
        cipher = TLS_CIPHER_DES_CBC_40;
        break;
    case TLS_RSA_WITH_DES_CBC_SHA:
    case TLS_DH_DSS_WITH_DES_CBC_SHA:
    case TLS_DH_RSA_WITH_DES_CBC_SHA:
    case TLS_DHE_DSS_WITH_DES_CBC_SHA:
    case TLS_DHE_RSA_WITH_DES_CBC_SHA:
    case TLS_DH_anon_WITH_DES_CBC_SHA:
    case TLS_KRB5_WITH_DES_CBC_SHA:
    case TLS_KRB5_WITH_DES_CBC_MD5:
        cipher = TLS_CIPHER_DES_CBC;
        break;
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
    case TLS_KRB5_WITH_3DES_EDE_CBC_SHA:
    case TLS_KRB5_WITH_3DES_EDE_CBC_MD5:
        cipher = TLS_CIPHER_3DES_EDE_CBC;
        break;
    case TLS_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_anon_WITH_AES_128_CBC_SHA:
        cipher = TLS_CIPHER_AES_128_CBC;
        break;
    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
    case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_anon_WITH_AES_256_CBC_SHA:
        cipher = TLS_CIPHER_AES_256_CBC;
        break;
    default:
        cipher = TLS_CIPHER_UNKNOWN;
    }

    switch (cypher_suite)
    {
    case TLS_NULL_WITH_NULL_NULL:
        hash = TLS_HASH_NULL;
        break;
    case TLS_RSA_WITH_NULL_MD5:
    case TLS_RSA_EXPORT_WITH_RC4_40_MD5:
    case TLS_RSA_WITH_RC4_128_MD5:
    case TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
    case TLS_DH_anon_EXPORT_WITH_RC4_40_MD5:
    case TLS_DH_anon_WITH_RC4_128_MD5:
    case TLS_KRB5_WITH_DES_CBC_MD5:
    case TLS_KRB5_WITH_3DES_EDE_CBC_MD5:
    case TLS_KRB5_WITH_RC4_128_MD5:
    case TLS_KRB5_WITH_IDEA_CBC_MD5:
    case TLS_KRB5_EXPORT_WITH_DES_CBC_40_MD5:
    case TLS_KRB5_EXPORT_WITH_RC2_CBC_40_MD5:
    case TLS_KRB5_EXPORT_WITH_RC4_40_MD5:
        hash = TLS_HASH_MD5;
        break;
    case TLS_RSA_WITH_NULL_SHA:
    case TLS_RSA_WITH_RC4_128_SHA:
    case TLS_RSA_WITH_IDEA_CBC_SHA:
    case TLS_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_RSA_WITH_DES_CBC_SHA:
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_DSS_WITH_DES_CBC_SHA:
    case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_RSA_WITH_DES_CBC_SHA:
    case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DHE_DSS_WITH_DES_CBC_SHA:
    case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DHE_RSA_WITH_DES_CBC_SHA:
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
    case TLS_DH_anon_WITH_DES_CBC_SHA:
    case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
    case TLS_KRB5_WITH_DES_CBC_SHA:
    case TLS_KRB5_WITH_3DES_EDE_CBC_SHA:
    case TLS_KRB5_WITH_RC4_128_SHA:
    case TLS_KRB5_WITH_IDEA_CBC_SHA:
    case TLS_KRB5_EXPORT_WITH_DES_CBC_40_SHA:
    case TLS_KRB5_EXPORT_WITH_RC2_CBC_40_SHA:
    case TLS_KRB5_EXPORT_WITH_RC4_40_SHA:
    case TLS_PSK_WITH_NULL_SHA:
    case TLS_DHE_PSK_WITH_NULL_SHA:
    case TLS_RSA_PSK_WITH_NULL_SHA:
    case TLS_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
    case TLS_DH_anon_WITH_AES_128_CBC_SHA:
    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
    case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DH_anon_WITH_AES_256_CBC_SHA:
        hash = TLS_HASH_SHA;
        break;
    default:
        hash = TLS_HASH_UNKNOWN;
    }
    if (key_echange != TLS_KEY_EXCHANGE_UNKNOWN)
    {
        printf("TLS_%s_", __TLS_KEY_ECHANGE[key_echange]);
        if (exported)
            printf("EXPORT_");
        printf("WITH_%s_%s\n", __TLS_CIPHER[cipher], __TLS_HASH[hash]);
    }
    else
        printf("{%#02X, %#02X}\n", (cypher_suite >> 8) & 0xff, cypher_suite & 0xff);
}

#endif //TLS_DEBUG_REQUESTS

static HANDLE tlss_create_tcb(TLSS* tlss, HANDLE handle)
{
    TLSS_TCB* tcb;
    HANDLE tcb_handle;
    //TODO: multiple sessions support
    if (so_count(&tlss->tcbs))
        return INVALID_HANDLE;
    tcb_handle = so_allocate(&tlss->tcbs);
    if (tcb_handle == INVALID_HANDLE)
        return INVALID_HANDLE;
    tcb = so_get(&tlss->tcbs, tcb_handle);
    tcb->handle = handle;
    tcb->rx = tcb->tx = NULL;
    tcb->state = TLSS_STATE_CLIENT_HELLO;
    tcb->version = TLS_PROTOCOL_VERSION_UNSUPPORTED;
    memset(&tcb->client_random, 0x00, sizeof(TLS_RANDOM));
    return tcb_handle;
}

//TODO: tlss_destroy_tcb

static TLSS_TCB* tlss_tcb_find(TLSS* tlss, HANDLE handle)
{
    TLSS_TCB* tcb;
    HANDLE tcb_handle;
    for (tcb_handle = so_first(&tlss->tcbs); tcb_handle != INVALID_HANDLE; tcb_handle = so_next(&tlss->tcbs, tcb_handle))
    {
        tcb = so_get(&tlss->tcbs, tcb_handle);
        if (tcb->handle == handle)
            return tcb;
    }
    return NULL;
}

static void tlss_rx(TLSS* tlss)
{
    HANDLE tcb_handle;
    TLSS_TCB* tcb;
    if (tlss->tcpip == INVALID_HANDLE)
        return;
    //TODO: read from listen handle
    tcb_handle = so_first(&tlss->tcbs);
    //no active listen handles
    if (tcb_handle == INVALID_HANDLE)
        return;
    io_reset(tlss->io);
    tcb = so_get(&tlss->tcbs, tcb_handle);
    tcp_read(tlss->tcpip, tcb->handle, tlss->io, TLS_IO_SIZE);
}

static void tlss_tx_alert(TLSS* tlss, TLSS_TCB* tcb, TLS_ALERT_LEVEL alert_level, TLS_ALERT_DESCRIPTION alert_description)
{
    printd("TODO: TLS alert\n");
}

static inline void tlss_init(TLSS* tlss)
{
    tlss->tcpip = INVALID_HANDLE;
    tlss->user = INVALID_HANDLE;
    tlss->io = NULL;
    tlss->tx = NULL;
    tlss->tx_busy = false;
    //relative time will be set on first clientHello request
    so_create(&tlss->tcbs, sizeof(TLSS_TCB), 1);
}

static inline void tlss_open(TLSS* tlss, HANDLE tcpip)
{
    if (tlss->tcpip != INVALID_HANDLE)
    {
        error(ERROR_ALREADY_CONFIGURED);
        return;
    }
    tlss->io = io_create(TLS_IO_SIZE + sizeof(TCP_STACK));
    tlss->tx = io_create(TLS_IO_SIZE + sizeof(TCP_STACK));
    tlss->tcpip = tcpip;
}

static inline void tlss_close(TLSS* tlss)
{
    if (tlss->tcpip == INVALID_HANDLE)
    {
        error(ERROR_NOT_CONFIGURED);
        return;
    }
    //TODO: flush & close all handles
    tlss->tcpip = INVALID_HANDLE;
    io_destroy(tlss->io);
    io_destroy(tlss->tx);
    tlss->io = NULL;
    tlss->tx = NULL;
}

static inline void tlss_request(TLSS* tlss, IPC* ipc)
{
    switch (HAL_ITEM(ipc->cmd))
    {
    case IPC_OPEN:
        tlss_open(tlss, (HANDLE)ipc->param2);
        break;
    case IPC_CLOSE:
        tlss_close(tlss);
        break;
    default:
        error(ERROR_NOT_SUPPORTED);
    }
}

static inline void tlss_tcp_open(TLSS* tlss, HANDLE handle)
{
    HANDLE tcb_handle = tlss_create_tcb(tlss, handle);
    if (tcb_handle == INVALID_HANDLE)
    {
        tcp_close(tlss->tcpip, handle);
        return;
    }
#if (TLS_DEBUG)
///    tcp_get_remote_addr(hss->tcpip, handle, &session->remote_addr);
    printf("TLS: new session from ");
///    ip_print(&session->remote_addr);
    printf("\n");
#endif //TLS_DEBUG
    tlss_rx(tlss);
}

static inline bool tlss_rx_change_cypher(TLSS* tlss, TLSS_TCB* tcb, void* data, unsigned short len)
{
    printd("TODO: change cypher\n");
    dump(data, len);
    return false;
}

static inline bool tlss_rx_alert(TLSS* tlss, TLSS_TCB* tcb, void* data, unsigned short len)
{
    printd("TODO: alert\n");
    dump(data, len);
    return false;
}

static inline bool tlss_rx_client_hello(TLSS* tlss, TLSS_TCB* tcb, void* data, unsigned short len)
{
    int i;
    unsigned short crypto_len;
    uint8_t* crypto;
    TLS_CLIENT_HELLO* hello = data;
    //1. Check state and clientHello header size
    if ((tcb->state != TLSS_STATE_CLIENT_HELLO) || (len < sizeof(TLS_CLIENT_HELLO)))
    {
        tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_UNEXPECTED_MESSAGE);
        return true;
    }
    //2. Check protocol version
    if ((hello->version.major < 3) || (hello->version.minor < 1))
    {
        tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_PROTOCOL_VERSION);
#if (TLS_DEBUG)
        printf("TLS: Protocol version too old\n");
#endif //TLS_DEBUG_REQUESTS
        return true;
    }
    if ((hello->version.major > 3) || (hello->version.minor > 3))
        tcb->version = TLS_PROTOCOL_1_2;
    else
        tcb->version = (TLS_PROTOCOL_VERSION)hello->version.minor;
    //3. Copy random
    memcpy(&tcb->client_random, &hello->random, sizeof(TLS_RANDOM));
    //4. Ignore session, just check size
    data += sizeof(TLS_CLIENT_HELLO);
    len -= sizeof(TLS_CLIENT_HELLO);
    if (len < hello->session_id_length + 2)
    {
        tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_UNEXPECTED_MESSAGE);
        return true;
    }
    data += hello->session_id_length;
    len -= hello->session_id_length;
    //5. decode cypher suites and apply
    crypto_len = be2short(data);
    data += 2;
    len -= 2;
    //also byte for encryption len
    if (len <= crypto_len)
    {
        tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_UNEXPECTED_MESSAGE);
        return true;
    }
    crypto = data;
    data += crypto_len;
    len -= crypto_len;
    for (i = 0; i < crypto_len; i += 2)
    {
        //TODO:
    }
    //TODO: decode encryption
    //TODO: decode extensions
    tcb->state = TLSS_STATE_SERVER_HELLO;
    dump(data, len);
    sleep_ms(100);
#if (TLS_DEBUG_REQUESTS)
    printf("TLS: clientHello\n");
    printf("Protocol version: %d.%d\n", hello->version.major - 2, hello->version.minor - 1);
    printf("Client random: ");
    tlss_dump(hello->random.gmt_unix_time_be, sizeof(uint32_t));
    printf(" ");
    tlss_dump(hello->random.random_bytes, TLS_RANDOM_SIZE);
    printf("\n");
    printf("Session ID: ");
    if (hello->session_id_length == 0)
        printf("NULL");
    else
        tlss_dump((uint8_t*)hello + sizeof(TLS_CLIENT_HELLO), hello->session_id_length);
    printf("\n");
    printf("Cypher suites: \n");
    for (i = 0; i < crypto_len; i += 2)
        tlss_print_cipher_suite(be2short(crypto + i));
#endif //TLS_DEBUG_REQUESTS
    return false;
}

static inline bool tlss_rx_handshakes(TLSS* tlss, TLSS_TCB* tcb, void* data, unsigned short len)
{
    unsigned short offset, len_cur;
    TLS_HANDSHAKE* handshake;
    void* data_cur;
    bool alert;
    //iterate through handshake messages
    for (offset = 0; offset < len; offset += len_cur + sizeof(TLS_HANDSHAKE))
    {
        if (len - offset < sizeof(TLS_HANDSHAKE))
        {
            tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_UNEXPECTED_MESSAGE);
            return true;
        }
        handshake = (TLS_HANDSHAKE*)((uint8_t*)data + offset);
        len_cur = be2short(handshake->message_length_be);
        if ((len_cur + sizeof(TLS_HANDSHAKE) > len - offset) || (handshake->reserved))
        {
            tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_UNEXPECTED_MESSAGE);
            return true;
        }
        data_cur = (uint8_t*)data + offset + sizeof(TLS_HANDSHAKE);
        switch (handshake->message_type)
        {
        case TLS_HANDSHAKE_CLIENT_HELLO:
            alert = tlss_rx_client_hello(tlss, tcb, data_cur, len_cur);
            break;
        //TODO: not sure about others this time
//        TLS_HANDSHAKE_HELLO_REQUEST:
//        TLS_HANDSHAKE_CLIENT_HELLO,
//        TLS_HANDSHAKE_CERTIFICATE = 11,
//        TLS_HANDSHAKE_SERVER_KEY_EXCHANGE,
//        TLS_HANDSHAKE_CERTIFICATE_REQUEST,
//        TLS_HANDSHAKE_SERVER_HELLO_DONE,
//        TLS_HANDSHAKE_CERTIFICATE_VERIFY,
//        TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE,
//        TLS_HANDSHAKE_FINISHED = 20
        default:
#if (TLS_DEBUG)
            printf("TLS: unexpected handshake type: %d\n", handshake->message_type);
#endif //TLS_DEBUG
            tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_UNEXPECTED_MESSAGE);
            alert = true;
        }
        if (alert)
            return true;
    }
    return false;
}

static inline bool tlss_rx_app(TLSS* tlss, TLSS_TCB* tcb, void* data, unsigned short len)
{
    printd("TODO: app\n");
    dump(data, len);
    return false;
}

static inline void tlss_tcp_rx(TLSS* tlss, HANDLE handle)
{
    bool alert;
    TLS_RECORD* rec;
    unsigned short len;
    void* data;
    TLSS_TCB* tcb = tlss_tcb_find(tlss, handle);
    alert = true;
    do {
        //closed before
        if (tcb == NULL)
        {
            alert = false;
            break;
        }
        //Empty records disabled by TLS
        if (tlss->io->data_size <= sizeof(TLS_RECORD))
        {
            tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_UNEXPECTED_MESSAGE);
            break;
        }
        rec = io_data(tlss->io);
        //check TLS 1.0 - 1.2
        if ((rec->version.major != 3) || (rec->version.minor == 0) || (rec->version.minor > 3))
        {
            tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_PROTOCOL_VERSION);
            break;
        }
        len = be2short(rec->record_length_be);
        if (len > tlss->io->data_size - sizeof(TLS_RECORD))
        {
            tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_UNEXPECTED_MESSAGE);
            break;
        }
        data = (uint8_t*)io_data(tlss->io) + sizeof(TLS_RECORD);
        //TODO: check state before
        switch (rec->content_type)
        {
        case TLS_CONTENT_CHANGE_CYPHER:
            alert = tlss_rx_change_cypher(tlss, tcb, data, len);
            break;
        case TLS_CONTENT_ALERT:
            alert = tlss_rx_alert(tlss, tcb, data, len);
            break;
        case TLS_CONTENT_HANDSHAKE:
            alert = tlss_rx_handshakes(tlss, tcb, data, len);
            break;
        case TLS_CONTENT_APP:
            alert = tlss_rx_app(tlss, tcb, data, len);
            break;
        default:
#if (TLS_DEBUG)
            printf("TLS: unexpected message type: %d\n", rec->content_type);
#endif //TLS_DEBUG
            tlss_tx_alert(tlss, tcb, TLS_ALERT_LEVEL_FATAL, TLS_ALERT_UNEXPECTED_MESSAGE);
        }

        //TODO: process after
    } while (false);
    if (!alert)
        tlss_rx(tlss);
}

static inline void tlss_tcp_request(TLSS* tlss, IPC* ipc)
{
    switch (HAL_ITEM(ipc->cmd))
    {
    case IPC_OPEN:
        tlss_tcp_open(tlss, (HANDLE)ipc->param1);
        break;
///    case IPC_CLOSE:
        //TODO:
    case IPC_READ:
        tlss_tcp_rx(tlss, (HANDLE)ipc->param1);
        break;
///    case IPC_WRITE:
        //TODO:
    default:
        printf("got from tcp\n");
        error(ERROR_NOT_SUPPORTED);
    }
}

static inline void tlss_user_listen(TLSS* tlss, IPC* ipc)
{
    //just forward to tcp
    if (tlss->tcpip == INVALID_HANDLE)
    {
        error(ERROR_NOT_CONFIGURED);
        return;
    }
    if (tlss->user != INVALID_HANDLE)
    {
        error(ERROR_ALREADY_CONFIGURED);
        return;
    }
    tlss->user = ipc->process;
    //just forward to tcp
    ipc->param2 = tcp_listen(tlss->tcpip, ipc->param1);
}

static inline void tlss_user_close_listen(TLSS* tlss, IPC* ipc)
{
    //just forward to tcp
    if (tlss->user == INVALID_HANDLE)
    {
        error(ERROR_NOT_CONFIGURED);
        return;
    }
    tlss->user = INVALID_HANDLE;
    //just forward to tcp
    tcp_close_listen(tlss->tcpip, ipc->param1);
}

static inline void tlss_user_request(TLSS* tlss, IPC* ipc)
{
    //TODO:
//    TCP_GET_REMOTE_ADDR
//    TCP_GET_REMOTE_PORT
//    TCP_GET_LOCAL_PORT

    switch (HAL_ITEM(ipc->cmd))
    {
    case TCP_LISTEN:
        tlss_user_listen(tlss, ipc);
        break;
    case TCP_CLOSE_LISTEN:
        tlss_user_close_listen(tlss, ipc);
        break;
///    case IPC_CLOSE:
        //TODO:
///    case IPC_READ:
        //TODO:
///    case IPC_WRITE:
        //TODO:
///    case IPC_FLUSH:
        //TODO:
    default:
        printf("tls user request\n");
        error(ERROR_NOT_SUPPORTED);
    }
}

void tlss_main()
{
    IPC ipc;
    TLSS tlss;
    tlss_init(&tlss);
#if (TLS_DEBUG)
    open_stdout();
#endif //HS_DEBUG

    for (;;)
    {
        ipc_read(&ipc);
        switch (HAL_GROUP(ipc.cmd))
        {
        case HAL_TLS:
            tlss_request(&tlss, &ipc);
            break;
        case HAL_TCP:
            if (ipc.process == tlss.tcpip)
                tlss_tcp_request(&tlss, &ipc);
            else
                tlss_user_request(&tlss, &ipc);
            break;
        default:
            error(ERROR_NOT_SUPPORTED);
            break;
        }
        ipc_write(&ipc);
    }
}
