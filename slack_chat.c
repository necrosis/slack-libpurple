#include "slack_common.h"
#include "slack_connection.h"
#include "slack_chat.h"
#include "miniwebsock.h"
//#include "slack_messages.h"
#include <cmds.h>
#include <accountopt.h>
#include <connection.h>
#include <dnsquery.h>
#include <time.h>
#include <debug.h>

static void 
slack_check_state(SlackAccount *sa);

gboolean
slack_rtp_poll(gpointer userdata)
{
	SlackWSConnection *slackcon = userdata;
	SlackAccount *sa = slackcon->sa;

	purple_debug_info(PROTOCOL_CODE, "Poll\n");

	frame_value val = poll_frame(slackcon->socket_fd);

	if (val.type == frame_text)
	{
		purple_debug_info(PROTOCOL_CODE, "Get text: %s\n", val.data);

		/*
		int len = strlen(val.data);
		json_value* json = json_parse(val.data, len);
		
		if (json == NULL)
		{
			json_value* type = json_get_value(json, "type");	
		}
		*/
	}

	g_free(val.data);

	slackcon->poll_timeout = purple_timeout_add_seconds(1, slack_rtp_poll, slackcon);
	return FALSE;
}


gboolean 
slack_timeout(gpointer userdata)
{
	SlackAccount *sa = userdata;
	slack_check_state(sa);

	// If no response within 3 minutes, assume connection lost and try again
	purple_timeout_remove(sa->connection_timeout);
	sa->connection_timeout = purple_timeout_add_seconds(3 * 60, slack_timeout, sa);

	return FALSE;
}


static void
slack_check_new_messages_cb(
		SlackAccount *sa, 
		json_value *obj, 
		gpointer user_data
)
{
	SlackChannel *ch = user_data;
	json_value *ok = json_get_value(obj, "ok");
	if (ok->u.boolean)
	{
		json_value *latest = json_get_value(obj, "latest");
		if (latest)
			purple_account_set_string(sa->account, "last_message_time", latest->u.str.ptr);

		json_value *messages = json_get_value(obj, "messages");

		if (messages) 
		{
			gchar *html;
			gdouble timestamp;
			int length = messages->u.array.length;

			for (int i = 0; i < length; i++)
			{
				json_value *message = messages->u.array.values[i];
				json_value *type = json_get_value(message, "type");

				if (!g_strcmp0(type->u.str.ptr, "message"))
				{
					json_value *user = json_get_value(message, "user");
					json_value *text = json_get_value(message, "text");
					json_value *ts = json_get_value(message, "ts");

					timestamp = g_strtod(ts->u.str.ptr, NULL);
					
					html = purple_markup_escape_text(text->u.str.ptr, -1);
					purple_debug_info(
							PROTOCOL_CODE, 
							"Message from %s: %s", 
							user->u.str.ptr, 
							text->u.str.ptr
					);
					PurpleConversation *conv = purple_find_conversation_with_account(
									PURPLE_CONV_TYPE_IM, 
									ch->name, 
									sa->account
					);
					if (conv == NULL)
					{
						conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, sa->account, ch->name);
					}
					purple_conversation_write(conv, ch->name, html, PURPLE_MESSAGE_SEND, timestamp);

					/*
					serv_got_im(
						sa->pc, 
						ch->name,
						html, 
						PURPLE_MESSAGE_RECV, 
						timestamp
					);
					*/
					g_free(html);
				}
			}
		}
	}
	
	json_value_free(obj);
}

static void
slack_check_new_messages(
	SlackAccount *sa, 
	SlackChannel *ch, 
	const gchar *last_message_time
)
{
	if (ch->typeflag & BUDDY_CHANNEL)
	{
		GString *url = g_string_new("/api/channels.history?");

		g_string_append_printf(url, "token=%s&", purple_url_encode(sa->token));
		g_string_append_printf(url, "channel=%s&", purple_url_encode(ch->id));
		g_string_append_printf(url, "oldest=%s", purple_url_encode(last_message_time));
		
		purple_debug_info(PROTOCOL_CODE, "URL: %s\n", url->str);

		get_or_post_request(
				sa, 
				SLACK_METHOD_GET | SLACK_METHOD_SSL, 
				NULL, 
				url->str, 
				NULL, 
				slack_check_new_messages_cb, 
				ch,
				TRUE
		);
		
		g_string_free(url, TRUE);
	}
}

