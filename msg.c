#include "server.h"


char buffer[0xffff];

void MSG_ReadData(msg_buffer_t *msg, void *out, size_t len)
{
	memcpy(out, &(msg->data[msg->index]), len);
	msg->index += len;
}

// unsigned
uint8_t MSG_ReadByte(msg_buffer_t *msg)
{
	unsigned char b = msg->data[msg->index];
	msg->index++;
	return b & 0xff;
}

// signed
int8_t MSG_ReadChar(msg_buffer_t *msg)
{
    signed char c = msg->data[msg->index];
    msg->index++;
    return c;
}

uint16_t MSG_ReadShort(msg_buffer_t *msg)
{
	return 	(msg->data[msg->index++] +
			(msg->data[msg->index++] << 8)) & 0xffff;
}

int16_t MSG_ReadWord(msg_buffer_t *msg)
{
	return 	(msg->data[msg->index++] +
			(msg->data[msg->index++] << 8));
}

int32_t MSG_ReadLong(msg_buffer_t *msg)
{
	return 	msg->data[msg->index++] +
			(msg->data[msg->index++] << 8) +
			(msg->data[msg->index++] << 16) +
			(msg->data[msg->index++] << 24);
}

char *MSG_ReadString(msg_buffer_t *msg)
{
	static char str[MAX_STRING_CHARS];
	static char character;
	size_t i, len = 0;

	do {
		len++;
	} while (msg->data[(msg->index + len)] != 0);

	memset(&str, 0, MAX_STRING_CHARS);

	for (i=0; i<=len; i++) {
		character = MSG_ReadByte(msg) & 0x7f;
		strcat(str,  &character);
	}

	return str;
}


void MSG_WriteByte(byte b, msg_buffer_t *buf)
{
	buf->data[buf->index] = b;
	buf->index++;
	buf->length++;
}

void MSG_WriteShort(uint16_t s, msg_buffer_t *buf)
{
	buf->data[buf->index++] = s & 0xff;
	buf->data[buf->index++] = s >> 8;
	buf->length += 2;
}

void MSG_WriteLong(uint32_t l, msg_buffer_t *buf)
{
	buf->data[buf->index++] = l & 0xff;
	buf->data[buf->index++] = (l >> 8) & 0xff;
	buf->data[buf->index++] = (l >> 16) & 0xff;
	buf->data[buf->index++] = l >> 24;
	buf->length += 4;
}

void MSG_WriteString(const char *str, msg_buffer_t *buf)
{
	size_t len;

	if (!str) {
		MSG_WriteByte(0, buf);
		return;
	}

	len = strlen(str);

	MSG_WriteData(str, len + 1, buf);
}

void MSG_WriteData(const void *data, size_t length, msg_buffer_t *buf)
{
	uint32_t i;
	for (i=0; i<length; i++) {
		MSG_WriteByte(((byte *) data)[i], buf);
	}
}



