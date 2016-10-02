#ifndef _SLACK_CHAT_H
#define _SLACK_CHAT_H

#include "slack_plugin.h"

#include <glib/gi18n.h>
#include <plugin.h>
#include <prpl.h>

typedef struct _SlackChannel
{
	SlackAccount *sa;
	PurpleBuddy *buddy;
	gchar *id;
	gchar *name;
	gint purple_id;
	guint typeflag;
} SlackChannel;

// on slack login
void 
slack_chat_login(PurpleAccount * account);


void
slack_chat_close(PurpleConnection *pc);


void
slack_join_chat(PurpleConnection * gc, 
		   GHashTable * data);

const char *
slack_list_icon(PurpleAccount * account,
		PurpleBuddy * buddy);

GList *
slack_statuses(PurpleAccount * acct);


void 
slack_buddy_free(PurpleBuddy *buddy);


int
slack_chat_send(
	PurpleConnection * gc, 
	int id, 
	const char *message,
	PurpleMessageFlags flags
);


gint 
slack_send_im(
	PurpleConnection *pc, 
	const gchar *who, 
	const gchar *msg,
	PurpleMessageFlags flags
);
#endif //_SLACK_CHAT_H
