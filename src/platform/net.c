/* SD2Cloud -- the network: the cable's link and the address from the router, before anything is asked of Google. */
#include "platform.h"

/* ------------------------------------------------------------ network */

/* The drivers and the IP stack start once: starting lwIP a second time hangs the PS2 (that's what happened when the
 * first try failed for lack of a cable and a backup tried again). Every later call only waits for the cable and the
 * router again. A plugged cable has its link in 1-3 s, so 5 s without it = no cable; the router gets its own time.
 *
 * The address: lwIP asks the router for one as soon as it starts, before the cable's link is up, and by itself only
 * asks again after 2, 4, 8... seconds. A request that goes out while the link (or the router's port) isn't ready is
 * lost, and one lost at the wrong moment left the next one past the time SD2Cloud waited: "no address from the
 * router", now and then, with nothing wrong with the router. So the request is made again as soon as the link is
 * up, and again every few seconds until the address comes */
#define NET_DHCP_MS   30000
#define NET_DHCP_AGAIN 3000   /* without an offer from the router for this long, it is asked again */
static int netStarted;
#ifdef DEBUG_BUILD
int debugNoLinkOnce;   /* script "N": the first try finds no cable (PCSX2 can't unplug one) */
int debugNoDhcpOnce;   /* script "n": the first try gets no address from the router */
int debugNoZero;      /* script "u": the network starts without the heap it takes being zeroed (see network_up) */
#endif

static int link_up(void)
{
#ifdef DEBUG_BUILD
    if (debugNoLinkOnce)
        return 0;
#endif
    return NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0) == NETMAN_NETIF_ETH_LINK_STATE_UP;
}

static int dhcp_bound(void)
{
    t_ip_info i;
#ifdef DEBUG_BUILD
    if (debugNoDhcpOnce)
        return 0;
#endif
    if (ps2ip_getconfig("sm0", &i) < 0 || !i.dhcp_enabled)
        return 0;
    return i.dhcp_status == DHCP_STATE_BOUND;
}

static int wait_for(int (*check)(void), int ms)
{
    u64 end = now_ms() + ms;
    while (!check()) {
        if (now_ms() >= end)
            return -1;
        sleep_ms(200);
    }
    return 0;
}

/* asks the router for an address (again: after a cable comes back, lwIP's DHCP may be waiting a long while between
 * its tries, so it's restarted) */
static int start_dhcp(int restart)
{
    t_ip_info i;
    if (ps2ip_getconfig("sm0", &i) < 0)
        return -1;
    if (restart && i.dhcp_enabled) {
        i.dhcp_enabled = 0;
        ps2ip_setconfig(&i);
    }
    i.dhcp_enabled = 1;
    return ps2ip_setconfig(&i) < 0 ? -1 : 0;
}

/* While the network starts, what its libraries take from the heap comes zeroed and with nothing of it waiting in the
 * cache (the Makefile wraps malloc and memalign for this). The SDK's netman takes its two frame tables from the heap
 * and zeroes them through the uncached address only: on a heap that has been used (the icons of a card with many
 * saves, just read), that memory was something else's a moment ago and a line of it is still in the cache, to be
 * written over the zeros later. The driver then sees frames that don't exist: its transmit thread waits for the IOP,
 * the IOP waits for the receive thread, which never runs again; the router's answer never arrives and the IOP stops
 * serving sound and files. Seen on a console (not on PCSX2, which has no cache to disagree with the memory) */
static int zeroNew;
void *__real_malloc(size_t size);
void *__real_memalign(size_t align, size_t size);

static void *zeroed(void *p, size_t size, int whole_lines)
{
    if (zeroNew && p && size) {
        memset(p, 0, size);
        SyncDCache(p, (char *)p + size - 1);
        if (whole_lines)   /* no neighbour shares these lines: they can go from the cache altogether */
            InvalidDCache(p, (char *)p + size - 1);
    }
    return p;
}

void *__wrap_malloc(size_t size) { return zeroed(__real_malloc(size), size, 0); }

