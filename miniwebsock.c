#include "miniwebsock.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>

const struct frame_header_0
final_frame = { 0x88, 0x80, 0x0 };


gchar* 
encode_by_mask(
		const gchar* input, 
		guint32 mask
)
{
	gchar* dup = g_strdup(input);
	gint64 size = strlen(input);
	gchar* it = dup;
	gchar *mask_holder = (gchar*)&mask;

	guint32 *temp;
	while ((size - 4) >= 0)
	{
		temp = (guint32*)it;
		*temp ^= mask;
		size -= 4;
		it += 4;
	}

	switch (size) 
	{
		case 3: it[2] ^= mask_holder[2];
		case 2: it[1] ^= mask_holder[1];
		case 1: it[0] ^= mask_holder[0];
		case 0:
			break;
	}

	return dup;
}

guint64 
create_frame_from_text(
		const gchar* txt, 
		guchar** out
)
{	
	GRand* rnd = g_rand_new ();

	guint32 mask_key = g_rand_int(rnd);
	guint64 str_size = strlen(txt);
	guchar*	data;
	int header_size;

	if (str_size > 125)
	{
		if (str_size > 0xffff)
		{
			struct frame_header_8 header;
			header.header = 0x81;
			header.mask_len = 0xff;
			header.len = str_size;
			header.mask = mask_key;
			header_size = 14;

			data = g_new0(guchar, header_size + str_size);
			memcpy(data, &header, header_size);
		}
		else
		{
			struct frame_header_2 header;
			header.header = 0x81;
			header.mask_len = 0xfe;
			header.len = (guint16) str_size;
			header.mask = mask_key;
			header_size = 8;

			data = g_new0(guchar, header_size + str_size);
			memcpy(data, &header, header_size);
		}
	} else
	{
		struct frame_header_0 header;
		header.header = 0x81;
		header.mask_len = 0x80 | str_size;
		header.mask = mask_key;
		header_size = 6;

		data = g_new0(guchar, header_size + str_size);
		memcpy(data, &header, header_size);
	}

	gchar *masked_text = encode_by_mask(txt, mask_key);
	memcpy(data+header_size, masked_text, str_size);

	g_free(masked_text);
	g_rand_free(rnd);

	*out = data;
	return header_size + str_size;
}


int 
start_websocket_session(
		int socket_fd, 
		gchar *host, 
		gchar *req
)
{
	int rezult = 0;

	GRand* rnd = g_rand_new ();
	guchar *key = g_new0(guchar, 16);
	for (int i = 0; i < 16; i++)
		key[i] = (guchar)g_rand_int_range(rnd, 0, 255);

	gchar* ekey = g_base64_encode(key, 16);

	GString *request = g_string_new(NULL);
	g_string_append_printf(request, "GET %s HTTP/1.1\r\n", req);
	g_string_append_printf(request, "Host: %s\r\n", host);
	g_string_append_printf(request, "Upgrade: websocket\r\n");
	g_string_append_printf(request, "Connection: Upgrade\r\n");
	g_string_append_printf(request, "Sec-WebSocket-Key: %s\r\n", ekey);
	//g_string_append_printf(request, "Sec-WebSocket-Protocol: chat, superchat\r\n");
	g_string_append_printf(request, "Sec-WebSocket-Version: 13\r\n\r\n");

	
	int n = write(socket_fd, request->str, request->len);

	if (n < 0) 
		return 0;

	char chunk[256];
	GString *fullans = g_string_new(NULL);

	int flags = fcntl(socket_fd, F_GETFL, 0);
	fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
	
	char* rnrn_found = NULL;
	while (!rnrn_found) 
	{
		n = read(socket_fd, chunk, 255);
		if (n > 0)
		{
			chunk[n] = '\0';
			rnrn_found = strstr(chunk, "\r\n\r\n");
			fullans = g_string_append(fullans, chunk);
		}
	}

	gchar** params = g_strsplit(fullans->str, "\r\n", -1);
	gchar** first = g_strsplit(params[0], " ", -1);

	guint code = g_ascii_strtoll(first[1], NULL, 10);

	if (code == 101) 
	{
		// TODO Check Upgrade: websocket
		// TODO Check Connection: Upgrade
		// TODO Check Sec-WebSocket-Accept
		// TODO Check Sec-WebSocket-Extensions
		// TODO Check Sec-WebSocket-Protocol
	
		rezult = 1;
	}

	g_strfreev(first);
	g_strfreev(params);
	g_string_free(fullans, FALSE);
	g_string_free(request, FALSE);
	g_free(ekey);
	g_free(key);
	g_rand_free(rnd);

	return rezult;
}

int 
close_websocket_session(
		int socket_fd
)
{
	struct frame_header_0 fframe = final_frame;
	int n = write(socket_fd, (void*)&final_frame, 6);
	return n == 6;
}



frame_value 
poll_frame(
		int socket_fd
)
{
	frame_value val;
	val.data = NULL;
	val.type = frame_none;
	
	guint8 header[2];
	int n = read(socket_fd, header, 2);

	if (n == 2)
	{
		val.last = (header[0] & 0x80) ? last_frame : not_last_frame;
		int opcode = header[0] & 0x0f;

		switch (opcode)
		{
			case 0x1:
				val.type = frame_text;
				break;
			case 0x2:
				val.type = frame_binary;
				break;
			case 0x8:
				val.type = frame_close;
				break;
			case 0x9:
				val.type = frame_ping;
				break;
			case 0xa:
				val.type = frame_pong;
				break;
			default:
				val.type = frame_none;
				break;

		}

		int masked = header[1] & 0x80;
		guint32 mask = 0;
		guint64 payload_length = header[1] & 0x7f;

		switch (payload_length)
		{
			case 126:
				n = read(socket_fd, &payload_length, 2); // TODO check readed
				break;
			case 127:
				n = read(socket_fd, &payload_length, 8); // TODO check readed
				break;
		}

		if (masked)
		{
			n = read(socket_fd, &mask, 4); // TODO check readed
		}

		guchar* temp = g_new0(guchar, payload_length); // TODO check memory malloced
		n = read(socket_fd, temp, payload_length);

		// TODO check readed
		
		if (masked)
		{
			val.data = encode_by_mask(temp, mask);
			g_free(temp);
		} else
			val.data = temp;
	}
	
	return val;
}



int 
send_text_frame(
		int socket_fd, 
		const char* text
)
{
	guchar* frame = NULL;
	gint64 len = create_frame_from_text(text, &frame);

	gint64 n = write(socket_fd, (void*)frame, len);
	g_free(frame);

	return n == len;
}
