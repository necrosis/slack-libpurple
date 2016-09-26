#include "slack_connection.h"
#include "slack_common.h"
#include "slack_chat.h"
#include <curl/curl.h>
#include <zlib.h>
#include <stdlib.h>
#include <errno.h>
#include <debug.h>
#include <blist.h>
#include <status.h>
#include <version.h>
#include <dnsquery.h>
#include <netinet/in.h>


#if !PURPLE_VERSION_CHECK(3, 0, 0)
	#define purple_connection_error purple_connection_error_reason
#endif

#if !GLIB_CHECK_VERSION (2, 22, 0)
#define g_hostname_is_ip_address(hostname) (g_ascii_isdigit(hostname[0]) && g_strstr_len(hostname, 4, "."))
#endif


static size_t 
writefunc(void *ptr, size_t size, size_t nmemb, string *s)
{
	size_t new_len = s->len + size*nmemb;
	s->ptr = realloc(s->ptr, new_len+1);

	if (s->ptr == NULL)
	{
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}

	memcpy(s->ptr+s->len, ptr, size*nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size*nmemb;
}

static void 
slack_process_channels_json(SlackAccount *sa, json_value *jobj)
{
	// iterate through channels list
	json_value *channels = json_get_value(jobj, "channels");
	int length = channels->u.array.length;
	purple_debug_info(PROTOCOL_CODE, "Buddys %d\n", length);
	PurpleGroup *group = NULL;
	
	for (int i = 0; i < length; i++)
	{
		json_value *channel = channels->u.array.values[i];
		json_value *id = json_get_value(channel, "id");
		json_value *name = json_get_value(channel, "name");

		SlackChannel *ch = g_new0(SlackChannel, 1);
		ch->sa = sa;
		ch->id = g_strdup(id->u.str.ptr);
		ch->name = g_strdup(name->u.str.ptr);
		ch->purple_id = g_str_hash(id->u.str.ptr);


		if (!purple_find_buddy(sa->account, ch->id))
		{
			if (!group)
			{
				group = purple_find_group("Slack");
				if (!group)
				{
					purple_debug_info(PROTOCOL_CODE, "Create group\n");
					group = purple_group_new("Slack");
					purple_blist_add_group(group, NULL);
				}
				purple_debug_info(PROTOCOL_CODE, "Usr add\n");
				purple_blist_add_buddy(purple_buddy_new(sa->account, ch->id, NULL), NULL, group, NULL);
			}
		}

		const char *status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);

		purple_serv_got_private_alias(sa->pc, id->u.str.ptr, name->u.str.ptr);
		purple_prpl_got_user_status(sa->account, id, status_id, "message", NULL, NULL); // check status

		//TODO load history
		//TODO start message poll

		sa->channels = g_list_append(sa->channels, ch);
	}
}

void
slack_read_channels(SlackAccount* ac)
{
	purple_debug_info(PROTOCOL_CODE, "Channels data update\n");
	
	CURLcode res;
	CURL *curl = curl_easy_init();

	if (curl) 
	{
		string s = NULL_STRING;
		create_string(&s, 0);

		curl_easy_setopt(curl, CURLOPT_URL, "https://slack.com/api/channels.list?token=xoxp-37479727381-37710578004-73740828260-ff3474037e");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

#ifdef SKIP_PEER_VERIFICATION
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

#ifdef SKIP_HOSTNAME_VERIFICATION
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
		res = curl_easy_perform(curl);

		if(res != CURLE_OK)
			purple_debug_error(PROTOCOL_CODE, "Error on connection\n");
		else
		{	// parse json
			purple_debug_info(PROTOCOL_CODE, "Channels data accured\n");
			json_value* json = json_parse(s.ptr, s.len);

			if (json != NULL)
			{
				json_value *ok = json_get_value(json, "ok");
				if (ok->u.boolean)
				{
					slack_process_channels_json(ac, json);
				}
				else
					purple_debug_info(PROTOCOL_CODE, "No data available\n");

				json_value_free(json);
			}
			else
				purple_debug_error(PROTOCOL_CODE, "Error on reading json\n");
		}
	}

	curl_easy_cleanup(curl);
}


