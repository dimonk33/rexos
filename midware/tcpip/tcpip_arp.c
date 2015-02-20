/*
    RExOS - embedded RTOS
    Copyright (c) 2011-2015, Alexey Kramarenko
    All rights reserved.
*/

#include "tcpip_arp.h"
#include "tcpip_private.h"
#include "../../userspace/stdio.h"
#include "../../userspace/block.h"
#include "tcpip_mac.h"
#include "tcpip_ip.h"

#define ARP_HEADER_SIZE                             (2 + 2 + 1 + 1 + 2)

#define ARP_HRD(buf)                                (((buf)[0] << 8) | (buf)[1])
#define ARP_PRO(buf)                                (((buf)[2] << 8) | (buf)[3])
#define ARP_HLN(buf)                                ((buf)[4])
#define ARP_PLN(buf)                                ((buf)[5])
#define ARP_OP(buf)                                 (((buf)[6] << 8) | (buf)[7])
#define ARP_SHA(buf)                                ((MAC*)((buf) + 8))
#define ARP_SPA(buf)                                ((IP*)((buf) + 8 + 6))
#define ARP_THA(buf)                                ((MAC*)((buf) + 8 + 6 + 4))
#define ARP_TPA(buf)                                ((IP*)((buf) + 8 + 6 + 4 + 6))

#define ARP_CACHE_ITEM(tcpip, i)                    (((ARP_CACHE_ENTRY*)(array_data((tcpip)->arp.cache)))[(i)])
#define ARP_CACHE_ITEMS_COUNT(tcpip)                (array_size((tcpip)->arp.cache) / sizeof(ARP_CACHE_ENTRY))


static const MAC __MAC_BROADCAST =                  {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};
static const MAC __MAC_REQUEST =                    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

void tcpip_arp_init(TCPIP* tcpip)
{
    array_create(&tcpip->arp.cache, sizeof(ARP_CACHE_ENTRY));
}

static void arp_request(TCPIP* tcpip, const IP* ip)
{
#if (TCPIP_ARP_DEBUG_FLOW)
    printf("ARP: request to ");
    print_ip(ip);
    printf("\n\r");
#endif

    const MAC* own_mac;
    TCPIP_IO io;
    if (tcpip_mac_allocate_io(tcpip, &io) == NULL)
        return;
    //hrd
    io.buf[0] = (ARP_HRD_ETHERNET >> 8) & 0xff;
    io.buf[1] = ARP_HRD_ETHERNET & 0xff;
    //pro
    io.buf[2] = (ETHERTYPE_IP >> 8) & 0xff;
    io.buf[3] = ETHERTYPE_IP & 0xff;
    //hln
    io.buf[4] = MAC_SIZE;
    //pln
    io.buf[5] = IP_SIZE;
    //op
    io.buf[6] = (ARP_REQUEST >> 8) & 0xff;
    io.buf[7] = ARP_REQUEST & 0xff;
    //sha
    own_mac = tcpip_mac(tcpip);
    ARP_SHA(io.buf)->u32.hi = own_mac->u32.hi;
    ARP_SHA(io.buf)->u32.lo = own_mac->u32.lo;
    //spa
    ARP_SPA(io.buf)->u32.ip = tcpip_ip(tcpip)->u32.ip;
    //tha
    ARP_THA(io.buf)->u32.hi = __MAC_REQUEST.u32.hi;
    ARP_THA(io.buf)->u32.lo = __MAC_REQUEST.u32.lo;
    //tpa
    ARP_TPA(io.buf)->u32.ip = ip->u32.ip;

    io.size = ARP_HEADER_SIZE + MAC_SIZE * 2 + IP_SIZE * 2;
    tcpip_mac_tx(tcpip, &io, &__MAC_BROADCAST, ETHERTYPE_ARP);
}

static inline void arp_reply(TCPIP* tcpip, MAC* mac, IP* ip)
{
#if (TCPIP_ARP_DEBUG_FLOW)
    printf("ARP: reply to ");
    print_mac(mac);
    printf("(");
    print_ip(ip);
    printf(")\n\r");
#endif

    const MAC* own_mac;
    TCPIP_IO io;
    if (tcpip_mac_allocate_io(tcpip, &io) == NULL)
        return;
    //hrd
    io.buf[0] = (ARP_HRD_ETHERNET >> 8) & 0xff;
    io.buf[1] = ARP_HRD_ETHERNET & 0xff;
    //pro
    io.buf[2] = (ETHERTYPE_IP >> 8) & 0xff;
    io.buf[3] = ETHERTYPE_IP & 0xff;
    //hln
    io.buf[4] = MAC_SIZE;
    //pln
    io.buf[5] = IP_SIZE;
    //op
    io.buf[6] = (ARP_REPLY >> 8) & 0xff;
    io.buf[7] = ARP_REPLY & 0xff;
    //sha
    own_mac = tcpip_mac(tcpip);
    ARP_SHA(io.buf)->u32.hi = own_mac->u32.hi;
    ARP_SHA(io.buf)->u32.lo = own_mac->u32.lo;
    //spa
    ARP_SPA(io.buf)->u32.ip = tcpip_ip(tcpip)->u32.ip;
    //tha
    ARP_THA(io.buf)->u32.hi = mac->u32.hi;
    ARP_THA(io.buf)->u32.lo = mac->u32.lo;
    //tpa
    ARP_TPA(io.buf)->u32.ip = ip->u32.ip;

    io.size = ARP_HEADER_SIZE + MAC_SIZE * 2 + IP_SIZE * 2;
    tcpip_mac_tx(tcpip, &io, mac, ETHERTYPE_ARP);
}

