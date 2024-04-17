#ifndef __NCB_HELPER_H__
#define __NCB_HELPER_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ncb_type_network_callback)(int status);
typedef void (*ncb_type_network_error_callback)(int error_code);

int ncb_init();
int ncb_deinit();
int ncb_set_token(const char *token);
int ncb_connect();
int ncb_text_send(const char *text);
int ncb_set_network_callback(ncb_type_network_callback callback);
int ncb_set_network_error_callback(ncb_type_network_error_callback callback);
int ncb_start_listening_with_wakeup();
int ncb_stop_listening();
int ncb_start_listening();

int ncb_load_library(const char *path);
void ncb_unload_library();

void *ncb_get_library_handle();

/* Media */
enum nugu_media_status {
	NUGU_MEDIA_STATUS_STOPPED, /**< media stopped */
	NUGU_MEDIA_STATUS_READY, /**< media ready */
	NUGU_MEDIA_STATUS_PLAYING, /**< media playing */
	NUGU_MEDIA_STATUS_PAUSED /**< media paused */
};

enum nugu_media_event {
	NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED, /**< media source changed */
	NUGU_MEDIA_EVENT_MEDIA_INVALID, /**< media invalid */
	NUGU_MEDIA_EVENT_MEDIA_LOAD_FAILED, /**< media load failed */
	NUGU_MEDIA_EVENT_MEDIA_LOADED, /**< media loaded */
	NUGU_MEDIA_EVENT_MEDIA_UNDERRUN, /**< media buffer underrun */
	NUGU_MEDIA_EVENT_MEDIA_BUFFER_FULL, /**< media buffer full */
	NUGU_MEDIA_EVENT_END_OF_STREAM, /**< end of stream */
	NUGU_MEDIA_EVENT_MAX
};

/* PCM Driver */
typedef struct _nugu_pcm_driver NuguPcmDriver;
typedef struct _nugu_pcm NuguPcm;

enum nugu_audio_sample_rate {
	NUGU_AUDIO_SAMPLE_RATE_8K, /**< 8K */
	NUGU_AUDIO_SAMPLE_RATE_16K, /**< 16K */
	NUGU_AUDIO_SAMPLE_RATE_32K, /**< 32K */
	NUGU_AUDIO_SAMPLE_RATE_22K, /**< 22K */
	NUGU_AUDIO_SAMPLE_RATE_44K, /**< 44K */
	NUGU_AUDIO_SAMPLE_RATE_MAX
};

enum nugu_audio_format {
	NUGU_AUDIO_FORMAT_S8, /**< Signed 8 bits */
	NUGU_AUDIO_FORMAT_U8, /**< Unsigned 8 bits */
	NUGU_AUDIO_FORMAT_S16_LE, /**< Signed 16 bits little endian */
	NUGU_AUDIO_FORMAT_S16_BE, /**< Signed 16 bits big endian */
	NUGU_AUDIO_FORMAT_U16_LE, /**< Unsigned 16 bits little endian */
	NUGU_AUDIO_FORMAT_U16_BE, /**< Unsigned 16 bits big endian */
	NUGU_AUDIO_FORMAT_S24_LE, /**< Signed 24 bits little endian */
	NUGU_AUDIO_FORMAT_S24_BE, /**< Signed 24 bits big endian */
	NUGU_AUDIO_FORMAT_U24_LE, /**< Unsigned 24 bits little endian */
	NUGU_AUDIO_FORMAT_U24_BE, /**< Unsigned 24 bits big endian */
	NUGU_AUDIO_FORMAT_S32_LE, /**< Signed 32 bits little endian */
	NUGU_AUDIO_FORMAT_S32_BE, /**< Signed 32 bits big endian */
	NUGU_AUDIO_FORMAT_U32_LE, /**< Unsigned 32 bits little endian */
	NUGU_AUDIO_FORMAT_U32_BE, /**< Unsigned 32 bits big endian */
	NUGU_AUDIO_FORMAT_MAX
};

struct nugu_audio_property {
	enum nugu_audio_sample_rate samplerate;
	enum nugu_audio_format format;
	int channel;
};
typedef struct nugu_audio_property NuguAudioProperty;

struct nugu_pcm_driver_ops {
	/**
	 * @brief Called when pcm is created
	 * @see nugu_pcm_new()
	 */
	int (*create)(NuguPcmDriver *driver, NuguPcm *pcm,
		      NuguAudioProperty property);

	/**
	 * @brief Called when pcm is destroyed
	 * @see nugu_pcm_free()
	 */
	void (*destroy)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief Called when pcm is started
	 * @see nugu_pcm_start()
	 */
	int (*start)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief Called when a pcm data is pushed to pcm object
	 * @see nugu_pcm_push_data()
	 */
	int (*push_data)(NuguPcmDriver *driver, NuguPcm *pcm, const char *data,
			 size_t size, int is_last);

	/**
	 * @brief Called when pcm is stopped
	 * @see nugu_pcm_stop()
	 */
	int (*stop)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief Called when pcm is paused
	 * @see nugu_pcm_pause()
	 */
	int (*pause)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief called when pcm is resumed
	 * @see nugu_pcm_resume()
	 */
	int (*resume)(NuguPcmDriver *driver, NuguPcm *pcm);

