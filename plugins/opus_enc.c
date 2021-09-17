/*
 * Copyright (c) 2019 SK Telecom Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>
#include <opusenc.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_encoder.h"
#include "base/nugu_pcm.h"

#define SAMPLING_RATES 16000
#define CHANNELS 1
#define PCM_SAMPLES 480

/*The frame size is hardcoded for this sample code but it doesn't have to be*/
#define FRAME_SIZE 640 /* 40, 80, 160, 320, 640, 960 */
#define MAX_FRAME_SIZE (16 * FRAME_SIZE)

struct opus_data {
	OggOpusEnc *enc_handle;
	OggOpusComments *comments;
	NuguBuffer *buf;

	void *enc_buffer;
	size_t enc_buffer_size;
	//	opus_int16 *pcm_buf;
	//	size_t pcm_buf_size;
	int dump_fd;
	int dump_src_fd;
};

static NuguEncoderDriver *enc_driver;
static OpusEncCallbacks callbacks;

static int _dumpfile_open(const char *path, const char *prefix)
{
	char ymd[32];
	char hms[32];
	time_t now;
	struct tm now_tm;
	char *buf = NULL;
	int fd;

	if (!path)
		return -1;

	now = time(NULL);
	localtime_r(&now, &now_tm);

	snprintf(ymd, sizeof(ymd), "%04d%02d%02d", now_tm.tm_year + 1900,
		 now_tm.tm_mon + 1, now_tm.tm_mday);
	snprintf(hms, sizeof(hms), "%02d%02d%02d", now_tm.tm_hour,
		 now_tm.tm_min, now_tm.tm_sec);

	buf = g_strdup_printf("%s/%s_%s_%s.dat", path, prefix, ymd, hms);

	fd = open(buf, O_CREAT | O_WRONLY, 0644);
	if (fd < 0)
		nugu_error("open(%s) failed: %s", buf, strerror(errno));

	nugu_dbg("%s filedump to '%s' (fd=%d)", prefix, buf, fd);

	free(buf);

	return fd;
}

void packet_callback(void *user_data, const unsigned char *packet_ptr,
		     opus_int32 packet_len, opus_uint32 flags)
{
	struct opus_data *od = user_data;

	nugu_dbg("packet_callback: %d", packet_len);
	nugu_buffer_add(od->buf, packet_ptr, packet_len);
}

int write_func(void *user_data, const unsigned char *ptr, opus_int32 len)
{
	nugu_error("write_func: %d", len);
	return 0;
}

int close_func(void *user_data)
{
	nugu_error("close_func");
	return 0;
}

int _encoder_create(NuguEncoderDriver *driver, NuguEncoder *enc,
		    NuguAudioProperty property)
{
	int sample_rate;
	struct opus_data *od;
	int err = 0;

	g_return_val_if_fail(driver != NULL, -1);
	g_return_val_if_fail(enc != NULL, -1);

	switch (property.samplerate) {
	case NUGU_AUDIO_SAMPLE_RATE_8K:
		sample_rate = 8000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_16K:
		sample_rate = 16000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_32K:
		sample_rate = 32000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_22K:
		sample_rate = 22050;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_44K:
		sample_rate = 44100;
		break;
	default:
		nugu_error("not supported sample-rate type(%d)",
			   property.samplerate);
		return -1;
	}

	od = malloc(sizeof(struct opus_data));
	if (!od) {
		nugu_error_nomem();
		return -1;
	}

	od->enc_buffer = NULL;
	od->enc_buffer_size = 0;
	od->buf = nugu_buffer_new(4096);
	od->comments = ope_comments_create();

	od->enc_handle =
		ope_encoder_create_callbacks(&callbacks, od, od->comments,
					     sample_rate, property.channel, 0,
					     &err);
	ope_encoder_ctl(od->enc_handle,
			OPE_SET_PACKET_CALLBACK(packet_callback, od));
	//	ope_encoder_flush_header(od->enc_handle);

	if (err != OPUS_OK) {
		nugu_error("opus_decoder_create() failed. (%d)", err);
		free(od);
		return -1;
	}

#ifdef NUGU_ENV_DUMP_PATH_ENCODER
	od->dump_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_ENCODER), "opus");
	od->dump_src_fd =
		_dumpfile_open(getenv(NUGU_ENV_DUMP_PATH_ENCODER), "pcm");
#else
	od->dump_fd = -1;
	od->dump_src_fd = -1;
#endif

	nugu_encoder_set_driver_data(enc, od);

	nugu_dbg("new opus %d encoder created", sample_rate);

	return 0;
}

