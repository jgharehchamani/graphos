#include "AES.hpp"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/rand.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
//#include "sgx_error.h"

void AES::Setup() {
    // Initialise OpenSSL
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
    //    OPENSSL_config(NULL);
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, NULL);
}

void AES::Cleanup() {
    EVP_cleanup();
    ERR_free_strings();
}

static void error(const char *msg) {
    throw msg;
}

int AES::EncryptBytes(bytes<Key> key, bytes<IV> iv, byte_t *plaintext, size_t plen, byte_t *ciphertext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx) {
        error("Failed to create new cipher");
    }

    // Initialise the encryption operation
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key.data(), iv.data()) != 1) {
        error("Failed to initialise encryption");
    }

    // Encrypt
    int len;
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, (int) plen) != 1) {
        error("Failed to complete EncryptUpdate");
    }

    int clen = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        error("Failed to complete EncryptFinal");
    }
    clen += len;

    EVP_CIPHER_CTX_free(ctx);

    return clen;
}

int AES::DecryptBytes(bytes<Key> key, bytes<IV> iv, byte_t *ciphertext, size_t clen, byte_t *plaintext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx) {
        error("Failed to create new cipher");
    }

    // Initialise the decryption operation
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key.data(), iv.data()) != 1) {
        error("Failed to initialise decryption");
    }

    // Dencrypt
    int len;
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, (int) clen) != 1) {
        error("Failed to complete DecryptUpdate");
    }

    int plen = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) {
        error("Failed to complete DecryptFinal");
    }
    plen += len;

    EVP_CIPHER_CTX_free(ctx);

    return plen;
}

block AES::EncryptBlock(bytes<Key> key, bytes<IV> iv, block plaintext, size_t clen_size, size_t plaintext_size) {
    block ciphertext(clen_size);
    EncryptBytes(key, iv, plaintext.data(), plaintext_size, ciphertext.data());
    return ciphertext;
}

block AES::DecryptBlock(bytes<Key> key, bytes<IV> iv, block ciphertext, size_t clen_size) {
    block plaintext(clen_size);
    int plen = DecryptBytes(key, iv, ciphertext.data(), clen_size, plaintext.data());

    // Trim plaintext to actual size
    plaintext.resize(plen);

    return plaintext;
}

block AES::Encrypt(bytes<Key> key, block plaintext, size_t clen_size, size_t plaintext_size) {
    block ciphertext;
    bytes<IV> iv = AES::GenerateIV();

    ciphertext = EncryptBlock(key, iv, plaintext, clen_size, plaintext_size);

    // Put randomised IV at the front of the ciphertext
    ciphertext.insert(ciphertext.end(), iv.begin(), iv.end());
    return ciphertext;
}

block AES::Decrypt(bytes<Key> key, block ciphertext, size_t clen_size) {
    // Extract the IV
    bytes<IV> iv;
    std::copy(ciphertext.end() - IV, ciphertext.end(), iv.begin());

    // Perform the decryption
    block plaintext = DecryptBlock(key, iv, ciphertext, clen_size);

    return plaintext;
}

int AES::GetCiphertextLength(int plen) {
    // Round up to the next 16 bytes (due to padding)
    return (plen / 16 + 1) * 16;
}

bytes<IV> AES::GenerateIV() {
    bytes<IV> iv;

    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        // Bytes generated aren't cryptographically strong
        error("Needs more entropy");
    }

    return iv;
}

int AES::PRF(std::string plaintext, int plaintext_len, unsigned char *ciphertext) {
    EVP_CIPHER_CTX *ctx;

    int len;
    unsigned char key[32];
    unsigned char iv[16];
    memset(key, 0, 32);
    memset(iv, 0, 16);
    //    char *iv = "0123456789012345";
    //    char *key = "01234567890123456789012345678901";


    int ciphertext_len;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new()))
        error("initialize");

    /*
     * Initialise the encryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
        error("init");

    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, (const unsigned char*) plaintext.c_str(), plaintext_len))
        error("update");
    ciphertext_len = len;

    /*
     * Finalise the encryption. Further ciphertext bytes may be written at
     * this stage.
     */
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
        error("final");
    ciphertext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext_len;
}