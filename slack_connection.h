#ifndef _SLACK_CONNECTION_H
#define _SLACK_CONNECTION_H

/*system includes*/
#include <glib/gi18n.h>

/*purple includes*/
#include <plugin.h>
#include <prpl.h>

typedef struct _SlackConnection
{
	PurpleAccount *account;
	PurpleRoomlist *roomlist;
	PurpleConnection *gc;
	PurpleSslConnection *gsc;
	gchar *hostname;
	GHashTable *channels;
	guint message_timer;
	GList *queue;
	gboolean needs_join;
	gchar *desired_room;
} SlackConnection;


#endif//_SLACK_CONNECTION_H