void
slack_read_channels_purple(SlackAccount* ac)
{
	UNUSED(ac);
	purple_debug_info(PROTOCOL_CODE, "Channels data update\n");
}

/**********************************************************/
static void slack_attempt_connection(SlackConnection *);
static void slack_next_connection(SlackAccount *sa);


static gchar 
*slack_gunzip(
	const guchar *gzip_data, 
	gssize *len_ptr
)
{
	gsize gzip_data_len = *len_ptr;
	z_stream zstr;
	int gzip_err = 0;
	gchar *data_buffer;
	gulong gzip_len = G_MAXUINT16;
	GString *output_string = NULL;

	data_buffer = g_new0(gchar, gzip_len);

	zstr.next_in = NULL;
	zstr.avail_in = 0;
	zstr.zalloc = Z_NULL;
	zstr.zfree = Z_NULL;
	zstr.opaque = 0;
	gzip_err = inflateInit2(&zstr, MAX_WBITS+32);
	if (gzip_err != Z_OK)
	{
		g_free(data_buffer);
		purple_debug_error("steam", "no built-in gzip support in zlib\n");
		return NULL;
	}
	
	zstr.next_in = (Bytef *)gzip_data;
	zstr.avail_in = gzip_data_len;
	
	zstr.next_out = (Bytef *)data_buffer;
	zstr.avail_out = gzip_len;
	
	gzip_err = inflate(&zstr, Z_SYNC_FLUSH);

	if (gzip_err == Z_DATA_ERROR)
	{
		inflateEnd(&zstr);
		inflateInit2(&zstr, -MAX_WBITS);
		if (gzip_err != Z_OK)
		{
			g_free(data_buffer);
			purple_debug_error("steam", "Cannot decode gzip header\n");
			return NULL;
		}
		zstr.next_in = (Bytef *)gzip_data;
		zstr.avail_in = gzip_data_len;
		zstr.next_out = (Bytef *)data_buffer;
		zstr.avail_out = gzip_len;
		gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
	}
	output_string = g_string_new("");
	while (gzip_err == Z_OK)
	{
		//append data to buffer
		output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
		//reset buffer pointer
		zstr.next_out = (Bytef *)data_buffer;
		zstr.avail_out = gzip_len;
		gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
	}
	if (gzip_err == Z_STREAM_END)
	{
		output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
	} else {
		purple_debug_error("steam", "gzip inflate error\n");
	}
	inflateEnd(&zstr);

	g_free(data_buffer);	

	if (len_ptr)
		*len_ptr = output_string->len;

	return g_string_free(output_string, FALSE);
}

void
slack_connection_close(SlackConnection *slackcon)
{
	slackcon->sa->conns = g_slist_remove(slackcon->sa->conns, slackcon);
	
	if (slackcon->connect_data != NULL) {
		purple_proxy_connect_cancel(slackcon->connect_data);
		slackcon->connect_data = NULL;
	}

	if (slackcon->ssl_conn != NULL) {
		purple_ssl_close(slackcon->ssl_conn);
		slackcon->ssl_conn = NULL;
	}

	if (slackcon->fd >= 0) {
		close(slackcon->fd);
		slackcon->fd = -1;
	}

	if (slackcon->input_watcher > 0) {
		purple_input_remove(slackcon->input_watcher);
		slackcon->input_watcher = 0;
	}
	
	purple_timeout_remove(slackcon->timeout_watcher);
	
	g_free(slackcon->rx_buf);
	slackcon->rx_buf = NULL;
	slackcon->rx_len = 0;
}

void 
slack_connection_destroy(SlackConnection *slackcon)
{
	slack_connection_close(slackcon);
	
	if (slackcon->request != NULL)
		g_string_free(slackcon->request, TRUE);
	
	g_free(slackcon->url);
	g_free(slackcon->hostname);
	g_free(slackcon);
}

