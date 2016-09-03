#ifndef _SLACK_CONNECTION_H
#define _SLACK_CONNECTION_H

/*
#include <glib/gi18n.h>

#include <plugin.h>
#include <prpl.h>
*/

#include "slack_plugin.h"

typedef struct _SlackConnection
{
	SlackAccount *sa;
	PurpleConnection *gc;
	PurpleSslConnection *gsc;
	gchar *hostname;
	gchar *token;
	guint message_timer;
	GList *queue;
	gboolean needs_join;
	gchar *desired_room;
} SlackConnection;

void
slack_read_channels(SlackConnection * conn);


#endif//_SLACK_CONNECTION_H