static int tcpip_arp_index(TCPIP* tcpip, const IP* ip)
{
    int i;
    for (i = 0; i < ARP_CACHE_ITEMS_COUNT(tcpip); ++i)
    {
        if (ARP_CACHE_ITEM(tcpip, i).ip.u32.ip == ip->u32.ip)
            return i;
    }
    return -1;
}

static void tcpip_arp_remove(TCPIP* tcpip, int idx)
{
    IP ip;
    ip.u32.ip = 0;
    if ((ARP_CACHE_ITEM(tcpip, idx).mac.u32.hi == 0) && (ARP_CACHE_ITEM(tcpip, idx).mac.u32.lo == 0))
        ip.u32.ip = ARP_CACHE_ITEM(tcpip, idx).ip.u32.ip;
#if (TCPIP_ARP_DEBUG)
    else
    {
        printf("ARP: route removed ");
        print_ip(&ARP_CACHE_ITEM(tcpip, idx).ip);
        printf(" -> ");
        print_mac(&ARP_CACHE_ITEM(tcpip, idx).mac);
        printf("\n\r");
    }
#endif
    array_remove(&tcpip->arp.cache, idx * sizeof(ARP_CACHE_ENTRY), sizeof(ARP_CACHE_ENTRY));

    //inform route on incomplete ARP if not resolved
    if (ip.u32.ip)
        tcpip_route_arp_not_resolved(tcpip, &ip);
}

static void tcpip_arp_insert(TCPIP* tcpip, const IP* ip, const MAC* mac, unsigned int timeout)
{
    int i, idx;
    unsigned int ttl;
    //don't add dups
    if (tcpip_arp_index(tcpip, ip) >= 0)
        return;
    //remove first non-static if no place
    if (ARP_CACHE_ITEMS_COUNT(tcpip) == TCPIP_ARP_CACHE_SIZE_MAX)
    {
        //first is static, can't remove
        if (ARP_CACHE_ITEM(tcpip, 0).ttl == 0)
            return;
        tcpip_arp_remove(tcpip, 0);
    }
    idx = ARP_CACHE_ITEMS_COUNT(tcpip);
    ttl = 0;
    //add all static routes to the end of cache
    if (timeout)
    {
        ttl = tcpip_seconds(tcpip) + timeout;
        for (i = 0; i < ARP_CACHE_ITEMS_COUNT(tcpip); ++i)
            if ((ARP_CACHE_ITEM(tcpip, i).ttl > ttl) || (ARP_CACHE_ITEM(tcpip, i).ttl == 0))
            {
                idx = i;
                break;
            }
    }
    array_insert(&tcpip->arp.cache, idx * sizeof(ARP_CACHE_ENTRY), sizeof(ARP_CACHE_ENTRY));
    ARP_CACHE_ITEM(tcpip, idx).ip.u32.ip = ip->u32.ip;
    ARP_CACHE_ITEM(tcpip, idx).mac.u32.hi = mac->u32.hi;
    ARP_CACHE_ITEM(tcpip, idx).mac.u32.lo = mac->u32.lo;
    ARP_CACHE_ITEM(tcpip, idx).ttl = ttl;
#if (TCPIP_ARP_DEBUG)
    if (mac->u32.hi && mac->u32.lo)
    {
        printf("ARP: route added ");
        print_ip(ip);
        printf(" -> ");
        print_mac(mac);
        printf("\n\r");
    }
#endif
}

static void tcpip_arp_update(TCPIP* tcpip, const IP* ip, const MAC* mac)
{
    int idx = tcpip_arp_index(tcpip, ip);
    if (idx < 0)
        return;
    ARP_CACHE_ITEM(tcpip, idx).mac.u32.hi = mac->u32.hi;
    ARP_CACHE_ITEM(tcpip, idx).mac.u32.lo = mac->u32.lo;
    ARP_CACHE_ITEM(tcpip, idx).ttl = tcpip_seconds(tcpip) + TCPIP_ARP_CACHE_TIMEOUT;
#if (TCPIP_ARP_DEBUG)
    printf("ARP: route resolved ");
    print_ip(ip);
    printf(" -> ");
    print_mac(mac);
    printf("\n\r");
#endif
}