static void 
slack_check_state(SlackAccount *sa)
{
	GString *unix_now = g_string_new("");
	g_string_printf(unix_now, "%d.000000", (int)time(NULL));

	const gchar *last_message_time = purple_account_get_string(
						sa->account, 
						"last_message_time", 
						unix_now->str
					);
	

	guint channels_amount = g_slist_length(sa->channels);

	if (channels_amount) 
	{
		GSList *it;

		for (it = sa->channels; it; it = it->next)
			slack_check_new_messages(sa, it->data, last_message_time);
	}

	sa->poll_timeout = purple_timeout_add_seconds(3, slack_timeout, sa);
	g_string_free(unix_now, TRUE);
}

static void
slack_send_im_cb(
		G_GNUC_UNUSED SlackAccount *sa, 
		json_value *obj, 
		G_GNUC_UNUSED gpointer user_data
)
{
	json_value *ok = json_get_value(obj, "ok");
	if (!ok->u.boolean)
	{
		purple_debug_error(PROTOCOL_CODE, "Message send error\n");
	}


	json_value_free(obj);
}

gint 
slack_send_im(
		PurpleConnection *pc, 
		const gchar *who, 
		const gchar *msg,
		G_GNUC_UNUSED PurpleMessageFlags flags
)
{
	SlackAccount *sa = pc->proto_data;
	GString *url = g_string_new("/api/chat.postMessage?");

	g_string_append_printf(url, "token=%s&", purple_url_encode(sa->token));
	g_string_append_printf(url, "channel=%s&", purple_url_encode(who));
	g_string_append(url, "as_user=true&");
	g_string_append_printf(url, "text=%s", purple_url_encode(msg));

	get_or_post_request(
			sa, 
			SLACK_METHOD_GET | SLACK_METHOD_SSL, 
			NULL, 
			url->str, 
			NULL, 
			slack_send_im_cb, 
			NULL, 
			TRUE
	);

	g_string_free(url, TRUE);
	return 1;
}

static void
slack_read_users_cb(
		SlackAccount *sa, 
		json_value *obj, 
		G_GNUC_UNUSED gpointer user_data
)
{
	json_value *ok = json_get_value(obj, "ok");
	if (ok->u.boolean)
	{
		json_value *members = json_get_value(obj, "members");
		int length = members->u.array.length;
		PurpleGroup *group = NULL;
		const char *status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);

		for (int i = 0; i< length; i++)
		{
			json_value *member = members->u.array.values[i];
			json_value* deleted = json_get_value(member, "deleted");

			if (deleted->u.boolean)
				continue;

			json_value* id = json_get_value(member, "id");
			json_value* name = json_get_value(member, "name");

			SlackChannel *ch = g_new0(SlackChannel, 1);
			ch->typeflag = BUDDY_USER;
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
						group = purple_group_new("Slack");
						purple_blist_add_group(group, NULL);
					}
					
				}
				purple_blist_add_buddy(
						purple_buddy_new(sa->account, ch->id, NULL), 
						NULL, 
						group, 
						NULL
				);
			}

			purple_serv_got_private_alias(
					sa->pc, 
					name->u.str.ptr, 
					id->u.str.ptr
			);
			purple_prpl_got_user_status(
					sa->account, 
					id->u.str.ptr, 
					status_id, 
					"message", 
					NULL, 
					NULL
			); // check status

			sa->channels = g_slist_append(sa->channels, ch);
		}
	}
	json_value_free(obj);

	slack_check_state(sa);
}

