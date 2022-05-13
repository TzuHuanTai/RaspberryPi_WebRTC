#include <openssl/ssl.h>

// g++ test_boringssl.cpp -o test_boringssl -I/usr/local/include -L/usr/local -lssl -lcrypto -pthread
// g++ test_boringssl.cpp -o test_boringssl -lssl -lcrypto -pthread
// g++ test_boringssl.cpp -o test_boringssl -pthread /usr/local/lib/libssl.a /usr/local/lib/libcrypto.a

int main()
{
  ::SSL_COMP_free_compression_methods();
}