static bool tcpip_arp_lookup(TCPIP* tcpip, const IP* ip, MAC* mac)
{
    int idx = tcpip_arp_index(tcpip, ip);
    if (idx >= 0)
    {
        mac->u32.hi = ARP_CACHE_ITEM(tcpip, idx).mac.u32.hi;
        mac->u32.lo = ARP_CACHE_ITEM(tcpip, idx).mac.u32.lo;
        return true;
    }
    mac->u32.hi = 0;
    mac->u32.lo = 0;
    return false;
}

void tcpip_arp_link_event(TCPIP* tcpip, bool link)
{
    if (link)
    {
        //just for test
        IP ip;
        ip.u32.ip = IP_MAKE(192, 168, 1, 1);
        MAC mac;
        tcpip_arp_resolve(tcpip, &ip, &mac);
    }
    else
    {
        //flush ARP cache, except static routes
        while (ARP_CACHE_ITEMS_COUNT(tcpip) && ARP_CACHE_ITEM(tcpip, 0).ttl)
            tcpip_arp_remove(tcpip, 0);
    }
}

void tcpip_arp_timer(TCPIP* tcpip, unsigned int seconds)
{
    while (ARP_CACHE_ITEMS_COUNT(tcpip) && ARP_CACHE_ITEM(tcpip, 0).ttl && (ARP_CACHE_ITEM(tcpip, 0).ttl <= seconds))
        tcpip_arp_remove(tcpip, 0);
}

#if (SYS_INFO)
static inline void tcpip_arp_info(TCPIP* tcpip)
{
    int i;
    printf("TCPIP ARP info\n\r");
    if (ARP_CACHE_ITEMS_COUNT(tcpip))
    {
        printf("    IP           MAC            ttl\n\r");
        printf("---------------------------------------\n\r");
        for (i = 0; i < ARP_CACHE_ITEMS_COUNT(tcpip); ++i)
        {
            print_ip(&ARP_CACHE_ITEM(tcpip, i).ip);
            printf("  ");
            print_mac(&ARP_CACHE_ITEM(tcpip, i).mac);
            if (ARP_CACHE_ITEM(tcpip, i).ttl)
                printf("  %d\n\r", ARP_CACHE_ITEM(tcpip, i).ttl - tcpip_seconds(tcpip));
            else
                printf("  STATIC\n\r");
        }
    }
    else
        printf("No active routes found");
}
#endif

bool tcpip_arp_request(TCPIP* tcpip, IPC* ipc)
{
    bool need_post = false;
    switch (ipc->cmd)
    {
#if (SYS_INFO)
    case IPC_GET_INFO:
        tcpip_arp_info(tcpip);
        need_post = true;
        break;
#endif
    default:
        break;
    }
    return need_post;
}

void tcpip_arp_rx(TCPIP* tcpip, TCPIP_IO* io)
{
    if (io->size < ARP_HEADER_SIZE)
    {
        tcpip_release_io(tcpip, io);
        return;
    }
    if (ARP_PRO(io->buf) != ETHERTYPE_IP || ARP_HLN(io->buf) != MAC_SIZE || ARP_PLN(io->buf) != IP_SIZE)
    {
        tcpip_release_io(tcpip, io);
        return;
    }
    switch (ARP_OP(io->buf))
    {
    case ARP_REQUEST:
        if (ARP_TPA(io->buf)->u32.ip == tcpip_ip(tcpip)->u32.ip)
        {
            arp_reply(tcpip, ARP_SHA(io->buf), ARP_SPA(io->buf));
            //insert in cache
            tcpip_arp_insert(tcpip, ARP_SPA(io->buf), ARP_SHA(io->buf), TCPIP_ARP_CACHE_TIMEOUT);
        }
        break;
    case ARP_REPLY:
        if (mac_compare(tcpip_mac(tcpip), ARP_THA(io->buf)))
        {
            tcpip_arp_update(tcpip, ARP_SPA(io->buf), ARP_SHA(io->buf));
            tcpip_route_arp_resolved(tcpip, ARP_SPA(io->buf), ARP_SHA(io->buf));
#if (TCPIP_ARP_DEBUG_FLOW)
            printf("ARP: reply from ");
            print_ip(ARP_SPA(io->buf));
            printf(" is ");
            print_mac(ARP_SHA(io->buf));
            printf("\n\r");
#endif
        }
        break;
    }
    tcpip_release_io(tcpip, io);
}

bool tcpip_arp_resolve(TCPIP* tcpip, const IP* ip, MAC* mac)
{
    if (tcpip_arp_lookup(tcpip, ip, mac))
        return true;
    //request mac
    tcpip_arp_insert(tcpip, ip, mac, TCPIP_ARP_CACHE_INCOMPLETE_TIMEOUT);
    arp_request(tcpip, ip);
    return false;
}