static void
slack_read_channels_cb(
		SlackAccount *sa, 
		json_value *obj, 
		G_GNUC_UNUSED gpointer user_data
)
{
	json_value *ok = json_get_value(obj, "ok");
	if (ok->u.boolean)
	{
		json_value *channels = json_get_value(obj, "channels");
		int length = channels->u.array.length;
		PurpleGroup *group = NULL;
		PurpleBuddy *buddy;
		SlackBuddy *sbuddy;
		const char *status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);

		for (int i = 0; i < length; i++)
		{
			json_value *channel = channels->u.array.values[i];
			json_value *id = json_get_value(channel, "id");
			json_value *name = json_get_value(channel, "name");

			SlackChannel *ch = g_new0(SlackChannel, 1);
			ch->typeflag = BUDDY_CHANNEL;
			ch->sa = sa;
			ch->id = g_strdup(id->u.str.ptr);
			ch->name = g_strdup(name->u.str.ptr);
			ch->purple_id = g_str_hash(id->u.str.ptr);

			buddy = purple_find_buddy(sa->account, ch->id);

			if (!buddy)
			{
				if (!group)
				{
					group = purple_find_group("Slack");
					if (!group)
					{
						group = purple_group_new("Slack");
						purple_blist_add_group(group, NULL);
					}
					
				}
				buddy = purple_buddy_new(sa->account, ch->id, NULL);
				sbuddy = g_new0(SlackBuddy, 1);
				buddy->proto_data = sbuddy;
				sbuddy->slack_id = g_strdup(id->u.str.ptr);

				purple_blist_add_buddy(buddy, NULL, group, NULL);
			}

			sbuddy = buddy->proto_data;

			if (sbuddy == NULL)
			{
				sbuddy = g_new0(SlackBuddy, 1);
				buddy->proto_data = sbuddy;
				sbuddy->slack_id = g_strdup(id->u.str.ptr);
				sbuddy->nickname = g_strdup(name->u.str.ptr);
				sbuddy->avatar = NULL;
				sbuddy->personflags = BUDDY_CHANNEL;
			}

			serv_got_alias(sa->pc, sbuddy->slack_id, sbuddy->nickname);
			purple_prpl_got_user_status(
					sa->account, 
					id->u.str.ptr, 
					status_id, 
					"message", 
					NULL, 
					NULL
			); // check status

			sa->channels = g_slist_append(sa->channels, ch);
		}
	}

	json_value_free(obj);

	slack_check_state(sa);
}

void
slack_get_channels(SlackAccount* sa) 
{

	GString *url = g_string_new("/api/channels.list?");
	g_string_append_printf(url, "token=%s", purple_url_encode(sa->token));

	get_or_post_request(
			sa, 
			SLACK_METHOD_GET | SLACK_METHOD_SSL, 
			NULL, 
			url->str, 
			NULL, 
			slack_read_channels_cb, 
			NULL, 
			TRUE
	);
	g_string_free(url, TRUE);

	/*
	url = g_string_new("/api/channels.list?"); // /api/users.list?
	g_string_append_printf(url, "token=%s", purple_url_encode(sa->token));

	purple_debug_info("slack", "URL: %s\n", url->str);

	get_or_post_request(
			sa, 
			SLACK_METHOD_GET | SLACK_METHOD_SSL, 
			NULL, 
			url->str, 
			NULL, 
			slack_read_users_cb,
			NULL, 
			TRUE
	);
	g_string_free(url, TRUE);
	*/
}