void *__wrap_memalign(size_t align, size_t size)
{
    return zeroed(__real_memalign(align, size), size, align >= 64 && align % 64 == 0 && size % 64 == 0);
}

int network_up(void)
{
    struct ip4_addr ip, nm, gw;
    int fresh = !netStarted, i;
    u64 t0 = now_ms();
    if (!netStarted) {
#ifdef DEBUG_BUILD
        debug_log_memory("before the network drivers");
#endif
        zeroNew = 1;   /* see __wrap_malloc */
#ifdef DEBUG_BUILD
        if (debugNoZero) {   /* script "u": as it was before, to see the difference */
            zeroNew = 0;
            log_msg("network: starting WITHOUT zeroing what it takes from the heap");
        }
#endif
        if (init_network_driver(true) != EEIP_INIT_STATUS_OK) {
            zeroNew = 0;
            return T_NET_ERR_DRIVERS;
        }
        ip4_addr_set_zero(&ip);
        ip4_addr_set_zero(&nm);
        ip4_addr_set_zero(&gw);
        ps2ipInit(&ip, &nm, &gw);
        i = start_dhcp(0);
        zeroNew = 0;
        if (i != 0)
            return T_NET_ERR_DRIVERS;
        netStarted = 1;
#ifdef DEBUG_BUILD
        debug_log_memory("after the network drivers");
#endif
    }
    /* (right after a game the link has taken more than 5 s to come up, and a sync at IGR failed for it) */
    if (wait_for(link_up, 12000) != 0) {
        log_msg("network: no link (cable?) after %d ms", (int)(now_ms() - t0));
#ifdef DEBUG_BUILD
        debugNoLinkOnce = 0;
#endif
        return T_NET_ERR_LINK;
    }
    log_msg("network: link up after %d ms", (int)(now_ms() - t0));
    if (!dhcp_bound()) {
        u64 end = now_ms() + NET_DHCP_MS, last = 0;
        int asked = 0, state = -1;
        while (!dhcp_bound() && now_ms() < end) {
            t_ip_info info;
            int s = ps2ip_getconfig("sm0", &info) < 0 ? -1 : (int)info.dhcp_status;
            if (s != state) {   /* (each step of the way, with its time: what tells a slow router from a lost request) */
                log_msg("network: DHCP state %d after %d ms", s, (int)(now_ms() - t0));
                state = s;
            }
            /* asked again only while nothing has come from the router: once it has offered an address, lwIP is
             * busy accepting and checking it, and asking again would throw that away */
            if ((s == DHCP_STATE_SELECTING || s == DHCP_STATE_INIT || s == DHCP_STATE_OFF || s == DHCP_STATE_BACKING_OFF || s < 0) &&
                now_ms() - last >= NET_DHCP_AGAIN) {
                start_dhcp(1);
                last = now_ms();
                asked++;
            }
            sleep_ms(200);
        }
        log_msg("network: the router was asked for an address %d time(s), %d ms after the start", asked, (int)(now_ms() - t0));
        if (!dhcp_bound()) {
            log_msg("network: link up, but no address from the router");
#ifdef DEBUG_BUILD
            debugNoDhcpOnce = 0;
            {
                t_ip_info i;
                memset(&i, 0, sizeof(i));
                ps2ip_getconfig("sm0", &i);
                log_msg("network: DHCP enabled %d, status %d, address %08x", i.dhcp_enabled, i.dhcp_status, (unsigned)i.ipaddr.s_addr);
                debug_ask_iop("after the router didn't answer");
            }
#endif
            return T_NET_ERR_DHCP;
        }
    }
    eeip_get_current_config(&ip, &nm, &gw);
    log_msg("network ok: %d.%d.%d.%d (%d ms%s)", ip4_addr1(&ip), ip4_addr2(&ip), ip4_addr3(&ip), ip4_addr4(&ip), (int)(now_ms() - t0),
            fresh ? "" : ", tried again");
    return 0;
}
