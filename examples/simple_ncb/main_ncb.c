#include "fake_recorder.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>

#include "ncb_helper.h"
#include "fake_pcm.h"
#include "fake_player.h"

static guint src_id;

void on_network_callback(int status)
{
	printf("network_callback: %d\n", status);
}

void on_network_error_callback(int error_code)
{
	printf("network_error_callback: %d\n", error_code);
}

static gboolean on_stdin(GIOChannel *src, GIOCondition con, gpointer userdata)
{
	char keybuf[4096];

	if (fgets(keybuf, 4096, stdin) == NULL)
		return TRUE;

	if (strlen(keybuf) > 0 && keybuf[strlen(keybuf) - 1] == '\n')
		keybuf[strlen(keybuf) - 1] = '\0';

	if (g_strcmp0(keybuf, "q") == 0) {
		printf("Exit the program...\n");
		g_main_loop_quit(userdata);
		return FALSE;
	} else if (g_strcmp0(keybuf, "t") == 0) {
		ncb_text_send("클래식");
	} else if (g_strcmp0(keybuf, "w") == 0) {
		ncb_start_listening_with_wakeup();
	} else if (g_strcmp0(keybuf, "o") == 0) {
		ncb_stop_listening();
	} else if (g_strcmp0(keybuf, "l") == 0) {
		ncb_start_listening();
	}

	return TRUE;
}

int main(int argc, char *argv[])
{
	const char *env_token;
	GMainLoop *loop;
	GIOChannel *channel;
	GSource *src;

	if (argc != 2) {
		printf("usage: %s <libnugu library path>\n", argv[0]);
		return -1;
	}

	env_token = getenv("NUGU_TOKEN");
	if (env_token == NULL) {
		printf("Please set the token using the NUGU_TOKEN environment variable.\n");
		return -1;
	}

	loop = g_main_loop_new(NULL, FALSE);

	/* keyboard handler */
	channel = g_io_channel_unix_new(STDIN_FILENO);
	src = g_io_create_watch(channel,
				(GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP));
	g_source_set_priority(src, G_PRIORITY_DEFAULT);
	g_source_set_callback(src, (GSourceFunc)on_stdin, loop, NULL);
	src_id = g_source_attach(src, g_main_context_default());
	g_source_unref(src);
	g_io_channel_unref(channel);

	/* ncb test */
	if (ncb_load_library(argv[1]) < 0) {
		ncb_unload_library();
		return -1;
	}

	ncb_set_network_callback(on_network_callback);
	ncb_set_network_error_callback(on_network_error_callback);

	register_pcm();
	register_player();
	register_recorder();

	if (ncb_init() < 0) {
		printf("ncb_init failed\n");
		ncb_unload_library();
		return -1;
	}

	if (ncb_set_token(env_token) < 0) {
		printf("ncb_set_token failed\n");
		ncb_unload_library();
		return -1;
	}

	if (ncb_connect() < 0) {
		printf("ncb_connect failed\n");
		ncb_unload_library();
		return -1;
	}

	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	ncb_deinit();

	unregister_recorder();
	unregister_pcm();
	unregister_player();

	ncb_unload_library();

	return 0;
}
