#include <stdio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#define BITLEN  2048

int main(int argc, char **argv)
{
    FILE    *fp;
    RSA     *rsa;
    BIGNUM  *e;
    time_t  now;
    char    filename[64];

    time(&now);

    rsa = RSA_new();
    e = BN_new();

    BN_set_word(e, RSA_F4);
    RSA_generate_key_ex(rsa, BITLEN, e, NULL);

    snprintf(filename, sizeof(filename), "public-%d.pem", now);
    fp = fopen(filename, "wb");
    if (!fp) {
        printf("Unable to write key file\n");
        return 0;
    }
    PEM_write_RSAPublicKey(fp, rsa);
    fclose(fp);

    snprintf(filename, sizeof(filename), "private-%d.pem", now);
    fp = fopen(filename, "wb");
    if (!fp) {
        printf("Unable to write key file\n");
        return 0;
    }
    PEM_write_RSAPrivateKey(fp, rsa, NULL, NULL, 0, NULL, NULL);
    fclose(fp);

    BN_free(e);
    RSA_free(rsa);

    return 0;
}
