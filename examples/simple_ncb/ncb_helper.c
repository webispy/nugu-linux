#include <stdio.h>
#include <dlfcn.h>

#include "ncb_helper.h"

#define ARGS(...) __VA_ARGS__

// Binding function declaration with return value
#define DECL_RETN(_type, name, args_decl, args_param, fail_ret)                \
	static _type (*_##name)(args_decl);                                    \
	_type name(args_decl)                                                  \
	{                                                                      \
		if (!_##name) {                                                \
			_##name = dlsym(handle, #name);                        \
			if (!_##name) {                                        \
				printf("dlsym failed: '" #name "'\n");         \
				fail_ret;                                      \
			}                                                      \
		}                                                              \
		return _##name(args_param);                                    \
	}

// Binding function declaration without return value
#define DECL_VOID(name, args_decl, args_param)                                 \
	static void (*_##name)(args_decl);                                     \
	void name(args_decl)                                                   \
	{                                                                      \
		if (!_##name) {                                                \
			_##name = dlsym(handle, #name);                        \
			if (!_##name) {                                        \
				printf("dlsym failed: '" #name "'\n");         \
				return;                                        \
			}                                                      \
		}                                                              \
		_##name(args_param);                                           \
	}

static void *handle;

// Client-kit
DECL_RETN(int, ncb_init, , , return -1)
DECL_RETN(int, ncb_deinit, , , return -1)
DECL_RETN(int, ncb_set_token, ARGS(const char *token), ARGS(token), return -1)
DECL_RETN(int, ncb_connect, , , return -1)
DECL_RETN(int, ncb_text_send, ARGS(const char *text), ARGS(text), return -1)
DECL_RETN(int, ncb_set_network_callback,
	  ARGS(ncb_type_network_callback callback), ARGS(callback), return -1)
DECL_RETN(int, ncb_set_network_error_callback,
	  ARGS(ncb_type_network_error_callback callback), ARGS(callback),
	  return -1)
DECL_RETN(int, ncb_start_listening_with_wakeup, , , return -1)
DECL_RETN(int, ncb_stop_listening, , , return -1)
DECL_RETN(int, ncb_start_listening, , , return -1)

// PCM
DECL_VOID(nugu_pcm_emit_status,
	  ARGS(NuguPcm *pcm, enum nugu_media_status status), ARGS(pcm, status))
DECL_VOID(nugu_pcm_emit_event, ARGS(NuguPcm *pcm, enum nugu_media_event event),
	  ARGS(pcm, event))
DECL_RETN(int, nugu_pcm_set_driver_data, ARGS(NuguPcm *pcm, void *data),
	  ARGS(pcm, data), return -1)
DECL_RETN(void *, nugu_pcm_get_driver_data, ARGS(NuguPcm *pcm), ARGS(pcm), NULL)

DECL_RETN(NuguPcmDriver *, nugu_pcm_driver_new,
	  ARGS(const char *name, struct nugu_pcm_driver_ops *ops),
	  ARGS(name, ops), NULL)
DECL_RETN(int, nugu_pcm_driver_free, ARGS(NuguPcmDriver *driver), ARGS(driver),
	  return -1)
DECL_RETN(int, nugu_pcm_driver_register, ARGS(NuguPcmDriver *driver),
	  ARGS(driver), return -1)
DECL_RETN(int, nugu_pcm_driver_remove, ARGS(NuguPcmDriver *driver),
	  ARGS(driver), return -1)
DECL_RETN(int, nugu_pcm_driver_set_default, ARGS(NuguPcmDriver *driver),
	  ARGS(driver), return -1)

// Player
DECL_VOID(nugu_player_emit_status,
	  ARGS(NuguPlayer *player, enum nugu_media_status status),
	  ARGS(player, status))
DECL_VOID(nugu_player_emit_event,
	  ARGS(NuguPlayer *player, enum nugu_media_event event),
	  ARGS(player, event))
DECL_RETN(int, nugu_player_set_driver_data,
	  ARGS(NuguPlayer *player, void *data), ARGS(player, data), return -1)
DECL_RETN(void *, nugu_player_get_driver_data, ARGS(NuguPlayer *player),
	  ARGS(player), NULL)

DECL_RETN(NuguPlayerDriver *, nugu_player_driver_new,
	  ARGS(const char *name, struct nugu_player_driver_ops *ops),
	  ARGS(name, ops), NULL)
DECL_RETN(int, nugu_player_driver_free, ARGS(NuguPlayerDriver *driver),
	  ARGS(driver), return -1)
DECL_RETN(int, nugu_player_driver_register, ARGS(NuguPlayerDriver *driver),
	  ARGS(driver), return -1)
DECL_RETN(int, nugu_player_driver_remove, ARGS(NuguPlayerDriver *driver),
	  ARGS(driver), return -1)
DECL_RETN(int, nugu_player_driver_set_default, ARGS(NuguPlayerDriver *driver),
	  ARGS(driver), return -1)

// Recorder
DECL_RETN(int, nugu_recorder_set_driver_data,
	  ARGS(NuguRecorder *rec, void *data), ARGS(rec, data), return -1)
DECL_RETN(void *, nugu_recorder_get_driver_data, ARGS(NuguRecorder *rec),
	  ARGS(rec), NULL)

DECL_RETN(int, nugu_recorder_set_frame_size,
	  ARGS(NuguRecorder *rec, int size, int max), ARGS(rec, size, max),
	  return -1)
DECL_RETN(int, nugu_recorder_push_frame,
	  ARGS(NuguRecorder *rec, const char *data, int size),
	  ARGS(rec, data, size), return -1)

DECL_RETN(NuguRecorderDriver *, nugu_recorder_driver_new,
	  ARGS(const char *name, struct nugu_recorder_driver_ops *ops),
	  ARGS(name, ops), NULL)
DECL_RETN(int, nugu_recorder_driver_free, ARGS(NuguRecorderDriver *driver),
	  ARGS(driver), return -1)
DECL_RETN(int, nugu_recorder_driver_register, ARGS(NuguRecorderDriver *driver),
	  ARGS(driver), return -1)
DECL_RETN(int, nugu_recorder_driver_remove, ARGS(NuguRecorderDriver *driver),
	  ARGS(driver), return -1)
DECL_RETN(int, nugu_recorder_driver_set_default,
	  ARGS(NuguRecorderDriver *driver), ARGS(driver), return -1)

int ncb_load_library(const char *path)
{
	handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
		printf("dlopen failed: '%s' plugin. %s\n", path, dlerror());
		return -1;
	}

	printf("ncb_load_library success\n");

	return 0;
}

void ncb_unload_library()
{
	if (!handle)
		return;

	dlclose(handle);
	handle = NULL;

	printf("ncb_unload_library success\n");
}

void *ncb_get_library_handle()
{
	return handle;
}
