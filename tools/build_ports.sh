#!/bin/sh
# wolfSSL 5.9.4 + curl 8.22.0 (the newest releases, 09/2026) with the same recipe as ps2sdk-ports (build-cmakelibs.sh), but with SP_INT_BITS=4096:
# outside the PC wolfSSL defaults to 3072, and then it rejects the 4096-bit RSA roots (Google's GTS Root R1, ISRG Root
# X1) with error -155. Installs into ports4096/ in the project, without touching the toolchain.
#
# Run it from the project folder, with the ps2dev environment set (PS2DEV, PS2SDK):  sh tools/build_ports.sh
# The sources go to third_party/src/ (cloned if missing; git and network needed only for that).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/third_party/src
P=${PORTS4096:-$ROOT/ports4096}
OPTS="-Wno-dev -DCMAKE_TOOLCHAIN_FILE=$PS2DEV/share/ps2dev.cmake -DCMAKE_INSTALL_PREFIX=$P -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo"

mkdir -p "$SRC"
[ -d "$SRC/wolfssl" ] || git clone --depth 1 --branch v5.9.4-stable https://github.com/wolfSSL/wolfssl.git "$SRC/wolfssl"
[ -d "$SRC/curl" ] || git clone --depth 1 --branch curl-8_22_0 https://github.com/curl/curl.git "$SRC/curl"

rm -rf "$P" "$SRC/wolfssl/build" "$SRC/curl/build"

# The PS2's lwIP has no IPv6, and so no struct sockaddr_in6: wolfSSL's matching of a certificate's IPv6 addresses
# (since 5.9) gets a struct of its own. It never matches anything (XINET_PTON is 0): SD2Cloud connects by name
cat > "$SRC/wolfssl-ps2.h" <<'END'
struct wolfssl_ps2_in6 {
    unsigned short sin6_family, sin6_port;
    unsigned int sin6_flowinfo;
    struct { unsigned char s6_addr[16]; } sin6_addr;
    unsigned int sin6_scope_id;
};
#define WOLFSSL_SOCKADDR_IN6 struct wolfssl_ps2_in6
END

mkdir -p "$SRC/wolfssl/build" && cd "$SRC/wolfssl/build"
CFLAGS="-DWOLFSSL_GETRANDOM -DNO_WRITEV -DXINET_PTON\\(...\\)=0 -DSP_INT_BITS=4096 -DWOLFSSL_NO_ATOMICS -include $SRC/wolfssl-ps2.h" \
  cmake $OPTS "-DCMAKE_PREFIX_PATH=$P;$PS2SDK/ports" -DWOLFSSL_CRYPT_TESTS=OFF -DWOLFSSL_EXAMPLES=OFF \
  -DWOLFSSL_CURL=ON -DWARNING_C_FLAGS=-w .. > ../../wolfssl-cmake.log
make -j8 all install > ../../wolfssl-make.log
# whoever includes the headers (curl and the app) must see the same number size as the library, and its counters
# without atomics: the EE has no instructions for them, and since 5.9 GCC's would call __atomic_* functions the PS2
# has none of (wolfSSL runs on SD2Cloud's main thread only)
printf '\n#undef SP_INT_BITS\n#define SP_INT_BITS 4096\n#ifndef WOLFSSL_NO_ATOMICS\n#define WOLFSSL_NO_ATOMICS\n#endif\n' \
  >> "$P/include/wolfssl/options.h"

mkdir -p "$SRC/curl/build" && cd "$SRC/curl/build"
CFLAGS="-DSIZEOF_LONG=4 -DSIZEOF_LONG_LONG=8 -DNO_WRITEV" \
  cmake $OPTS "-DCMAKE_PREFIX_PATH=$P;$PS2SDK/ports" -DENABLE_THREADED_RESOLVER=OFF -DCURL_USE_OPENSSL=OFF \
  -DCURL_USE_WOLFSSL=ON -DCURL_DISABLE_SOCKETPAIR=ON -DHAVE_BASENAME=NO -DHAVE_ATOMIC=NO -DENABLE_WEBSOCKETS=ON \
  -DENABLE_IPV6=OFF -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF .. > ../../curl-cmake.log
make -j8 all install > ../../curl-make.log

ls -la "$P/lib/libwolfssl.a" "$P/lib/libcurl.a"
grep -n "SP_INT_BITS" "$P/include/wolfssl/options.h"
