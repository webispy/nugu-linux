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

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_buffer.h"
#include "base/nugu_pcm.h"

struct _nugu_pcm_driver {
	char *name;
	struct nugu_pcm_driver_ops *ops;
	int ref_count;
};

struct _nugu_pcm {
	char *name;
	NuguPcmDriver *driver;
	void *driver_data;

	enum nugu_media_status status;
	enum nugu_audio_attribute attr;
	NuguAudioProperty property;
	NuguMediaEventCallback ecb;
	NuguMediaStatusCallback scb;
	void *eud; /* user data for event callback */
	void *sud; /* user data for status callback */
	NuguBuffer *buf;
	int is_last;
	int volume;
	size_t total_size;

	pthread_mutex_t mutex;
};

static GList *_pcms;
static GList *_pcm_drivers;
static NuguPcmDriver *_default_driver;

NuguPcmDriver *nugu_pcm_driver_new(const char *name,
				   struct nugu_pcm_driver_ops *ops)
{
	NuguPcmDriver *driver;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(ops != NULL, NULL);

	driver = g_malloc0(sizeof(struct _nugu_pcm_driver));
	if (!driver) {
		nugu_error_nomem();
		return NULL;
	}

	driver->name = g_strdup(name);
	driver->ops = ops;
	driver->ref_count = 0;

	return driver;
}

int nugu_pcm_driver_free(NuguPcmDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (driver->ref_count != 0) {
		nugu_error("pcm still using driver");
		return -1;
	}

	if (_default_driver == driver)
		_default_driver = NULL;

	g_free(driver->name);

	memset(driver, 0, sizeof(struct _nugu_pcm_driver));
	g_free(driver);

	return 0;
}

int nugu_pcm_driver_register(NuguPcmDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	if (nugu_pcm_driver_find(driver->name)) {
		nugu_error("'%s' pcm driver already exist.", driver->name);
		return -1;
	}

	_pcm_drivers = g_list_append(_pcm_drivers, driver);

	if (!_default_driver)
		nugu_pcm_driver_set_default(driver);

	return 0;
}

int nugu_pcm_driver_remove(NuguPcmDriver *driver)
{
	GList *l;

	g_return_val_if_fail(driver != NULL, -1);

	l = g_list_find(_pcm_drivers, driver);
	if (!l)
		return -1;

	if (_default_driver == driver)
		_default_driver = NULL;

	_pcm_drivers = g_list_remove_link(_pcm_drivers, l);

	if (_pcm_drivers && _default_driver == NULL)
		_default_driver = _pcm_drivers->data;

	return 0;
}

int nugu_pcm_driver_set_default(NuguPcmDriver *driver)
{
	g_return_val_if_fail(driver != NULL, -1);

	nugu_info("set default pcm driver: '%s'", driver->name);

	_default_driver = driver;

	return 0;
}

NuguPcmDriver *nugu_pcm_driver_get_default(void)
{
#ifdef NUGU_ENV_DEFAULT_PCM_DRIVER
	const char *tmp;

	tmp = getenv(NUGU_ENV_DEFAULT_PCM_DRIVER);
	if (tmp) {
		nugu_info("force use '%s' to default pcm driver", tmp);
		return nugu_pcm_driver_find(tmp);
	}
#endif

	return _default_driver;
}