static void 
slack_update_cookies(SlackAccount *sa, const gchar *headers)
{
	const gchar *cookie_start;
	const gchar *cookie_end;
	gchar *cookie_name;
	gchar *cookie_value;
	int header_len;

	g_return_if_fail(headers != NULL);

	header_len = strlen(headers);

	/* look for the next "Set-Cookie: " */
	/* grab the data up until ';' */
	cookie_start = headers;
	while ((cookie_start = strstr(cookie_start, "\r\nSet-Cookie: ")) &&
			(cookie_start - headers) < header_len)
	{
		cookie_start += 14;
		cookie_end = strchr(cookie_start, '=');
		cookie_name = g_strndup(cookie_start, cookie_end-cookie_start);
		cookie_start = cookie_end + 1;
		cookie_end = strchr(cookie_start, ';');
		cookie_value= g_strndup(cookie_start, cookie_end-cookie_start);
		cookie_start = cookie_end;

		g_hash_table_replace(sa->cookie_table, cookie_name,
				cookie_value);
	}
}


static void 
slack_connection_process_data(SlackConnection *slackcon)
{
	gssize len;
	gchar *tmp;

	len = slackcon->rx_len;
	tmp = g_strstr_len(slackcon->rx_buf, len, "\r\n\r\n");
	if (tmp == NULL) {
		/* This is a corner case that occurs when the connection is
		 * prematurely closed either on the client or the server.
		 * This can either be no data at all or a partial set of
		 * headers.  We pass along the data to be good, but don't
		 * do any fancy massaging.  In all likelihood the result will
		 * be tossed by the connection callback func anyways
		 */
		tmp = g_strndup(slackcon->rx_buf, len);
	} else {
		tmp += 4;
		len -= g_strstr_len(slackcon->rx_buf, len, "\r\n\r\n") -
				slackcon->rx_buf + 4;
		tmp = g_memdup(tmp, len + 1);
		tmp[len] = '\0';
		slackcon->rx_buf[slackcon->rx_len - len] = '\0';
		slack_update_cookies(slackcon->sa, slackcon->rx_buf);

		if (strstr(slackcon->rx_buf, "Content-Encoding: gzip"))
		{
			/* we've received compressed gzip data, decompress */
			gchar *gunzipped;
			gunzipped = slack_gunzip((const guchar *)tmp, &len);
			g_free(tmp);
			tmp = gunzipped;
		}
	}

	g_free(slackcon->rx_buf);
	slackcon->rx_buf = NULL;

	if (slackcon->callback != NULL) {
		if (!len)
		{
			purple_debug_error(PROTOCOL_CODE, "No data in response\n");
		} else {
			json_value* json = json_parse(tmp, len);
			if (json == NULL)
			{
				if (slackcon->error_callback != NULL) {
					slackcon->error_callback(slackcon->sa, tmp, len, slackcon->user_data);
				} else {
					purple_debug_error(PROTOCOL_CODE, "Error parsing response: %s\n", tmp);
				}

				json_value_free(json);
			} else {
				purple_debug_info(PROTOCOL_CODE, "executing callback for %s\n", slackcon->url);

				slackcon->callback(slackcon->sa, json, slackcon->user_data);
			}
		}
	}

	g_free(tmp);
}

