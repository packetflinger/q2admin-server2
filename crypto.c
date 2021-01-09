#include "server.h"

/**
 * We (server) will only have access to the client's public key (assuming the server
 * operator provides it via the web interface.
 *
 * We (server) will also only en/decypher using our own private key. The client should
 * already have our public key from when they sent us their public key.
 *
 * In order for us (server) to trust a q2 server (client):
 * 1. Server sends random data to client (saving it for later comparison)
 * 2. Client encrypts using their private key and sends back to server
 * 3. Server decrypts what it received using client's public key.
 * 4. If decrypted data matches original data, client is authenticated and trusted.
 *
 * In order for client to trust server:
 * 1. Client generates random data and sends (as part of HELLO message)
 * 2. Server encrypts using it's private key and sends it back
 * 3. Client decrypts using server's public key
 * 4. If decrypted data matches original data, server is trusted.
 *
 * Q. Why do we care about trust?
 * A. Clients do what server tells them to (kick/ban clients, stuff connect commands, etc),
 *    the server must be trusted to unconditionally do what it says. Likewise, forged input
 *    from a malicious user pretending to be valid server could lead to denial of service or
 *    general unpleasantness.
 */


/**
 * Encrypt data using client's public key. Only their private key
 * will be able to decrypt it.
 */
void Client_PublicKey_Encypher(q2_server_t *q2, byte *to, byte *from, int *len)
{
	RSA *publickey;
	FILE *fp;
	char keypath[20];
	int bytes;

	sprintf(keypath, "keys/%d.pub", q2->id);
	fp = fopen(keypath, "rb");

	publickey = PEM_read_RSAPublicKey(fp, NULL, NULL, NULL);
	bytes = RSA_public_encrypt(CHALLENGE_LEN, from, to, publickey, RSA_PKCS1_PADDING);

	*len = bytes;

	RSA_free(publickey);
	fclose(fp);
}

/**
 * Decrypt data using a client's public key. This will only work if the
 * data was encrypted using the corresponding private key
 */
size_t Client_Challenge_Decrypt(q2_server_t *q2, byte *to, byte *from)
{
	size_t cypherlen;
	cypherlen = RSA_public_decrypt(RSA_size(q2->publickey), from, to, q2->publickey, RSA_PKCS1_PADDING);
	return cypherlen;
}

/**
 * Encrypt data using the server's private key
 */
uint32_t Server_PrivateKey_Encypher(byte *to, byte *from)
{
	RSA *privatekey;
	FILE *fp;
	size_t cypherlen;

	fp = fopen("server-private.key", "rb");

	privatekey = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
	cypherlen = RSA_private_encrypt(CHALLENGE_LEN, from, to, privatekey, RSA_PKCS1_PADDING);

	RSA_free(privatekey);
	fclose(fp);

	return cypherlen;
}

/**
 * Decrypt data using server's private key
 */
uint32_t Server_PrivateKey_Decypher(byte *to, byte *from)
{
	RSA *privatekey;
	FILE *fp;
	size_t cypherlen;

	fp = fopen("server-private.key", "rb");

	privatekey = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
	cypherlen = RSA_private_decrypt(CHALLENGE_LEN, from, to, privatekey, RSA_PKCS1_PADDING);

	RSA_free(privatekey);
	fclose(fp);

	return cypherlen;
}

/**
 * Encrypt the client-provided challenge with our private key to authenticate us
 */
size_t Sign_Client_Challenge(byte *to, byte *from)
{
	RSA *privatekey;
	FILE *fp;
	size_t cypherlen;

	fp = fopen(config.private_key, "rb");

	if (!fp) {
	    printf("* unable to open private key: %s *\n", config.private_key);
	    return 0;
	}

	privatekey = RSA_new();
	privatekey = PEM_read_RSAPrivateKey(fp, &privatekey, NULL, NULL);

	if (!privatekey) {
	    printf("* error loading private key *\n");
	    return 0;
	}

	cypherlen = RSA_private_encrypt(CHALLENGE_LEN, from, to, privatekey, RSA_PKCS1_PADDING);

	RSA_free(privatekey);
	fclose(fp);

	return cypherlen;
}

void Verify_Client_Challenge() {
}

/**
 * This is the symmetric AES key for encrypting messages between client/server.
 * This is only generated if the client indicates it wants encryption.
 *
 * Both the key and the IV are encrypted together since the client will need both. This
 * saves on cpu time for decrypting. The RSA key size is 2048 bits, so two 128 bit keys
 * will fit in the same amount of data sent to the client.
 */
size_t Encrypt_AESKey(RSA *publickey, byte *key, byte *iv, byte *cipher)
{
	size_t cipherlen;

	// concat the key and iv into one contiguous block of memory
	byte key_plus_iv[AESKEY_LEN + AESBLOCK_LEN];
	memcpy(key_plus_iv, key, AESKEY_LEN);
	memcpy(key_plus_iv + AESKEY_LEN, iv, AESBLOCK_LEN);

	//hexDump("Key", key, AESKEY_LEN);
	//hexDump("IV", iv, AESBLOCK_LEN);

	cipherlen = RSA_public_encrypt(AESKEY_LEN + AESBLOCK_LEN, key_plus_iv, cipher, publickey, RSA_PKCS1_PADDING);

	return cipherlen;
}

/**
 * Helper for printing out binary keys and ciphertext as hex
 */
void hexDump (char *desc, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

/**
 *
 */
size_t SymmetricEncrypt(q2_server_t *q2, byte *dest, byte *src, size_t src_len)
{
    connection_t *c = &q2->connection;
    int dest_len = 0;
    int written = 0;

    if (!(c || c->e_ctx)) {
        return 0;
    }

    EVP_EncryptInit_ex(
            q2->connection.e_ctx,
            EVP_aes_128_cbc(),
            NULL,
            q2->connection.aeskey,
            q2->connection.iv
    );

    EVP_EncryptUpdate(c->e_ctx, dest + dest_len, &dest_len, src, src_len);
    written += dest_len;

    EVP_EncryptFinal_ex(c->e_ctx, dest + dest_len, &dest_len);
    written += dest_len;

    return written;
}

/**
 *
 */
size_t SymmetricDecrypt(q2_server_t *q2, byte *dest, byte *src, size_t src_len)
{
    connection_t *c = &q2->connection;
    int dest_len = 0;
    int written = 0;

    if (!(c || c->d_ctx)) {
        return 0;
    }

    EVP_DecryptInit_ex(
            q2->connection.d_ctx,
            EVP_aes_128_cbc(),
            NULL,
            q2->connection.aeskey,
            q2->connection.iv
    );

    EVP_DecryptUpdate(c->d_ctx, dest + dest_len, &dest_len, src, src_len);
    written += dest_len;

    EVP_DecryptFinal_ex(c->d_ctx, dest + dest_len, &dest_len);
    written += dest_len;

    return written;
}

