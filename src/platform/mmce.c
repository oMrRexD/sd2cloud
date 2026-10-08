/* SD2Cloud -- the MMCE device the microSD is in (an sd2psx, a MemCard PRO2): which one it is, the card it is using,
 * and telling it to take another. */
#include "platform.h"

/* ------------------------------------------------------------ sd2psx (MMCE commands, mmceman's devctl) */

#define MMCE_CMD_PING        0x1
#define MMCE_CMD_GET_CARD    0x3
#define MMCE_CMD_SET_CARD    0x4
#define MMCE_CMD_GET_CHANNEL 0x5
#define MMCE_CMD_SET_CHANNEL 0x6
#define MMCE_CMD_SET_GAMEID  0x8

/* -2 = not on an MMCE device */
static int mmce_devctl(int cmd, void *arg, unsigned int len)
{
    char dev[8];
    if (strncmp(sdRoot, "mmce", 4) != 0)
        return -2;
    snprintf(dev, sizeof(dev), "%.6s", sdRoot);   /* "mmce0:" */
    return fileXioDevctl(dev, cmd, arg, len, NULL, 0);
}

int mmce_ping(void) { return mmce_devctl(MMCE_CMD_PING, NULL, 0); }

/* the same, without a word in the log: it is asked over and over while the screens wait */
int mmce_card_now(int *channel)
{
    int card, ch;
    if (strncmp(sdRoot, "mmce", 4) != 0)
        return -2;
    card = mmce_devctl(MMCE_CMD_GET_CARD, NULL, 0);
    ch = mmce_devctl(MMCE_CMD_GET_CHANNEL, NULL, 0);
    if (card < 0 || ch < 0)
        return -1;
    *channel = ch;
    return card;
}

int mmce_active_card(int *channel)
{
    int ch = -1, card = mmce_card_now(&ch);
    if (card != -2)
        log_msg("sd2psx: card %d, channel %d", card, ch);
    if (card >= 0)
        *channel = ch;
    return card;
}

/* mmceman takes the kind of card (1 = the BootCard), how to pick it (0 = by number) and the number in one word */
int mmce_set_card(int boot, int number)
{
    u32 arg = ((u32)(boot ? 1 : 0) << 24) | (number & 0xFFFF);
    int r = mmce_devctl(MMCE_CMD_SET_CARD, &arg, sizeof(arg));
    log_msg("sd2psx: asked for %s %d (%d)", boot ? "the BootCard" : "card", number, r);
    return r < 0 ? -1 : 0;
}

int mmce_set_channel(int channel)
{
    u32 arg = channel & 0xFFFF;
    int r = mmce_devctl(MMCE_CMD_SET_CHANNEL, &arg, sizeof(arg));
    log_msg("sd2psx: asked for channel %d (%d)", channel, r);
    return r < 0 ? -1 : 0;
}

int mmce_set_gameid(const char *id)
{
    char t[64];
    int r;
    snprintf(t, sizeof(t), "%s", id);
    r = mmce_devctl(MMCE_CMD_SET_GAMEID, t, strlen(t) + 1);
    log_msg("sd2psx: asked for the card of %s (%d)", t, r);
    return r < 0 ? -1 : 0;
}

/* The device. The sd2psx's firmware also runs on the PSxMemCard and on the PicoMemcards; 8BitMods' MemCard PRO2 has
 * its own, which keeps the PS2 cards in /PS2/<folder>/<folder>-<channel>.mc2 (raw, as a .mcd is), the ones that aren't
 * a game's in MemoryCard1, MemoryCard2... (its wiki, and what its users' tools go by). SD2Cloud was not tried on one:
 * what its card numbers mean isn't taken for granted, so the card in use is told by the root folder the PS2 sees in the
 * slot; telling it to take another card is offered as a preview, checked the same way; and it is never moved off a
 * card for that card to be changed */
static const device_t devSd2psx = {"sd2psx", "sd2psx", "MemoryCards/PS2", ".mcd", "Card", 1};
static const device_t devMcp2 = {"MemCard PRO2", "PRO2", "PS2", ".mc2", "MemoryCard", 0};
const device_t *dev = &devSd2psx;
int cardTold = 1;
int devicePings;

/* which one answers in the microSD's slot: the ping gives the protocol's version, the product (1 = SD2PSX, 2 = MemCard
 * PRO2, 3 and 4 = the PicoMemcards) and its revision. One that doesn't answer is a MemCard PRO2 when its firmware's
 * file is on the microSD. And whatever it answers, the microSD itself tells: one with no MemoryCards/PS2 and with the
 * MemCard PRO2's own /PS2 is a MemCard PRO2's (a real one, 10/2026, was taken for an sd2psx and its cards not found) */
void find_device(void)
{
    char c[64];
    int r = mmce_devctl(MMCE_CMD_PING, NULL, 0), product = r >= 0 ? (r >> 8) & 0xFF : 0;
    dev = &devSd2psx;
#ifdef DEBUG_BUILD
    {   /* PCSX2 has no device: product.txt in the data folder says which one to be */
        buffer_t b = {0};
        snprintf(c, sizeof(c), "%sproduct.txt", dataDir);
        if (file_read(c, &b) == 0 && b.len)
            product = atoi((char *)b.data);
        buf_free(&b);
    }
#endif
    snprintf(c, sizeof(c), "%smcp2.bin", sdRoot);
    if (product == 2 || (r < 0 && r != -2 && file_exists(c)) || (!has_dir(sdRoot, "MemoryCards/PS2") && has_dir(sdRoot, "PS2")))
        dev = &devMcp2;
    cardTold = dev->sd2psx;
    devicePings = r >= 0;
#ifdef DEBUG_BUILD
    snprintf(c, sizeof(c), "%sby-slot.txt", dataDir);   /* to try on an sd2psx what a device that isn't one gets */
    if (file_exists(c))
        cardTold = 0;
#endif
    log_msg("device: %s (ping %d: protocol %d, product %d, revision %d)", dev->name, r, r >= 0 ? (r >> 16) & 0xFF : 0, product,
            r >= 0 ? r & 0xFF : 0);
}

void system_reload(void)
{
    find_device();
    config_read();
}