static void 
slack_fatal_connection_cb(SlackConnection *slackcon)
{
	PurpleConnection *pc = slackcon->sa->pc;

	purple_debug_error(PROTOCOL_CODE, "fatal connection error\n");

	slack_connection_destroy(slackcon);

	/* We died.  Do not pass Go.  Do not collect $200 */
	/* In all seriousness, don't attempt to call the normal callback here.
	 * That may lead to the wrong error message being displayed */
	purple_connection_error(pc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Server closed the connection."));

}

static void 
slack_post_or_get_readdata_cb(
	gpointer data, 
	gint source,
	PurpleInputCondition cond
)
{
	SlackConnection *slackcon;
	SlackAccount *sa;
	gchar buf[4096];
	gssize len;

	slackcon = data;
	sa = slackcon->sa;

	if (slackcon->method & SLACK_METHOD_SSL) {
		len = purple_ssl_read(slackcon->ssl_conn, buf, sizeof(buf) - 1);
	} else {
		len = recv(slackcon->fd, buf, sizeof(buf) - 1, 0);
	}

	if (len < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			/* Try again later */
			return;
		}

		if (slackcon->method & SLACK_METHOD_SSL && slackcon->rx_len > 0) {
			/*
			 * This is a slightly hacky workaround for a bug in either
			 * GNU TLS or in the SSL implementation on steam's web
			 * servers.  The sequence of events is:
			 * 1. We attempt to read the first time and successfully read
			 *    the server's response.
			 * 2. We attempt to read a second time and libpurple's call
			 *    to gnutls_record_recv() returns the error
			 *    GNUTLS_E_UNEXPECTED_PACKET_LENGTH, or
			 *    "A TLS packet with unexpected length was received."
			 *
			 * Normally the server would have closed the connection
			 * cleanly and this second read() request would have returned
			 * 0.  Or maybe it's normal for SSL connections to be severed
			 * in this manner?  In any case, this differs from the behavior
			 * of the standard recv() system call.
			 */
			purple_debug_warning(PROTOCOL_CODE,
				"ssl error, but data received.  attempting to continue\n");
		} else {
			/* Try resend the request */
			slackcon->retry_count++;
			if (slackcon->retry_count < 3) {
				slack_connection_close(slackcon);
				slackcon->request_time = time(NULL);
				
				g_queue_push_head(sa->waiting_conns, slackcon);
				slack_next_connection(sa);
			} else {
				slack_fatal_connection_cb(slackcon);
			}
			return;
		}
	}

	if (len > 0)
	{
		buf[len] = '\0';

		slackcon->rx_buf = g_realloc(slackcon->rx_buf,
				slackcon->rx_len + len + 1);
		memcpy(slackcon->rx_buf + slackcon->rx_len, buf, len + 1);
		slackcon->rx_len += len;

		/* Wait for more data before processing */
		return;
	}

	/* The server closed the connection, let's parse the data */
	slack_connection_process_data(slackcon);

	slack_connection_destroy(slackcon);
	
	slack_next_connection(sa);
}

static void 
slack_post_or_get_ssl_readdata_cb(
	gpointer data,
	PurpleSslConnection *ssl, 
	PurpleInputCondition cond
)
{
	slack_post_or_get_readdata_cb(data, -1, cond);
}


static void 
slack_post_or_get_connect_cb(
	gpointer data, 
	gint source,
	const gchar *error_message
)
{
	SlackConnection *slackcon;
	gssize len;

	slackcon = data;
	slackcon->connect_data = NULL;

	if (error_message)
	{
		purple_debug_error(PROTOCOL_CODE, "post_or_get_connect failure to %s\n", slackcon->url);
		purple_debug_error(PROTOCOL_CODE, "post_or_get_connect_cb %s\n",
				error_message);
		slack_fatal_connection_cb(slackcon);
		return;
	}

	slackcon->fd = source;

	len = write(slackcon->fd, slackcon->request->str,
			slackcon->request->len);
	if (len != slackcon->request->len)
	{
		purple_debug_error(PROTOCOL_CODE, "post_or_get_connect failed to write request\n");
		slack_fatal_connection_cb(slackcon);
		return;
	}
	slackcon->input_watcher = purple_input_add(slackcon->fd,
			PURPLE_INPUT_READ,
			slack_post_or_get_readdata_cb, slackcon);
}

static void 
slack_post_or_get_ssl_connect_cb(
	gpointer data,
	PurpleSslConnection *ssl, 
	PurpleInputCondition cond
)
{
	SlackConnection *slackcon;
	gssize len;

	slackcon = data;

	purple_debug_info(PROTOCOL_CODE, "post_or_get_ssl_connect_cb\n");

	len = purple_ssl_write(slackcon->ssl_conn,
			slackcon->request->str, slackcon->request->len);
	if (len != slackcon->request->len)
	{
		purple_debug_error(PROTOCOL_CODE, "post_or_get_ssl_connect failed to write request\n");
		slack_fatal_connection_cb(slackcon);
		return;
	}
	purple_ssl_input_add(slackcon->ssl_conn,
			slack_post_or_get_ssl_readdata_cb, slackcon);
}