	/**
	 * @brief called when pcm is needed to set volume
	 * @see nugu_pcm_resume()
	 */
	int (*set_volume)(NuguPcmDriver *driver, NuguPcm *pcm, int volume);

	/**
	 * @brief Called when a playback position is requested.
	 * @see nugu_pcm_get_position()
	 */
	int (*get_position)(NuguPcmDriver *driver, NuguPcm *pcm);
};

void nugu_pcm_emit_status(NuguPcm *pcm, enum nugu_media_status status);
void nugu_pcm_emit_event(NuguPcm *pcm, enum nugu_media_event event);
int nugu_pcm_set_driver_data(NuguPcm *pcm, void *data);
void *nugu_pcm_get_driver_data(NuguPcm *pcm);

NuguPcmDriver *nugu_pcm_driver_new(const char *name,
				   struct nugu_pcm_driver_ops *ops);
int nugu_pcm_driver_free(NuguPcmDriver *driver);
int nugu_pcm_driver_register(NuguPcmDriver *driver);

int nugu_pcm_driver_remove(NuguPcmDriver *driver);
int nugu_pcm_driver_set_default(NuguPcmDriver *driver);

/* Player Driver */
typedef struct _nugu_player NuguPlayer;
typedef struct _nugu_player_driver NuguPlayerDriver;

struct nugu_player_driver_ops {
	/**
	 * @brief Called when player is created
	 * @see nugu_player_new()
	 */
	int (*create)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when player is destroyed
	 * @see nugu_player_free()
	 */
	void (*destroy)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when set the player source
	 * @see nugu_player_set_source()
	 */
	int (*set_source)(NuguPlayerDriver *driver, NuguPlayer *player,
			  const char *url);

	/**
	 * @brief Called when player is started
	 * @see nugu_player_start()
	 */
	int (*start)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when player is stopped
	 * @see nugu_player_stop()
	 */
	int (*stop)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when player is paused
	 * @see nugu_player_pause()
	 */
	int (*pause)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when player is resumed
	 * @see nugu_player_resume()
	 */
	int (*resume)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when playback position is changed by seek
	 * @see nugu_player_seek()
	 */
	int (*seek)(NuguPlayerDriver *driver, NuguPlayer *player, int sec);

	/**
	 * @brief Called when volume is changed
	 * @see nugu_player_set_volume()
	 */
	int (*set_volume)(NuguPlayerDriver *driver, NuguPlayer *player,
			  int vol);

	/**
	 * @brief Called when a playback duration is requested.
	 * @see nugu_player_get_duration()
	 */
	int (*get_duration)(NuguPlayerDriver *driver, NuguPlayer *player);

	/**
	 * @brief Called when a playback position is requested.
	 * @see nugu_player_get_position()
	 */
	int (*get_position)(NuguPlayerDriver *driver, NuguPlayer *player);
};

void nugu_player_emit_status(NuguPlayer *player, enum nugu_media_status status);
void nugu_player_emit_event(NuguPlayer *player, enum nugu_media_event event);
int nugu_player_set_driver_data(NuguPlayer *player, void *data);
void *nugu_player_get_driver_data(NuguPlayer *player);

NuguPlayerDriver *nugu_player_driver_new(const char *name,
					 struct nugu_player_driver_ops *ops);
int nugu_player_driver_free(NuguPlayerDriver *driver);
int nugu_player_driver_register(NuguPlayerDriver *driver);
int nugu_player_driver_remove(NuguPlayerDriver *driver);
int nugu_player_driver_set_default(NuguPlayerDriver *driver);

/* Recorder Driver */
typedef struct _nugu_recorder NuguRecorder;
typedef struct _nugu_recorder_driver NuguRecorderDriver;

struct nugu_recorder_driver_ops {
	/**
	 * @brief Called when recording is started
	 * @see nugu_recorder_start()
	 */
	int (*start)(NuguRecorderDriver *driver, NuguRecorder *rec,
		     NuguAudioProperty property);

	/**
	 * @brief Called when recording is stopped
	 * @see nugu_recorder_stop()
	 */
	int (*stop)(NuguRecorderDriver *driver, NuguRecorder *rec);
};

int nugu_recorder_set_driver_data(NuguRecorder *rec, void *data);
void *nugu_recorder_get_driver_data(NuguRecorder *rec);

int nugu_recorder_set_frame_size(NuguRecorder *rec, int size, int max);
int nugu_recorder_push_frame(NuguRecorder *rec, const char *data, int size);

NuguRecorderDriver *
nugu_recorder_driver_new(const char *name,
			 struct nugu_recorder_driver_ops *ops);
int nugu_recorder_driver_free(NuguRecorderDriver *driver);
int nugu_recorder_driver_register(NuguRecorderDriver *driver);
int nugu_recorder_driver_remove(NuguRecorderDriver *driver);
int nugu_recorder_driver_set_default(NuguRecorderDriver *driver);

#ifdef __cplusplus
}
#endif

#endif
