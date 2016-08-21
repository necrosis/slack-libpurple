#include "slack_common.h"
#include "slack_connection.h"
#include "slack_chat.h"
#include "slack_messages.h"
#include <cmds.h>
#include <accountopt.h>
#include <debug.h>

void 
slack_chat_login(G_GNUC_UNUSED PurpleAccount * account)
{
	// Init connection
	PurpleConnection *gc = purple_account_get_connection(account);
	const char *username = purple_account_get_username(account);
	
	SlackConnection* conn;
	// char *pos;
	
	PurpleCmdFlag f = PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PRPL_ONLY;
	gchar* prpl_id = SLACK_PLUGIN_ID; 
	
	conn = g_new0(SlackConnection, 1);
	
	conn->gc = gc;
	conn->account = account;

	// TODO get real name
	purple_connection_set_display_name(gc, "necrosis");

	purple_debug_info("slack", "username: %s\n", username);
	purple_debug_info("slack", "hostname: %s\n", conn->hostname);

	gc->proto_data = conn;

	purple_cmd_register(SLACK_CMD_MESSAGE, "s", PURPLE_CMD_P_PRPL, f, prpl_id,
			    slack_parse_cmd,
			    "me &lt;action to perform&gt;:  Perform an action.",
			    conn);

	purple_connection_set_state(gc, PURPLE_CONNECTED);
}


void
slack_chat_close(G_GNUC_UNUSED PurpleAccount * account)
{
}

void
slack_chat_buddy_free(G_GNUC_UNUSED PurpleBuddy * buddy)
{}

void
slack_join_chat(G_GNUC_UNUSED PurpleConnection * gc, 
		G_GNUC_UNUSED GHashTable * data)
{
	purple_debug_info(PROTOCOL_CODE, "Join chat");
}

void
slack_chat_leave(G_GNUC_UNUSED PurpleConnection * gc, 
		 G_GNUC_UNUSED int id)
{
	purple_debug_info(PROTOCOL_CODE, "Leave chat");
}

gchar *
slack_status_text(G_GNUC_UNUSED PurpleBuddy * buddy)
{
	return NULL;
}

void
slack_set_status(G_GNUC_UNUSED PurpleAccount * acct,
		 G_GNUC_UNUSED PurpleStatus * status)
{
}

void
slack_buddy_free(G_GNUC_UNUSED PurpleBuddy * buddy)
{
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
					     _("Online"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	// Chat is offline, if status from service offline 
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL,
					     _("Offline"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	return types;
}

GList*
slack_chat_info(G_GNUC_UNUSED PurpleConnection * gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = "_Channel:";
	pce->identifier = "channel";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	return m;
}

char *
slack_get_chat_name(GHashTable * data)
{
	return g_strdup(g_hash_table_lookup(data, "channel"));
}

char *
slack_get_channel_name(GHashTable * data)
{
	return g_strdup(g_hash_table_lookup(data, "channel"));
}

PurpleRoomlist *
slack_roomlist_get_list(PurpleConnection * gc)
{
	SlackConnection *slack = gc->proto_data;
	GList *fields = NULL;
	PurpleRoomlistField *f;

	purple_debug_info("slack", "initiating ROOMLIST GET LIST\n");

	if (slack->roomlist) {
		purple_roomlist_unref(slack->roomlist);
	}

	slack->roomlist = purple_roomlist_new(
		purple_connection_get_account(gc)
	);

	f = purple_roomlist_field_new(
		PURPLE_ROOMLIST_FIELD_STRING, 
		"Channels",
		"channels", 
		FALSE
	);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(
		PURPLE_ROOMLIST_FIELD_STRING, 
		"",
		"id",
		TRUE
	);
	fields = g_list_append(fields, f);

	purple_roomlist_set_fields(slack->roomlist, fields);
	purple_roomlist_set_in_progress(slack->roomlist, TRUE);
	
	slack_channel_query(slack);

	purple_debug_info("slack", "Get list returned\n");
	purple_roomlist_set_in_progress(slack->roomlist, FALSE);

	return slack->roomlist;
}


void
slack_roomlist_cancel(PurpleRoomlist * list)
{
	PurpleConnection *gc = purple_account_get_connection(list->account);
	SlackConnection *slack= NULL;

	if (gc == NULL)
		return;

	slack = gc->proto_data;

	purple_roomlist_set_in_progress(list, FALSE);

	if (slack->roomlist == list) {
		slack->roomlist = NULL;
		purple_roomlist_unref(list);
	}
}


int
slack_chat_send(
	PurpleConnection * gc, 
	int id, 
	const char *message,
	G_GNUC_UNUSED PurpleMessageFlags flags
)
{
	slack_message_send(
		gc->proto_data, 
		id, 
		message, 
		MESSAGE_TEXT 
	);
	
	return 1; 
}