static void 
slack_host_lookup_cb(
	GSList *hosts, 
	gpointer data,
	const char *error_message
)
{
	GSList *host_lookup_list;
	struct sockaddr_in *addr;
	gchar *hostname;
	gchar *ip_address;
	SlackAccount *sa;
	PurpleDnsQueryData *query;

	/* Extract variables */
	host_lookup_list = data;

	sa = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);
	hostname = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);
	query = host_lookup_list->data;
	host_lookup_list =
			g_slist_delete_link(host_lookup_list, host_lookup_list);

	/* The callback has executed, so we no longer need to keep track of
	 * the original query.  This always needs to run when the cb is 
	 * executed. */
	sa->dns_queries = g_slist_remove(sa->dns_queries, query);

	/* Any problems, capt'n? */
	if (error_message != NULL)
	{
		purple_debug_warning(PROTOCOL_CODE,
				"Error doing host lookup: %s\n", error_message);
		return;
	}

	if (hosts == NULL)
	{
		purple_debug_warning(PROTOCOL_CODE,
				"Could not resolve host name\n");
		return;
	}

	/* Discard the length... */
	hosts = g_slist_delete_link(hosts, hosts);
	/* Copy the address then free it... */
	addr = hosts->data;
	ip_address = g_strdup(inet_ntoa(addr->sin_addr));
	g_free(addr);
	hosts = g_slist_delete_link(hosts, hosts);

	/*
	 * DNS lookups can return a list of IP addresses, but we only cache
	 * the first one.  So free the rest.
	 */
	while (hosts != NULL)
	{
		/* Discard the length... */
		hosts = g_slist_delete_link(hosts, hosts);
		/* Free the address... */
		g_free(hosts->data);
		hosts = g_slist_delete_link(hosts, hosts);
	}

	g_hash_table_insert(sa->hostname_ip_cache, hostname, ip_address);
}

static void 
slack_cookie_foreach_cb(
	gchar *cookie_name,
	gchar *cookie_value, 
	GString *str
)
{
	g_string_append_printf(str, "%s=%s;", cookie_name, cookie_value);
}

gchar *slack_cookies_to_string(SlackAccount *sa)
{
	GString *str;
	str = g_string_new(NULL);

	g_hash_table_foreach(sa->cookie_table,
			(GHFunc)slack_cookie_foreach_cb, str);

	return g_string_free(str, FALSE);
}

static void 
slack_ssl_connection_error(
	PurpleSslConnection *ssl,
	PurpleSslErrorType errortype, 
	gpointer data
)
{
	SlackConnection *slackcon = data;
	SlackAccount *sa = slackcon->sa;
	PurpleConnection *pc = sa->pc;
	
	slackcon->ssl_conn = NULL;
	
	/* Try resend the request */
	slackcon->retry_count++;
	if (slackcon->retry_count < 3) {
		slack_connection_close(slackcon);
		slackcon->request_time = time(NULL);
		
		g_queue_push_head(sa->waiting_conns, slackcon);
		slack_next_connection(sa);
	} else {
		slack_connection_destroy(slackcon);
		purple_connection_ssl_error(pc, errortype);
	}
}

