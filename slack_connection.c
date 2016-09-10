#include "slack_connection.h"
#include "slack_common.h"
#include "slack_chat.h"
#include "json.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <debug.h>
#include <blist.h>

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

		if (!purple_find_buddy(sa->account, ch->name))
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
				purple_blist_add_buddy(purple_buddy_new(sa->account, ch->name, NULL), NULL, group, NULL);
			}
		}

		//TODO load history
		//TODO start message poll

		sa->channels = g_list_append(sa->channels, ch);
	}
}

void
slack_read_channels(SlackConnection * conn)
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
					slack_process_channels_json(conn->sa, json);
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

