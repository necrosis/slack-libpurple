#ifndef _SLACK_MESSAGES_H
#define _SLACK_MESSAGES_H

#define MESSAGE_TEXT 0

#define SLACK_CMD_MESSAGE "message"
#define SLACK_CMD_CHANNEL "channel"


#include "cstring.h"
#include "slack_common.h"
#include "slack_connection.h"

#include <time.h>
#include <cmds.h>

typedef struct _SlackMessage
{
	string	message;
	time_t	time;
	string	user_id;
} SlackMessage;


void
slack_channel_query(SlackConnection * conn);


void
slack_message_send(
	SlackConnection *slack, 
	int id, 
	const char *message, 
	int msg_type
);

PurpleCmdRet
slack_parse_cmd(
	PurpleConversation * conv, 
	const gchar * cmd,
	gchar ** args, 
	gchar ** error, 
	void *data
);


#endif //_SLACK_MESSAGES_H
