#include "slack_common.h"
#include "slack_connection.h"
#include "slack_chat.h"
//#include "slack_messages.h"
#include <cmds.h>
#include <accountopt.h>
#include <connection.h>
#include <debug.h>


static void
slack_send_im_cb(SlackAccount *sa, json_value *obj, G_GNUC_UNUSED gpointer user_data)
{
	json_value *ok = json_get_value(obj, "ok");
	if (!ok->u.boolean)
	{
		purple_debug_error(PROTOCOL_CODE, "Message send error\n");
	}

}

gint 
slack_send_im(
		PurpleConnection *pc, 
		const gchar *who, 
		const gchar *msg,
		PurpleMessageFlags flags
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
slack_read_users_cb(SlackAccount *sa, json_value *obj, gpointer user_data)
{
	json_value *ok = json_get_value(obj, "ok");
	if (ok->u.boolean)
	{
		json_value *members = json_get_value(obj, "members");
		int length = members->u.array.length;
		PurpleGroup *group = NULL;
		const char *status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);

		purple_debug_info(PROTOCOL_CODE, "Users: %d\n", length);
		
		for (int i = 0; i< length; i++)
		{
			json_value *member = members->u.array.values[i];
			json_value* deleted = json_get_value(member, "deleted");

			if (deleted->u.boolean)
				continue;

			json_value* id = json_get_value(member, "id");
			json_value* name = json_get_value(member, "name");

			purple_debug_info(PROTOCOL_CODE, "name %s\n", name->u.str.ptr);

			SlackChannel *ch = g_new0(SlackChannel, 1);
			ch->typeflag = BUDDY_USER;
			ch->sa = sa;
			ch->id = g_strdup(id->u.str.ptr);
			ch->name = g_strdup(name->u.str.ptr);
			ch->purple_id = g_str_hash(id->u.str.ptr);


			if (!purple_find_buddy(sa->account, ch->id))
			{
				purple_debug_info(PROTOCOL_CODE, "%s not found\n", name->u.str.ptr);
				
				if (!group)
				{
					group = purple_find_group("Slack");
					if (!group)
					{
						group = purple_group_new("Slack");
						purple_blist_add_group(group, NULL);
					}
					
				}
				purple_debug_info(PROTOCOL_CODE, "Usr add\n");
				purple_blist_add_buddy(purple_buddy_new(sa->account, ch->id, NULL), NULL, group, NULL);
			}

			purple_serv_got_private_alias(sa->pc, name->u.str.ptr, id->u.str.ptr);
			purple_prpl_got_user_status(sa->account, id->u.str.ptr, status_id, "message", NULL, NULL); // check status

			//TODO load history
			//TODO start message poll

			sa->channels = g_list_append(sa->channels, ch);
		}
	}
	
	json_value_free(obj);
}

static void
slack_read_channels_cb(SlackAccount *sa, json_value *obj, gpointer user_data)
{
	json_value *ok = json_get_value(obj, "ok");
	if (ok->u.boolean)
	{
		json_value *channels = json_get_value(obj, "channels");
		int length = channels->u.array.length;
		purple_debug_info(PROTOCOL_CODE, "Buddys %d\n", length);
		PurpleGroup *group = NULL;
		PurpleBuddy *buddy;
		SlackBuddy *sbuddy;
		const char *status_id = purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE);

		for (int i = 0; i < length; i++)
		{
			json_value *channel = channels->u.array.values[i];
			json_value *id = json_get_value(channel, "id");
			json_value *name = json_get_value(channel, "name");

			purple_debug_info(PROTOCOL_CODE, "name %s\n", name->u.str.ptr);

			SlackChannel *ch = g_new0(SlackChannel, 1);
			ch->typeflag = BUDDY_CHANNEL;
			ch->sa = sa;
			ch->id = g_strdup(id->u.str.ptr);
			ch->name = g_strdup(name->u.str.ptr);
			ch->purple_id = g_str_hash(id->u.str.ptr);

			buddy = purple_find_buddy(sa->account, ch->id);

			if (!buddy)
			{
				purple_debug_info(PROTOCOL_CODE, "%s not found\n", name->u.str.ptr);
				
				if (!group)
				{
					group = purple_find_group("Slack");
					if (!group)
					{
						group = purple_group_new("Slack");
						purple_blist_add_group(group, NULL);
					}
					
				}
				purple_debug_info(PROTOCOL_CODE, "Usr add\n");
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
			purple_prpl_got_user_status(sa->account, id->u.str.ptr, status_id, "message", NULL, NULL); // check status
			//purple_serv_got_private_alias(sa->pc, name->u.str.ptr, id->u.str.ptr);

			//TODO load history
			//TODO start message poll

			sa->channels = g_list_append(sa->channels, ch);
		}
	}

	
	json_value_free(obj);
}

void
slack_get_channels(SlackAccount* sa) 
{

	GString *url = g_string_new("/api/channels.list?");
	g_string_append_printf(url, "token=%s", purple_url_encode(sa->token));

	purple_debug_info("slack", "URL: %s\n", url->str);
	
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

	sa->hostname_ip_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	//sa->sent_messages_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	sa->waiting_conns = g_queue_new();
	//sa->last_message_timestamp = purple_account_get_int(sa->account, "last_message_timestamp", 0);

	const char *username = purple_account_get_username(account);
	
	purple_connection_set_state(gc, PURPLE_CONNECTING);
	purple_connection_update_progress(gc, _("Logging in"), 1, 4);

	// TODO get real name
	//acc->hostname = g_strdup("necrosis");
	purple_connection_set_display_name(gc, "necrosis");

	purple_debug_info("slack", "username: %s\n", username);
	//purple_debug_info("slack", "hostname: %s\n", acc->hostname);
	/*
	purple_cmd_register(SLACK_CMD_MESSAGE, "s", PURPLE_CMD_P_PRPL, f, prpl_id,
			    slack_parse_cmd,
			    "me &lt;action to perform&gt;:  Perform an action.",
			    conn);
	*/
	purple_connection_set_state(gc, PURPLE_CONNECTED);

	slack_get_channels(sa);
		
	purple_debug_info("slack", "url finished\n");
	
	//slack_read_channels(acc);
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
slack_chat_close(G_GNUC_UNUSED PurpleAccount * account)
{
	purple_debug_info(PROTOCOL_CODE, "Chat close\n");
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
	PurpleConnection * gc, 
	int id, 
	const char *message,
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
