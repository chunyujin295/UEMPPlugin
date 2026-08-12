#include "test_common.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include <cstring>

// ---- OpenSSL tests ----
static void test_openssl_version(TestRunner& t) {
    printf("\n[OpenSSL] version...\n");
    long ver = OpenSSL_version_num();
    printf("  OpenSSL version: 0x%lx\n", (unsigned long)ver);
    t.check(ver >= 0x30500000, "OpenSSL >= 3.5.0");    // 3.5.7 → 0x30507000
}

static void test_openssl_sha256(TestRunner& t) {
    printf("\n[OpenSSL] SHA256...\n");
    const char* input = "UEMPPlugin test";
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    t.check(ctx != nullptr, "EVP_MD_CTX_new");

    int ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    t.check(ok == 1, "EVP_DigestInit_ex");

    ok = EVP_DigestUpdate(ctx, input, strlen(input));
    t.check(ok == 1, "EVP_DigestUpdate");

    ok = EVP_DigestFinal_ex(ctx, digest, &digestLen);
    t.check(ok == 1, "EVP_DigestFinal_ex");
    t.check(digestLen == 32, "SHA256 digest is 32 bytes");

    // Verify determinism: same input → same hash
    unsigned char digest2[EVP_MAX_MD_SIZE];
    unsigned int digestLen2 = 0;
    EVP_MD_CTX* ctx2 = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx2, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx2, input, strlen(input));
    EVP_DigestFinal_ex(ctx2, digest2, &digestLen2);
    t.check(memcmp(digest, digest2, 32) == 0, "SHA256 is deterministic");

    EVP_MD_CTX_free(ctx);
    EVP_MD_CTX_free(ctx2);
}

static void test_openssl_random(TestRunner& t) {
    printf("\n[OpenSSL] random bytes...\n");
    unsigned char buf1[32];
    unsigned char buf2[32];

    int rc = RAND_bytes(buf1, sizeof(buf1));
    t.check(rc == 1, "RAND_bytes first call");

    rc = RAND_bytes(buf2, sizeof(buf2));
    t.check(rc == 1, "RAND_bytes second call");

    t.check(memcmp(buf1, buf2, 32) != 0, "two random buffers differ");
}

int main() {
    printf("=== OpenSSL Tests ===\n");
    TestRunner t;
    test_openssl_version(t);
    test_openssl_sha256(t);
    test_openssl_random(t);
    return t.finish();
}