SlackConnection *
get_or_post_request(
	SlackAccount *sa,
	SlackMethod method,
	const gchar *host, 
	const gchar *url, 
	const gchar *postdata,
	SlackProxyCallbackFunc callback_func, 
	gpointer user_data,
	gboolean keepalive
)
{
	GString *request;
	gchar *cookies;
	SlackConnection *slackcon;
	gchar *real_url;
	gboolean is_proxy = FALSE;
	const gchar *user_agent;
	const gchar* const *languages;
	gchar *language_names;
	PurpleProxyInfo *proxy_info = NULL;
	gchar *proxy_auth;
	gchar *proxy_auth_base64;

	/* TODO: Fix keepalive and use it as much as possible */
	keepalive = FALSE;

	if (host == NULL)
		host = "slack.com";

	if (sa && sa->account)
	{
		if (purple_account_get_bool(sa->account, "use-https", FALSE)) 
		{
			purple_debug_info(PROTOCOL_CODE, "use-https\n");
			method |= SLACK_METHOD_SSL;
		}
	}

	if (sa && sa->account && !(method & SLACK_METHOD_SSL))
	{
		proxy_info = purple_proxy_get_setup(sa->account);
		if (purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_USE_GLOBAL)
			proxy_info = purple_global_proxy_get_info();
		if (purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_HTTP)
		{
			is_proxy = TRUE;
		}
	}

	purple_debug_info(PROTOCOL_CODE, "Federal gunship\n");
	
	if (is_proxy == TRUE)
	{
		real_url = g_strdup_printf("http://%s%s", host, url);
	} else {
		real_url = g_strdup(url);
	}

	cookies = slack_cookies_to_string(sa);
	user_agent = purple_account_get_string(sa->account, "user-agent", "Opera/9.50 (Windows NT 5.1; U; en-GB)");

	purple_debug_info(PROTOCOL_CODE, "Cookies\n");
	
	if (method & SLACK_METHOD_POST && !postdata)
		postdata = "";

	/* Build the request */
	request = g_string_new(NULL);
	g_string_append_printf(request, "%s %s HTTP/1.0\r\n",
			(method & SLACK_METHOD_POST) ? "POST" : "GET",
			real_url);
	if (is_proxy == FALSE)
		g_string_append_printf(request, "Host: %s\r\n", host);
	g_string_append_printf(request, "Connection: %s\r\n",
			(keepalive ? "Keep-Alive" : "close"));
	g_string_append_printf(request, "User-Agent: %s\r\n", user_agent);
	if (method & SLACK_METHOD_POST) {
		g_string_append_printf(request,
				"Content-Type: application/x-www-form-urlencoded\r\n");
		g_string_append_printf(request,
				"Content-length: %zu\r\n", strlen(postdata));
	}
	g_string_append_printf(request, "Accept: */*\r\n");
	//Only use cookies for slack.com 
	if (g_str_equal(host, "slack.com"))
		g_string_append_printf(request, "Cookie: %s\r\n", cookies);
	g_string_append_printf(request, "Accept-Encoding: gzip\r\n");
	if (is_proxy == TRUE)
	{
		if (purple_proxy_info_get_username(proxy_info) &&
			purple_proxy_info_get_password(proxy_info))
		{
			proxy_auth = g_strdup_printf("%s:%s", purple_proxy_info_get_username(proxy_info), purple_proxy_info_get_password(proxy_info));
			proxy_auth_base64 = purple_base64_encode((guchar *)proxy_auth, strlen(proxy_auth));
			g_string_append_printf(request, "Proxy-Authorization: Basic %s\r\n", proxy_auth_base64);
			g_free(proxy_auth_base64);
			g_free(proxy_auth);
		}
	}

	/* Tell the server what language we accept, so that we get error messages in our language (rather than our IP's) */
	languages = g_get_language_names();
	language_names = g_strjoinv(", ", (gchar **)languages);
	purple_util_chrreplace(language_names, '_', '-');
	g_string_append_printf(request, "Accept-Language: %s\r\n", language_names);
	g_free(language_names);

	purple_debug_info(PROTOCOL_CODE, "getting url %s\n", url);

	g_string_append_printf(request, "\r\n");
	if (method & SLACK_METHOD_POST)
		g_string_append_printf(request, "%s", postdata);

	/* If it needs to go over a SSL connection, we probably shouldn't print
	 * it in the debug log.  Without this condition a user's password is
	 * printed in the debug log */
	if (method == SLACK_METHOD_POST)
		purple_debug_info(PROTOCOL_CODE, "sending request data:\n%s\n",
			postdata);

	g_free(cookies);

	slackcon = g_new0(SlackConnection, 1);
	slackcon->sa = sa;
	slackcon->url = real_url;
	slackcon->method = method;
	slackcon->hostname = g_strdup(host);
	slackcon->request = request;
	slackcon->callback = callback_func;
	slackcon->user_data = user_data;
	slackcon->fd = -1;
	slackcon->connection_keepalive = keepalive;
	slackcon->request_time = time(NULL);
	
	g_queue_push_head(sa->waiting_conns, slackcon);
	slack_next_connection(sa);
	
	return slackcon;
}

