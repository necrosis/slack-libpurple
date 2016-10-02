#ifndef _SLACK_CONNECTION_H
#define _SLACK_CONNECTION_H

/*
#include <glib/gi18n.h>

#include <plugin.h>
#include <prpl.h>
*/
#include "slack_plugin.h"
#include "json.h"
#include <proxy.h>

#define SLACK_MAX_CONNECTIONS 16

typedef void (*SlackProxyCallbackFunc)(SlackAccount *sa, json_value *obj, gpointer user_data);
typedef void (*SlackProxyCallbackErrorFunc)(SlackAccount *sa, const gchar *data, gssize data_len, gpointer user_data);

typedef enum
{
	SLACK_METHOD_GET  = 0x0001,
	SLACK_METHOD_POST = 0x0002,
	SLACK_METHOD_SSL  = 0x0004
} SlackMethod;

typedef struct _SlackConnection {
	SlackAccount *sa;
	SlackMethod method;
	gchar *hostname;
	gchar *url;
	GString *request;
	SlackProxyCallbackFunc callback;
	gpointer user_data;
	char *rx_buf;
	size_t rx_len;
	PurpleProxyConnectData *connect_data;
	PurpleSslConnection *ssl_conn;
	int fd;
	guint input_watcher;
	gboolean connection_keepalive;
	time_t request_time;
	guint retry_count;
	guint timeout_watcher;
	SlackProxyCallbackErrorFunc error_callback;
} SlackConnection;

/*
void
slack_read_channels(SlackAccount* ac);

void
slack_read_channels_purple(SlackAccount* ac);
*/


/*******************************************/
void
slack_connection_close(SlackConnection *slackcon);

void
slack_connection_destroy(SlackConnection *slackcon);


SlackConnection *
get_or_post_request(
	SlackAccount *na,
	SlackMethod method,
	const gchar *host, 
	const gchar *url, 
	const gchar *postdata,
	SlackProxyCallbackFunc callback_func, 
	gpointer user_data,
	gboolean keepalive
);


/*******************************************/

#endif//_SLACK_CONNECTION_H
