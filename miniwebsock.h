#include <glib/gi18n.h>

#ifndef _MINIWEBSOCK_H_
#define _MINIWEBSOCK_H_

typedef enum 
{
	frame_none,
	frame_text,
	frame_binary,
	frame_close,
	frame_ping,
	frame_pong
} frame_type;

typedef enum
{
	last_frame,
	not_last_frame
} frame_in_queue;

typedef struct __attribute__((__packed__)) _frame_value_
{
	frame_type 	type;
	frame_in_queue	last;
	guchar*		data;
} frame_value;

struct __attribute__((__packed__)) frame_header_0
{
	guint8	header;
	guint8	mask_len;
	guint32	mask;
};

struct __attribute__((__packed__)) frame_header_2
{
	guint8	header;
	guint8	mask_len;
	guint16	len;
	guint32	mask;
};

struct __attribute__((__packed__)) frame_header_8
{
	guint8	header;
	guint8	mask_len;
	guint64	len;
	guint32	mask;
};


extern const struct frame_header_0 final_frame;


/*
 * functions
 */


int 
start_websocket_session(
		int socket_fd, 
		gchar *host, 
		gchar *req
);

int 
close_websocket_session(
		int socket_fd
);

frame_value 
poll_frame(
		int socket_fd
);

int 
send_text_frame(
		int socket_fd, 
		const char* text
);

#endif