static void 
slack_next_connection(SlackAccount *sa)
{
	SlackConnection *slackcon;
	
	g_return_if_fail(sa != NULL);
	
	if (!g_queue_is_empty(sa->waiting_conns))
	{
		if(g_slist_length(sa->conns) < SLACK_MAX_CONNECTIONS)
		{
			slackcon = g_queue_pop_tail(sa->waiting_conns);
			slack_attempt_connection(slackcon);
		}
	}
}

static gboolean
slack_connection_timedout(gpointer userdata)
{
	SlackConnection *slackcon = userdata;
	SlackAccount *sa = slackcon->sa;
	
	/* Try resend the request */
	slackcon->retry_count++;
	if (slackcon->retry_count < 3) {
		slack_connection_close(slackcon);
		slackcon->request_time = time(NULL);
		
		g_queue_push_head(sa->waiting_conns, slackcon);
		slack_next_connection(sa);
	} else {
		slack_fatal_connection_cb(slackcon);
	}
	
	return FALSE;
}

static void
slack_attempt_connection(SlackConnection *slackcon)
{
	gboolean is_proxy = FALSE;
	SlackAccount *sa = slackcon->sa;
	PurpleProxyInfo *proxy_info = NULL;

	if (sa && sa->account && !(slackcon->method & SLACK_METHOD_SSL))
	{
		proxy_info = purple_proxy_get_setup(sa->account);
		if (purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_USE_GLOBAL)
			proxy_info = purple_global_proxy_get_info();
		if (purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_HTTP)
		{
			is_proxy = TRUE;
		}
	}

	sa->conns = g_slist_prepend(sa->conns, slackcon);

	/*
	 * Do a separate DNS lookup for the given host name and cache it
	 * for next time.
	 *
	 * TODO: It would be better if we did this before we call
	 *       purple_proxy_connect(), so we could re-use the result.
	 *       Or even better: Use persistent HTTP connections for servers
	 *       that we access continually.
	 *
	 * TODO: This cache of the hostname<-->IP address does not respect
	 *       the TTL returned by the DNS server.  We should expire things
	 *       from the cache after some amount of time.
	 */
	if (!is_proxy && !(slackcon->method & SLACK_METHOD_SSL) && !g_hostname_is_ip_address(slackcon->hostname))
	{
		/* Don't do this for proxy connections, since proxies do the DNS lookup */
		gchar *host_ip;

		host_ip = g_hash_table_lookup(sa->hostname_ip_cache, slackcon->hostname);
		if (host_ip != NULL) {
			g_free(slackcon->hostname);
			slackcon->hostname = g_strdup(host_ip);
		} else if (sa->account && !sa->account->disconnecting) {
			GSList *host_lookup_list = NULL;
			PurpleDnsQueryData *query;

			host_lookup_list = g_slist_prepend(
					host_lookup_list, g_strdup(slackcon->hostname));
			host_lookup_list = g_slist_prepend(
					host_lookup_list, sa);


			query = purple_dnsquery_a(
#if PURPLE_VERSION_CHECK(3, 0, 0)
					slackcon->sa->account,
#endif
					slackcon->hostname, 80,
					slack_host_lookup_cb, host_lookup_list);

			sa->dns_queries = g_slist_prepend(sa->dns_queries, query);
			host_lookup_list = g_slist_append(host_lookup_list, query);
		}
	}

	if (slackcon->method & SLACK_METHOD_SSL) {
		purple_debug_info(PROTOCOL_CODE, "getting url %s\n", slackcon->hostname);

		slackcon->ssl_conn = purple_ssl_connect(
					sa->account, 
					slackcon->hostname,
					443, 
					slack_post_or_get_ssl_connect_cb,
					slack_ssl_connection_error, 
					slackcon
		);
	} else {
		slackcon->connect_data = purple_proxy_connect(NULL, sa->account,
				slackcon->hostname, 80, slack_post_or_get_connect_cb, slackcon);
	}
	
	slackcon->timeout_watcher = purple_timeout_add_seconds(120, slack_connection_timedout, slackcon);

	return;
}
