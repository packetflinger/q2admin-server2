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
 * Encrypt the client-provided challenge with our private key to authenticate us.
 *
 * length of *to should be the same as the key length or RSA_size() (2048 for us)
 */
size_t Sign_Client_Challenge(byte *to, byte *from)
{
	RSA *privatekey;
	FILE *fp;
	size_t cipherlen;

	fp = fopen(config.private_key, "rb");

	if (!fp) {
	    printf("[error] unable to open private key: %s *\n", config.private_key);
	    return 0;
	}

	privatekey = RSA_new();
	privatekey = PEM_read_RSAPrivateKey(fp, &privatekey, NULL, NULL);

	if (!privatekey) {
	    printf("[error] problems loading private key *\n");
	    return 0;
	}

	cipherlen = RSA_private_encrypt(CHALLENGE_LEN, from, to, privatekey, RSA_PKCS1_PADDING);

	RSA_free(privatekey);
	fclose(fp);

	return cipherlen;
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

    EVP_DecryptInit_ex (
            c->d_ctx,
            EVP_aes_128_cbc(),
            NULL,
            c->aeskey,
            c->iv
    );

    EVP_DecryptUpdate(c->d_ctx, dest + dest_len, &dest_len, src, src_len);
    written += dest_len;

    EVP_DecryptFinal_ex(c->d_ctx, dest + dest_len, &dest_len);
    written += dest_len;

    return written;
}

