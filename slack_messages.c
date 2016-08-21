#include "slack_messages.h"
#include "json.h"
#include <curl/curl.h>
#include <stdlib.h>


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
slack_process_channels_json(PurpleRoomlist *roomlist, json_value *jobj)
{
	// iterate through channels list
	PurpleRoomlistRoom *room = NULL;
	json_value *channels = json_get_value(jobj, "channels");
	int length = channels->u.array.length;
	
	for (int i = 0; i < length; i++)
	{
		json_value *channel = channels->u.array.values[i];
		json_value *id = json_get_value(channel, "id");
		json_value *name = json_get_value(channel, "name");

		room = purple_roomlist_room_new(
			PURPLE_ROOMLIST_ROOMTYPE_ROOM, 
			name->u.str.ptr, 
			NULL 
		);
		purple_roomlist_room_add_field(
			roomlist,
			room, 
			channel->u.str.ptr
		);
		purple_roomlist_room_add_field(
			roomlist,
			room, 
			id->u.str.ptr
		);
		purple_roomlist_room_add(roomlist, room);
	}
}

void
slack_channel_query(SlackConnection * conn)
{
	UNUSED(conn);
	purple_debug_info(PROTOCOL_CODE, "Channels data update\n");
	
	CURLcode res;
	CURL *curl = curl_easy_init();

	if (curl) 
	{
		string s = NULL_STRING;
		create_string(&s, 0);

		curl_easy_setopt(curl, CURLOPT_URL, "https://slack.com/api/channels.list?token=xoxp-37479727381-37710578004-71297034352-53a89da583");
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
					slack_process_channels_json(conn->roomlist, json);
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
slack_message_send(
	SlackConnection *slack, 
	int id, 
	const char *message, 
	int msg_type
)
{
	UNUSED(slack);
	UNUSED(id);
	UNUSED(message);
	UNUSED(msg_type);
}


static void
slack_channel_update(
	SlackConnection* conn,
	gint id,
	gchar * channel
)
{
	UNUSED(conn);
	UNUSED(id);
	UNUSED(channel);
}


PurpleCmdRet
slack_parse_cmd(
	PurpleConversation * conv, 
	const gchar * cmd,
	gchar ** args, 
	G_GNUC_UNUSED gchar ** error, 
	void *data
)
{
	PurpleConnection *gc = purple_conversation_get_gc(conv);
	PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
	GString *message = NULL;

	if (!gc)
		return PURPLE_CMD_RET_FAILED;

	purple_debug_info("slack", "cmd %s: args[0]: %s\n", cmd, args[0]);
	
	if (g_strcmp0(cmd, SLACK_CMD_MESSAGE) == 0) {
		/* send a message */
		message = g_string_new("*");
		g_string_append(message, args[0]);
		g_string_append(message, "*");

		slack_message_send(
			data, 
			chat->id, 
			message->str,
			MESSAGE_TEXT
		);
	} else if (g_strcmp0(cmd, SLACK_CMD_CHANNEL) == 0) {
		/* do a room request */
		if (args[0])
			slack_channel_update(data, chat->id, args[0]);
		else
			slack_channel_update(data, chat->id, "");
	}/* else if (g_strcmp0(cmd, CAMPFIRE_CMD_ROOM) == 0) {
		// do a room request 
		campfire_room_update(data, chat->id, NULL, args[0]);
	}*/

	return PURPLE_CMD_RET_OK;
}
