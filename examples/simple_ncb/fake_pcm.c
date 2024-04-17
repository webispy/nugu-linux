#include "fake_pcm.h"
#include "ncb_helper.h"

static int is_first;
static int fake_pos = 0;
static int fake_duration = 5;

static int _pcm_create(NuguPcmDriver *driver, NuguPcm *pcm,
		       NuguAudioProperty prop)
{
	printf("pcm create\n");
	return 0;
}

static void _pcm_destroy(NuguPcmDriver *driver, NuguPcm *pcm)
{
	printf("pcm destroy\n");
}

static int _pcm_start(NuguPcmDriver *driver, NuguPcm *pcm)
{
	printf("pcm start\n");
	is_first = 1;
	fake_pos = 0;
	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_READY);

	return 0;
}

static int _pcm_stop(NuguPcmDriver *driver, NuguPcm *pcm)
{
	printf("pcm stop\n");
	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_STOPPED);

	return 0;
}

static int _pcm_pause(NuguPcmDriver *driver, NuguPcm *pcm)
{
	printf("pcm pause\n");
	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_PAUSED);

	return 0;
}

static int _pcm_resume(NuguPcmDriver *driver, NuguPcm *pcm)
{
	printf("pcm resume\n");
	nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_PLAYING);

	return 0;
}

static int _pcm_set_volume(NuguPcmDriver *driver, NuguPcm *pcm, int volume)
{
	printf("pcm set volume\n");
	return 0;
}

static int _pcm_get_position(NuguPcmDriver *driver, NuguPcm *pcm)
{
	printf("pcm get position return %d\n", fake_pos++);

	if (fake_pos == fake_duration) {
		nugu_pcm_emit_event(pcm, NUGU_MEDIA_EVENT_END_OF_STREAM);
		nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_STOPPED);
	}

	return fake_pos;
}

static int _pcm_push_data(NuguPcmDriver *driver, NuguPcm *pcm, const char *data,
			  size_t size, int is_last)
{
	printf("pcm push data(%zd, is_last=%d)\n", size, is_last);

	if (is_first) {
		nugu_pcm_emit_status(pcm, NUGU_MEDIA_STATUS_PLAYING);
		is_first = 0;
	}

	return 0;
}

static struct nugu_pcm_driver_ops pcm_ops = {
	//
	.create = _pcm_create,
	.destroy = _pcm_destroy,
	.start = _pcm_start,
	.stop = _pcm_stop,
	.pause = _pcm_pause,
	.resume = _pcm_resume,
	.push_data = _pcm_push_data,
	.set_volume = _pcm_set_volume,
	.get_position = _pcm_get_position
};

static NuguPcmDriver *pcm_driver;

int register_pcm()
{
	printf("pcm driver register\n");

	pcm_driver = nugu_pcm_driver_new("fake_pcm", &pcm_ops);
	if (!pcm_driver) {
		printf("nugu_pcm_driver_new() failed\n");
		return -1;
	}

	if (nugu_pcm_driver_register(pcm_driver) != 0) {
		nugu_pcm_driver_free(pcm_driver);
		pcm_driver = NULL;
		return -1;
	}

	return 0;
}

int unregister_pcm()
{
	printf("pcm driver unregister\n");

	if (pcm_driver) {
		nugu_pcm_driver_remove(pcm_driver);
		nugu_pcm_driver_free(pcm_driver);
		pcm_driver = NULL;
	}

	return 0;
}