NuguPcmDriver *nugu_pcm_driver_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _pcm_drivers;
	while (cur) {
		if (g_strcmp0(((NuguPcmDriver *)cur->data)->name, name) == 0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

NuguPcm *nugu_pcm_new(const char *name, NuguPcmDriver *driver,
		      NuguAudioProperty property)
{
	NuguPcm *pcm;

	g_return_val_if_fail(name != NULL, NULL);

	pcm = g_malloc0(sizeof(struct _nugu_pcm));
	if (!pcm) {
		nugu_error_nomem();
		return NULL;
	}

	pcm->name = g_strdup(name);
	pcm->buf = nugu_buffer_new(0);
	if (pcm->buf == NULL) {
		nugu_error("nugu_buffer_new() failed");
		g_free(pcm->name);
		g_free(pcm);
		return NULL;
	}

	pcm->driver = driver;
	pcm->is_last = 0;
	pcm->scb = NULL;
	pcm->ecb = NULL;
	pcm->sud = NULL;
	pcm->eud = NULL;
	pcm->driver_data = NULL;
	pcm->status = NUGU_MEDIA_STATUS_STOPPED;
	pcm->attr = NUGU_AUDIO_ATTRIBUTE_VOICE_COMMAND;
	pcm->volume = NUGU_SET_VOLUME_MAX;
	pcm->total_size = 0;
	memcpy(&pcm->property, &property, sizeof(NuguAudioProperty));

	pthread_mutex_init(&pcm->mutex, NULL);

	if (driver == NULL || driver->ops == NULL ||
	    driver->ops->create == NULL) {
		nugu_warn("Not supported");
		return pcm;
	}

	if (pcm->driver->ops->create(pcm->driver, pcm, pcm->property) < 0) {
		nugu_error("can't create nugu_pcm");
		g_free(pcm->name);
		nugu_buffer_free(pcm->buf, 1);
		g_free(pcm);
		return NULL;
	}

	pcm->driver->ref_count++;

	return pcm;
}

void nugu_pcm_free(NuguPcm *pcm)
{
	g_return_if_fail(pcm != NULL);

	if (pcm->driver) {
		pcm->driver->ref_count--;

		if (pcm->driver->ops && pcm->driver->ops->destroy)
			pcm->driver->ops->destroy(pcm->driver, pcm);
	}

	g_free(pcm->name);
	nugu_buffer_free(pcm->buf, 1);

	pthread_mutex_destroy(&pcm->mutex);

	memset(pcm, 0, sizeof(struct _nugu_pcm));
	g_free(pcm);
}

int nugu_pcm_add(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	if (nugu_pcm_find(pcm->name)) {
		nugu_error("'%s' pcm already exist.", pcm->name);
		return -1;
	}

	_pcms = g_list_append(_pcms, pcm);

	return 0;
}

int nugu_pcm_remove(NuguPcm *pcm)
{
	GList *l;

	l = g_list_find(_pcms, pcm);
	if (!l)
		return -1;

	_pcms = g_list_remove_link(_pcms, l);

	return 0;
}

NuguPcm *nugu_pcm_find(const char *name)
{
	GList *cur;

	g_return_val_if_fail(name != NULL, NULL);

	cur = _pcms;
	while (cur) {
		if (g_strcmp0(((NuguPcm *)cur->data)->name, name) == 0)
			return cur->data;

		cur = cur->next;
	}

	return NULL;
}

int nugu_pcm_set_audio_attribute(NuguPcm *pcm, NuguAudioAttribute attr)
{
	g_return_val_if_fail(pcm != NULL, -1);

	pcm->attr = attr;
	return 0;
}

int nugu_pcm_get_audio_attribute(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	return pcm->attr;
}

const char *nugu_pcm_get_audio_attribute_str(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, NULL);

	return nugu_audio_get_attribute_str(pcm->attr);
}

int nugu_pcm_start(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	nugu_pcm_clear_buffer(pcm);

	if (pcm->driver == NULL || pcm->driver->ops == NULL ||
	    pcm->driver->ops->start == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return pcm->driver->ops->start(pcm->driver, pcm);
}

int nugu_pcm_stop(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	nugu_pcm_clear_buffer(pcm);

	if (pcm->driver == NULL || pcm->driver->ops == NULL ||
	    pcm->driver->ops->stop == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return pcm->driver->ops->stop(pcm->driver, pcm);
}

int nugu_pcm_pause(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm->driver == NULL || pcm->driver->ops == NULL ||
	    pcm->driver->ops->pause == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return pcm->driver->ops->pause(pcm->driver, pcm);
}

int nugu_pcm_resume(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm->driver == NULL || pcm->driver->ops == NULL ||
	    pcm->driver->ops->resume == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return pcm->driver->ops->resume(pcm->driver, pcm);
}

int nugu_pcm_set_volume(NuguPcm *pcm, int volume)
{
	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm->driver == NULL || pcm->driver->ops == NULL ||
	    pcm->driver->ops->set_volume == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	if (volume < NUGU_SET_VOLUME_MIN)
		volume = NUGU_SET_VOLUME_MIN;
	else if (volume > NUGU_SET_VOLUME_MAX)
		volume = NUGU_SET_VOLUME_MAX;

	if (pcm->driver->ops->set_volume(pcm->driver, pcm, volume)) {
		nugu_error("volume is not changed");
		return -1;
	}

	pcm->volume = volume;
	nugu_dbg("change volume: %d", pcm->volume);

	return 0;
}

int nugu_pcm_get_volume(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	return pcm->volume;
}

int nugu_pcm_get_duration(NuguPcm *pcm)
{
	int samplerate;

	g_return_val_if_fail(pcm != NULL, -1);

	switch (pcm->property.samplerate) {
	case NUGU_AUDIO_SAMPLE_RATE_8K:
		samplerate = 8000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_16K:
		samplerate = 16000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_32K:
		samplerate = 32000;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_22K:
		samplerate = 22050;
		break;
	case NUGU_AUDIO_SAMPLE_RATE_44K:
		samplerate = 44100;
		break;
	default:
		samplerate = 16000;
		break;
	}

	return (pcm->total_size / samplerate);
}

int nugu_pcm_get_position(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	if (pcm->driver == NULL || pcm->driver->ops == NULL ||
	    pcm->driver->ops->get_position == NULL) {
		nugu_error("Not supported");
		return -1;
	}

	return pcm->driver->ops->get_position(pcm->driver, pcm);
}

void nugu_pcm_set_status_callback(NuguPcm *pcm, NuguMediaStatusCallback cb,
				  void *userdata)
{
	g_return_if_fail(pcm != NULL);

	pcm->scb = cb;
	pcm->sud = userdata;
}

void nugu_pcm_emit_status(NuguPcm *pcm, enum nugu_media_status status)
{
	g_return_if_fail(pcm != NULL);

	if (pcm->status == status)
		return;

	pcm->status = status;

	if (pcm->scb != NULL)
		pcm->scb(status, pcm->sud);
}

enum nugu_media_status nugu_pcm_get_status(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, NUGU_MEDIA_STATUS_STOPPED);

	return pcm->status;
}

void nugu_pcm_set_event_callback(NuguPcm *pcm, NuguMediaEventCallback cb,
				 void *userdata)
{
	g_return_if_fail(pcm != NULL);

	pcm->ecb = cb;
	pcm->eud = userdata;
}

void nugu_pcm_emit_event(NuguPcm *pcm, enum nugu_media_event event)
{
	g_return_if_fail(pcm != NULL);

	if (event == NUGU_MEDIA_EVENT_END_OF_STREAM)
		pcm->status = NUGU_MEDIA_STATUS_STOPPED;

	if (pcm->ecb != NULL)
		pcm->ecb(event, pcm->eud);
}

int nugu_pcm_set_driver_data(NuguPcm *pcm, void *data)
{
	g_return_val_if_fail(pcm != NULL, -1);

	pcm->driver_data = data;

	return 0;
}

void *nugu_pcm_get_driver_data(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, NULL);

	return pcm->driver_data;
}

void nugu_pcm_clear_buffer(NuguPcm *pcm)
{
	g_return_if_fail(pcm != NULL);
	g_return_if_fail(pcm->buf != NULL);

	pthread_mutex_lock(&pcm->mutex);

	nugu_buffer_clear(pcm->buf);
	pcm->total_size = 0;
	pcm->is_last = 0;

	pthread_mutex_unlock(&pcm->mutex);
}

int nugu_pcm_push_data(NuguPcm *pcm, const char *data, size_t size, int is_last)
{
	int ret;

	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->buf != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(size > 0, -1);

	pthread_mutex_lock(&pcm->mutex);

	pcm->total_size += size;
	if (!pcm->is_last)
		pcm->is_last = is_last;

	ret = nugu_buffer_add(pcm->buf, (void *)data, size);

	pthread_mutex_unlock(&pcm->mutex);

	if (pcm->driver && pcm->driver->ops && pcm->driver->ops->push_data)
		pcm->driver->ops->push_data(pcm->driver, pcm, data, size,
					    pcm->is_last);
	return ret;
}

int nugu_pcm_push_data_done(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	pthread_mutex_lock(&pcm->mutex);

	pcm->is_last = 1;

	pthread_mutex_unlock(&pcm->mutex);

	if (pcm->driver && pcm->driver->ops && pcm->driver->ops->push_data)
		pcm->driver->ops->push_data(pcm->driver, pcm, NULL, 0, 1);

	return 0;
}

int nugu_pcm_get_data(NuguPcm *pcm, char *data, size_t size)
{
	size_t data_size;
	const char *ptr;
	int ret;

	g_return_val_if_fail(pcm != NULL, -1);
	g_return_val_if_fail(pcm->buf != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(size != 0, -1);

	pthread_mutex_lock(&pcm->mutex);

	data_size = nugu_buffer_get_size(pcm->buf);
	if (data_size < size)
		size = data_size;

	ptr = nugu_buffer_peek(pcm->buf);
	if (ptr == NULL) {
		pthread_mutex_unlock(&pcm->mutex);

		nugu_error("buffer peek is internal error");
		return -1;
	}

	memcpy(data, ptr, size);

	ret = nugu_buffer_shift_left(pcm->buf, size);
	if (ret == -1) {
		pthread_mutex_unlock(&pcm->mutex);

		nugu_error("buffer shift left is internal error");
		return -1;
	}

	pthread_mutex_unlock(&pcm->mutex);

	return size;
}

size_t nugu_pcm_get_data_size(NuguPcm *pcm)
{
	size_t ret;

	g_return_val_if_fail(pcm != NULL, -1);

	pthread_mutex_lock(&pcm->mutex);

	ret = nugu_buffer_get_size(pcm->buf);

	pthread_mutex_unlock(&pcm->mutex);

	return ret;
}

int nugu_pcm_receive_is_last_data(NuguPcm *pcm)
{
	g_return_val_if_fail(pcm != NULL, -1);

	return pcm->is_last;
}
