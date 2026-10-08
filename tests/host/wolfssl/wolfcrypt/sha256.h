/* wolfSSL's SHA-256, as SD2Cloud calls it, for the tests: done by tests/host/sha256.c, so that they need no wolfSSL */
#ifndef HOST_SHA256_H
#define HOST_SHA256_H

#define WC_SHA256_DIGEST_SIZE 32

typedef struct {
    unsigned int h[8];
    unsigned long long bytes;
    unsigned char block[64];
    unsigned int used;
} wc_Sha256;

int wc_InitSha256(wc_Sha256 *s);
int wc_Sha256Update(wc_Sha256 *s, const unsigned char *d, unsigned int n);
int wc_Sha256Final(wc_Sha256 *s, unsigned char *out);
void wc_Sha256Free(wc_Sha256 *s);

#endif
