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
#include <opus.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_encoder.h"
#include "base/nugu_pcm.h"

#define FILENAME "/tmp/test.ogg"

struct opus_data {
	NuguBuffer *buf;
	int fd;
	unsigned char *data;
	size_t pos;
	size_t filesize;
};

static NuguEncoderDriver *enc_driver;

int _encoder_create(NuguEncoderDriver *driver, NuguEncoder *enc,
		    NuguAudioProperty property)
{
	int sample_rate;
	struct opus_data *od;

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

	od->buf = nugu_buffer_new(4096);
	od->fd = open(FILENAME, O_RDONLY);
	od->filesize = lseek(od->fd, 0, SEEK_END);
	lseek(od->fd, 0, SEEK_SET);
	od->data = malloc(od->filesize);
	od->pos = 0;

	{
		ssize_t nread;

		nread = read(od->fd, od->data, od->filesize);
		if (nread < 0) {
			nugu_error("read() failed");
			return -1;
		}
	}

	nugu_encoder_set_driver_data(enc, od);

	nugu_dbg("new opus %d encoder created", sample_rate);

	return 0;
}

int encode(struct opus_data *od)
{
	size_t pos;
	ssize_t base_pos, end_pos;
	int step;

	nugu_info("filename: %s, size: %zd", FILENAME, od->filesize);

	pos = od->pos;
	step = 0;
	end_pos = -1;
	base_pos = -1;

	nugu_dbg("parsing start: pos = %d, size = %d", pos, od->filesize);
	while (pos < od->filesize) {
		switch (step) {
		case 0:
			if (od->data[pos] == 'O') {
				//				nugu_dbg("step0: found 'O' from %d", pos);
				step = 1;
			} else
				step = 0;
			break;
		case 1:
			if (od->data[pos] == 'g') {
				//				nugu_dbg("step1: found 'Og' from %d", pos);
				step = 2;
			} else
				step = 0;
			break;
		case 2:
			if (od->data[pos] == 'g') {
				//				nugu_dbg("step2: found 'Ogg' from %d", pos);
				step = 3;
			} else
				step = 0;
			break;
		case 3:
			if (od->data[pos] == 'S') {
				//				nugu_dbg("step3: found 'OggS' from %d", pos);
				//				nugu_error("found 'OggS' from %d (base_pos=%d)",
				//					   pos, base_pos);
				if (base_pos != -1) {
					end_pos = pos - 4;
					step = 99;
					break;
				}
				base_pos = pos - 3;
			}
			step = 0;
			break;
		}

		if (step == 99) {
			nugu_error("done");
			break;
		}

		pos++;
	}

	nugu_info("base_pos = %d (0x%X), end_pos = %d (0x%X)", base_pos,
		  base_pos, end_pos, end_pos);

	if (base_pos >= 0) {
		if (end_pos < 0)
			end_pos = od->filesize - 1;

		nugu_error(" - data[%d] = 0x%X", base_pos, od->data[base_pos]);
		nugu_error(" - data[%d] = 0x%X", end_pos, od->data[end_pos]);

		nugu_buffer_add(od->buf, od->data + base_pos,
				end_pos - base_pos);
		od->pos = end_pos + 1;
	} else {
		od->pos = pos;
	}

	return base_pos;
}

int _encoder_encode(NuguEncoderDriver *driver, NuguEncoder *enc,
		    const void *data, size_t data_len, NuguBuffer *out_buf)
{
	struct opus_data *od;

	od = nugu_encoder_get_driver_data(enc);

	if (encode(od) < 0) {
		nugu_error("no more data");
		return 0;
	}

	nugu_error("buffer size: %zd", nugu_buffer_get_size(od->buf));

#if 0
	nugu_hexdump(NUGU_LOG_MODULE_DEFAULT, nugu_buffer_peek(od->buf),
		     nugu_buffer_get_size(od->buf), "", "", "");
#endif
	nugu_buffer_add(out_buf, nugu_buffer_peek(od->buf),
			nugu_buffer_get_size(od->buf));

	nugu_error("result buf size: %zd", nugu_buffer_get_size(out_buf));

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

	if (od->data)
		free(od->data);

	if (od->fd)
		close(od->fd);

	nugu_buffer_free(od->buf, 1);
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
		nugu_encoder_driver_remove(enc_driver);
		nugu_encoder_driver_free(enc_driver);
		enc_driver = NULL;
	}
}

NUGU_PLUGIN_DEFINE("opusenc", NUGU_PLUGIN_PRIORITY_DEFAULT, "0.0.1", load,
		   unload, init);
