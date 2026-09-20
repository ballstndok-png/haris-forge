/* OpenSSL compatibility shim.
 *
 * Some Linux distros (Ubuntu 22.04, RHEL 8) ship OpenSSL 3.0/3.1 which
 * lacks SSL_set_blocking_mode() (added in OpenSSL 3.5) and
 * SSL_set_incoming_stream_policy().
 *
 * This file provides weak fallback definitions so the library links.
 * At runtime, Haris only calls these when HTTP/3 (QUIC) is actually used,
 * which requires a real OpenSSL 3.5+ anyway.
 */

#include <openssl/ssl.h>
#include <openssl/opensslv.h>

#if defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x30500000L

# ifndef SSL_set_blocking_mode
int SSL_set_blocking_mode(SSL *s, int blocking) {
    (void)s; (void)blocking;
    return 0; /* not supported on this OpenSSL */
}
# endif

# ifndef SSL_set_incoming_stream_policy
int SSL_set_incoming_stream_policy(SSL *s, int policy, uint64_t arg) {
    (void)s; (void)policy; (void)arg;
    return 0;
}
# endif

#endif /* OpenSSL < 3.5 */