void 
slack_chat_login(PurpleAccount * account)
{
	// check token - no token - no work
	const gchar *slack_token = purple_account_get_string(account, "api_token", NULL);
	if (!slack_token)
	{
		purple_debug_error(PROTOCOL_CODE, "Token required");
		return;
	}

	// Init connection
	PurpleConnection *gc = purple_account_get_connection(account);
	SlackAccount* sa = g_new0(SlackAccount, 1);

	gc->proto_data = sa;

	if (!purple_ssl_is_supported()) 
	{
		purple_connection_error (
			gc,
			"Server requires TLS/SSL for login.  No TLS/SSL support found."
		);
		return;
	}

	sa->account = account;
	sa->pc = gc;
	sa->cookie_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	sa->token = g_strdup(slack_token);
	sa->channels = NULL;

	sa->hostname_ip_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	//sa->sent_messages_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	sa->waiting_conns = g_queue_new();
	sa->rtm = NULL;
	//sa->last_message_timestamp = purple_account_get_int(sa->account, "last_message_timestamp", 0);

	const char *username = purple_account_get_username(account);
	
	purple_connection_set_state(gc, PURPLE_CONNECTING);
	purple_connection_update_progress(gc, _("Logging in"), 1, 4);

	//acc->hostname = g_strdup("necrosis");
	purple_connection_set_display_name(gc, "necrosis");

	//purple_debug_info("slack", "hostname: %s\n", acc->hostname);
	/*
	purple_cmd_register(SLACK_CMD_MESSAGE, "s", PURPLE_CMD_P_PRPL, f, prpl_id,
			    slack_parse_cmd,
			    "me &lt;action to perform&gt;:  Perform an action.",
			    conn);
	*/
	purple_connection_set_state(gc, PURPLE_CONNECTED);

	slack_get_channels(sa);
}

void 
slack_buddy_free(PurpleBuddy *buddy)
{
	SlackBuddy *sbuddy = buddy->proto_data;
	if (sbuddy != NULL)
	{
		buddy->proto_data = NULL;

		g_free(sbuddy->slack_id);
		g_free(sbuddy->nickname);
		g_free(sbuddy->avatar);

		g_free(sbuddy);
	}
}

void
slack_chat_close(PurpleConnection *pc)
{
	SlackAccount *sa;

	g_return_if_fail(pc != NULL);
	g_return_if_fail(pc->proto_data != NULL);

	sa = pc->proto_data;

	if (sa->poll_timeout)
		purple_timeout_remove(sa->poll_timeout);

	if (sa->connection_timeout)
		purple_timeout_remove(sa->connection_timeout);

	
	while (!g_queue_is_empty(sa->waiting_conns))
		slack_connection_destroy(g_queue_pop_tail(sa->waiting_conns));
	g_queue_free(sa->waiting_conns);


	while (sa->conns != NULL)
		slack_connection_destroy(sa->conns->data);

	while (sa->dns_queries != NULL) 
	{
		PurpleDnsQueryData *dns_query = sa->dns_queries->data;
		purple_debug_info(SLACK_PLUGIN_ID, "canceling dns query for %s\n",
					purple_dnsquery_get_host(dns_query));
		sa->dns_queries = g_slist_remove(sa->dns_queries, dns_query);
		purple_dnsquery_destroy(dns_query);
	}


	g_hash_table_destroy(sa->cookie_table);
	g_hash_table_destroy(sa->hostname_ip_cache);

	g_free(sa->token);
	g_free(sa);
}

void
slack_join_chat(G_GNUC_UNUSED PurpleConnection * gc, 
		G_GNUC_UNUSED GHashTable * data)
{
	purple_debug_info(PROTOCOL_CODE, "Join chat\n");
}

const char *
slack_list_icon(G_GNUC_UNUSED PurpleAccount * account,
		     G_GNUC_UNUSED PurpleBuddy * buddy)
{
	return PROTOCOL_CODE;
}

GList *
slack_statuses(G_GNUC_UNUSED PurpleAccount * acct)
{
	GList *types = NULL;
	PurpleStatusType *status;

	// Chat is online, if status from service online 
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL,
					     "Online", TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	// Chat is offline, if status from service offline 
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL,
					     "Offline", TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	return types;
}

int
slack_chat_send(
	G_GNUC_UNUSED PurpleConnection * gc, 
	G_GNUC_UNUSED int id, 
	G_GNUC_UNUSED const char *message,
	G_GNUC_UNUSED PurpleMessageFlags flags
)
{
	/*
	slack_message_send(
		gc->proto_data, 
		id, 
		message, 
		MESSAGE_TEXT 
	);
	*/
	
	return 1; 
}
