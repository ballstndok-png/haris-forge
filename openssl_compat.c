/* OpenSSL compatibility shim for systems with OpenSSL < 3.5.
 * Provides stub definitions for functions added in OpenSSL 3.5
 * that Haris calls unconditionally. At runtime, QUIC/HTTP3 code
 * would fail on these systems anyway, but at least the base binary
 * links and runs.
 */
#include <openssl/ssl.h>
#include <openssl/opensslv.h>

#if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x30500000L

int SSL_set_blocking_mode(SSL *s, int blocking) {
    (void)s; (void)blocking;
    return 0;
}

#endif
