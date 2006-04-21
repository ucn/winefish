/* $Id$
 *
 * lykey, demo gtk-program with emacs-key style
 *
 * Copyright (c) 2006 kyanh <kyanh@o2.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"

#ifdef SNOOPER2

#define DEBUG

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "bluefish.h"
#include "snooper2.h"

static void report(GdkEventKey *kevent, gint newline, const gchar *str, const gchar *before) {
	gchar *tmpstr;
	if (kevent) {
		const gchar *ctrl, *shift, *mod1;
		gchar *ch;
		if ( ( (GdkEventKey*)kevent)->keyval == GDK_Escape ) {
			ch = g_strdup("escape");
		}else{
			guint32 character;
			character = gdk_keyval_to_unicode( kevent->keyval );
			if (character) {
				ch = g_strdup_printf("%c",  ((GdkEventKey*)kevent)->keyval);
			}else{
				ch = g_strdup("xchar");
			}
		}

		ctrl = kevent->state & GDK_CONTROL_MASK ? "<control>": "";
		shift = kevent->state & GDK_SHIFT_MASK ? "<shift>": "";
		mod1 = kevent->state & GDK_MOD1_MASK ? "<mod1>": "";

		tmpstr = g_strdup_printf("%s%s%s%s%s%s%s", before, ctrl, shift, mod1, ch, str, newline?"\n":"");
		g_free(ch);
	}else{
		tmpstr = g_strdup_printf("! sequence cancelled. %s%s", str, newline?"\n":"");	
	}
	DEBUG_MSG(tmpstr);
	g_free(tmpstr);
}

static gboolean snooper_loopkup_keyseq(GdkEventKey *kevent1, GdkEventKey *kevent2) {
	gchar *tmpstr;
	const gchar *ctrl, *shift, *mod1;
	const gchar *ctrl2, *shift2, *mod12;
	guint32 ch, ch2;
	Tsnooper *snooper;
	gchar *value;
	gboolean retval;
	
	DEBUG_MSG("snooper_loopkup_keyseq: started\n");

	ch = gdk_keyval_to_unicode( kevent1->keyval );
	ctrl = kevent1->state & GDK_CONTROL_MASK ? "<control>": "";
	shift = kevent1->state & GDK_SHIFT_MASK ? "<shift>": "";
	mod1 = kevent1->state & GDK_MOD1_MASK ? "<mod1>": "";

	if (kevent2) {
		ch2 = gdk_keyval_to_unicode( kevent2->keyval );
		ctrl2 = kevent2->state & GDK_CONTROL_MASK ? "<control>": "";
		shift2 = kevent2->state & GDK_SHIFT_MASK ? "<shift>": "";
		mod12 = kevent2->state & GDK_MOD1_MASK ? "<mod1>": "";
		/* TODO: use (gtk_accelerator_name) */
		tmpstr = g_strdup_printf("%s%s%s%c%s%s%s%c",ctrl,shift,mod1,ch,ctrl2,shift2,mod12,ch2);
		DEBUG_MSG("snooper_loopkup_keyseq: lookup (double) '%s'\n", tmpstr);
	}else{
		tmpstr = g_strdup_printf("%s%s%s%c",ctrl,shift,mod1,ch);
		DEBUG_MSG("snooper_loopkup_keyseq: lookup (single) '%s'\n", tmpstr);
	}

	snooper = SNOOPER(main_v->snooper);
	retval = FALSE;
	value = g_hash_table_lookup(snooper->key_hashtable, tmpstr);
	if (value) {
		gpointer value_;
		value_ = g_hash_table_lookup(snooper->func_hashtable,value);
		if (value_) {
			retval = TRUE;
			snooper->todo = value_;
		}
	}

	g_free(tmpstr);
	DEBUG_MSG("snooper_loopkup_keyseq: finished with retval = %d\n", retval);
	return retval;
}

static gboolean snooper_accel_group_find_func(GtkAccelKey *key, GClosure *closure, gpointer data) {
	GdkEventKey *test;
	test = (GdkEventKey*)data;
	return ( (key->accel_key == test->keyval) && (key->accel_mods == test->state) );
}

/* gtk_accel_groups_activate(GOBJECT(main_v->window), kevent->keyval, kevent->state); */

static gboolean snooper_loopkup_keys_in_accel_map(GdkEventKey *kevent) {
	GtkAccelKey *accel_key;
	gboolean retval;
	
	retval = FALSE;
	/* list of accel. groups attached to main_v->window */
	accel_key = gtk_accel_group_find( main_v->accel_group, (GtkAccelGroupFindFunc) snooper_accel_group_find_func, kevent);
	if (accel_key) {
		retval = TRUE;
	}
	DEBUG_MSG("snooper_loopkup_keys_in_accel_map: returns %d\n", retval);
	return retval;
}

static gint main_snooper (GtkWidget *widget, GdkEventKey *kevent, gpointer data) {
	Tsnooper *snooper =  SNOOPER(main_v->snooper);
	if ( snooper->stat == SNOOPER_TODO ) {
		snooper->stat = 0;
		return TRUE;
	}
	if (kevent->type == GDK_KEY_PRESS) {
		if ( snooper->stat && ( kevent->keyval == GDK_Escape ) ) {
			report(NULL, TRUE, "", "");
			snooper->stat = SNOOPER_CANCEL_RELEASE_EVENT;
			return TRUE;
		}else if ( (snooper->stat == SNOOPER_HALF_SEQ) || SNOOPER_IS_KEYSEQ(kevent) ) {
			if ( snooper->stat == SNOOPER_HALF_SEQ ) {
				report( (GdkEventKey*) snooper -> last_event , FALSE , "", "");
				report( kevent, TRUE ,"", "" );
				if (snooper_loopkup_keyseq((GdkEventKey*) snooper -> last_event, kevent)) {
					snooper->stat = SNOOPER_TODO;
				}else{
					snooper->stat = SNOOPER_CANCEL_RELEASE_EVENT;
					return TRUE;
				}
			} else if (snooper->stat == 0) {
				*( (GdkEventKey*) snooper->last_event )= *kevent;
				if (snooper_loopkup_keyseq(kevent, NULL) ) {
					snooper->stat = SNOOPER_TODO;
				}else{
					if (snooper_loopkup_keys_in_accel_map(kevent)) {
						snooper->stat = SNOOPER_CANCEL_RELEASE_EVENT;
						return FALSE;
					}else{
						snooper->stat = SNOOPER_HALF_SEQ;
						return TRUE;
					}
				}
			}
		}else{
			snooper->stat = 0;
			/* other keys, treat normal */
			return FALSE;
		}
	}else{
		if ( (snooper->stat == SNOOPER_CANCEL_RELEASE_EVENT) || (snooper->stat ==  SNOOPER_HALF_SEQ) ) {
			return TRUE;
		}
	}
	return FALSE;
}

void snooper_install() {
	Tsnooper *snooper =  SNOOPER(main_v->snooper);
	snooper->id = gtk_key_snooper_install( (GtkKeySnoopFunc) main_snooper, NULL);
	snooper->last_event = gdk_event_new(GDK_KEY_PRESS);
}

#endif /* SNOOPER2 */
