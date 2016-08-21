#ifndef _SLACK_CHAT_H
#define _SLACK_CHAT_H

#include <glib/gi18n.h>

#include <plugin.h>
#include <prpl.h>
#include <roomlist.h>

// on slack login
void 
slack_chat_login(PurpleAccount * account);

// on slack close
void
slack_chat_close(PurpleAccount * account);

void
slack_chat_buddy_free(PurpleBuddy * buddy);

void
slack_join_chat(PurpleConnection * gc, 
		   GHashTable * data);

void
slack_chat_leave(PurpleConnection * gc, int id);

gchar *
slack_status_text(PurpleBuddy * buddy);

void
slack_set_status(PurpleAccount * acct,
		 PurpleStatus * status);

void
slack_buddy_free(PurpleBuddy * buddy);

const char *
slack_list_icon(PurpleAccount * account,
		PurpleBuddy * buddy);

GList *
slack_statuses(PurpleAccount * acct);

GList* 
slack_chat_info(PurpleConnection * gc);

char *
slack_get_chat_name(GHashTable * data);

char *
slack_get_channel_name(GHashTable * data);

PurpleRoomlist *
slack_roomlist_get_list(PurpleConnection * gc);

void
slack_roomlist_cancel(PurpleRoomlist * list);

int
slack_chat_send(
	PurpleConnection * gc, 
	int id, 
	const char *message,
	PurpleMessageFlags flags
);
#endif //_SLACK_CHAT_H
