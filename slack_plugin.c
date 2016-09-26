#ifndef PURPLE_PLUGINS
#define PURPLE_PLUGINS
#endif

#include "slack_common.h"
#include "slack_chat.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib.h>

/* This will prevent compiler errors in some instances and is better explained in the
 * how-to documents on the wiki */
#ifndef G_GNUC_NULL_TERMINATED
# if __GNUC__ >= 4
#  define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
# else
#  define G_GNUC_NULL_TERMINATED
# endif
#endif

#include <debug.h>
#include <plugin.h>
#include <notify.h>
#include <version.h>
#include <accountopt.h>

PurplePlugin *slack_plugin = NULL;

/*******************************************/
static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	UNUSED(plugin);
	UNUSED(error);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	UNUSED(plugin);
	UNUSED(error);
	return TRUE;
}
/*******************************************/

static gboolean
libpurple2_plugin_load(PurplePlugin *plugin)
{
	return plugin_load(plugin, NULL);
}

static gboolean
libpurple2_plugin_unload(PurplePlugin *plugin)
{
	return plugin_unload(plugin, NULL);
}

static PurplePluginProtocolInfo slack_protocol_info = {
	/* options */
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD,	/*| OPT_PROTO_SLASH_COMMANDS_NATIVE, */
	NULL,			/* user_splits */
	NULL,			/* protocol_options */
	{			/* icon_spec, a PurpleBuddyIconSpec */
	 "png,jpg,gif",		/* format */
	 0,			/* min_width */
	 0,			/* min_height */
	 128,			/* max_width */
	 128,			/* max_height */
	 10000,			/* max_filesize */
	 PURPLE_ICON_SCALE_DISPLAY,	/* scale_rules */
	 },
	slack_list_icon,	/* list_icon */
	NULL,			/* list_emblems */
	NULL,			/* status_text */
	NULL,			/* tooltip_text */
	slack_statuses,		/* status_types */
	NULL,			/* blist_node_menu */
	NULL,			/* chat_info */
	NULL,			/* chat_info_defaults */
	slack_chat_login,	/* login */
	slack_chat_close,	/* close */
	slack_send_im,		/* send_im */
	NULL,			/* set_info */
	NULL,			/* send_typing */
	NULL,			/* get_info */
	NULL,			/* set_status */
	NULL,			/* set_idle */
	NULL,			/* change_passwd */
	NULL,			/* add_buddy */
	NULL,			/* add_buddies */
	NULL,			/* remove_buddy */
	NULL,			/* remove_buddies */
	NULL,			/* add_permit */
	NULL,			/* add_deny */
	NULL,			/* rem_permit */
	NULL,			/* rem_deny */
	NULL,			/* set_permit_deny */
	slack_join_chat,	/* join_chat */	
	NULL,			/* reject chat invite */
	NULL,			/* get_chat_name */
	NULL,			/* chat_invite */
	NULL,			/* chat_leave */
	NULL,			/* chat_whisper */
	slack_chat_send,	/* chat_send */
	NULL,			/* keepalive */
	NULL,			/* register_user */
	NULL,			/* get_cb_info */
	NULL,			/* get_cb_away */
	NULL,			/* alias_buddy */
	NULL,			/* group_buddy */
	NULL,			/* rename_group */
	slack_buddy_free,	/* buddy_free */
	NULL,			/* convo_closed */
	purple_normalize_nocase,	/* normalize */
	NULL,			/* set_buddy_icon */
	NULL,			/* remove_group */
	NULL,			/* get_cb_real_name */
	NULL,			/* set_chat_topic */
	NULL,			/* find_blist_chat */
	NULL,			/* roomlist_get_list */
	NULL,			/* roomlist_cancel */
	NULL,			/* roomlist_expand_category */
	NULL,			/* can_receive_file */
	NULL,			/* send_file */
	NULL,			/* new_xfer */
	NULL,			/* offline_message */
	NULL,			/* whiteboard_prpl_ops */
	NULL,			/* send_raw */
	NULL,			/* roomlist_room_serialize */
	NULL,			/* unregister_user */
	NULL,			/* send_attention */
	NULL,			/* attention_types */
	sizeof(PurplePluginProtocolInfo),	/* struct_size */
	NULL,			/*campfire_get_account_text_table *//* get_account_text_table */
	NULL,			/* initiate_media */
	NULL,			/* get_media_caps */
#if PURPLE_MAJOR_VERSION > 1
#if PURPLE_MINOR_VERSION > 6
	NULL,			/* get_moods */
	NULL,			/* set_public_alias */
	NULL,			/* get_public_alias */
#if PURPLE_MINOR_VERSION > 7
	NULL,			/* add_buddy_with_invite */
	NULL,			/* add_buddies_with_invite */
#endif /* PURPLE_MINOR_VERSION > 7 */
#endif /* PURPLE_MINOR_VERSION > 6 */
#endif /* PURPLE_MAJOR_VERSION > 1 */
};

static PurplePluginInfo info = {
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_PROTOCOL,
    NULL,
    0,
    NULL,
    PURPLE_PRIORITY_DEFAULT,
    SLACK_PLUGIN_ID,
    "Slack",
    SLACK_PLUGIN_VERSION ,
    "Slack protocol plugins",          
    "Add slack protocol support to libpurple.",          
    "Valeriy Golenkov <valery.golenkov@gmail.com>",                          
    "github",     
    libpurple2_plugin_load,                   
    libpurple2_plugin_unload,                          
    NULL,                          
    NULL,                          
    &slack_protocol_info,	/* extra info */ 
    NULL,                        
    NULL,                   
    NULL,                          
    NULL,                          
    NULL,                          
    NULL                           
};                               
    
static void                        
init_plugin(G_GNUC_UNUSED PurplePlugin *plugin)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option_token; 

	split = purple_account_user_split_new("Hostname", NULL, '@');
	slack_protocol_info.user_splits =
		g_list_append(slack_protocol_info.user_splits, split);

	option_token =
		purple_account_option_string_new("API token", "api_token",
						 NULL);
	slack_protocol_info.protocol_options =
		g_list_append(slack_protocol_info.protocol_options,
			      option_token);

}

PURPLE_INIT_PLUGIN(slack_plugin, init_plugin, info)