int do_encode(struct opus_data *od, opus_int16 *buf, size_t buf_len)
{
	int nbBytes;

	if (od->dump_src_fd != -1) {
		if (write(od->dump_src_fd, buf, buf_len * 2) < 0)
			nugu_error("write to fd-%d failed", od->dump_src_fd);
	}

	nbBytes = ope_encoder_write(od->enc_handle, buf, buf_len);
	if (nbBytes < 0) {
		nugu_error("ope_encoder_write() failed: %s",
			   ope_strerror(nbBytes));
		return -1;
	}

	nugu_dbg("ope_encoder_write() = %d", nbBytes);

	return 0;
}

int _encoder_encode(NuguEncoderDriver *driver, NuguEncoder *enc,
		    const void *data, size_t data_len, NuguBuffer *out_buf)
{
	struct opus_data *od;
	const unsigned char *pcm_data = data;
	size_t i;
	const unsigned char *pos;
	opus_int16 pcm_buf[FRAME_SIZE];

	od = nugu_encoder_get_driver_data(enc);

	nugu_info("data_len: %zd, samples: %zd", data_len, data_len / 2);

	pos = pcm_data;
	while (pos < pcm_data + data_len) {
		for (i = 0; i < FRAME_SIZE; i++) {
			pcm_buf[i] = (0xFF & pos[2 * i + 1]) << 8 |
				     (0xFF & pos[2 * i]);
		}

		if (pcm_buf[0] == 0)
			pcm_buf[0] = 1;

		nugu_info("fill pcm_data[%d] to pcm_data[%d], FRAME_SIZE=%d",
			  (pos - pcm_data), pos - pcm_data + i * 2, FRAME_SIZE);

		//		do_encode(od, pcm_buf, FRAME_SIZE);
		do_encode(od, (opus_int16 *)pos, FRAME_SIZE);

		pos += (FRAME_SIZE * 2);
	}

	nugu_info("-> encode result: %d bytes", nugu_buffer_get_size(od->buf));

	if (od->dump_fd != -1) {
		if (write(od->dump_fd, nugu_buffer_peek(od->buf),
			  nugu_buffer_get_size(od->buf)) < 0)
			nugu_error("write to fd-%d failed", od->dump_fd);
	}

	//nugu_buffer_add(out_buf, nugu_buffer_peek(od->buf),
	//		nugu_buffer_get_size(od->buf));

	//	nugu_hexdump(NUGU_LOG_MODULE_DEFAULT, nugu_buffer_peek(od->buf),
	//		     nugu_buffer_get_size(od->buf), "", "", "");

	nugu_buffer_clear(od->buf);

	return 0;
}

int _encoder_destroy(NuguEncoderDriver *driver, NuguEncoder *enc)
{
	struct opus_data *od;

	od = nugu_encoder_get_driver_data(enc);
	if (!od) {
		nugu_error("internal error");
		return -1;
	}

	if (od->dump_fd >= 0) {
		close(od->dump_fd);
		od->dump_fd = -1;
	}

	if (od->dump_src_fd >= 0) {
		close(od->dump_src_fd);
		od->dump_src_fd = -1;
	}

	ope_comments_destroy(od->comments);
	ope_encoder_destroy(od->enc_handle);

	free(od);
	nugu_encoder_set_driver_data(enc, NULL);

	nugu_dbg("opus encoder destroyed");

	return 0;
}

static struct nugu_encoder_driver_ops encoder_ops = {
	.create = _encoder_create,
	.encode = _encoder_encode,
	.destroy = _encoder_destroy
};

static int init(NuguPlugin *p)
{
	nugu_dbg("plugin-init '%s'", nugu_plugin_get_description(p)->name);

	enc_driver = nugu_encoder_driver_new("opus", NUGU_ENCODER_TYPE_OPUS,
					     &encoder_ops);
	if (!enc_driver)
		return -1;

#if 0
	if (nugu_encoder_driver_register(enc_driver) < 0) {
		nugu_encoder_driver_free(enc_driver);
		enc_driver = NULL;
		return -1;
	}
#endif

	callbacks.write = write_func;
	callbacks.close = close_func;

	return 0;
}

static int load(void)
{
	nugu_dbg("plugin-load");

	return 0;
}

static void unload(NuguPlugin *p)
{
	nugu_dbg("plugin-unload '%s'", nugu_plugin_get_description(p)->name);

	if (enc_driver) {
//		nugu_encoder_driver_remove(enc_driver);
		nugu_encoder_driver_free(enc_driver);
		enc_driver = NULL;
	}
}

NUGU_PLUGIN_DEFINE("opusenc", NUGU_PLUGIN_PRIORITY_DEFAULT, "0.0.1", load,
		   unload, init);
