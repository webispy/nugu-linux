#include <glib.h>

#include "fake_recorder.h"
#include "ncb_helper.h"

#define FILEPATH "pcm1.dat"

#define rec_100ms (22050 * 2 / 10)
#define rec_5sec (22050 * 2 * 5 / rec_100ms)

static NuguRecorderDriver *rec_driver;

static gboolean on_mic_data(gpointer data)
{
	NuguRecorder *rec = data;
	char *buf;
	FILE *fp;
	ssize_t nread = rec_100ms;

	buf = calloc(1, rec_100ms);
	if (!buf)
		return FALSE;

	fp = nugu_recorder_get_driver_data(rec);
	if (fp) {
		nread = fread(buf, 1, rec_100ms, fp);
		printf("read pcm data from file (%zd bytes)\n", nread);

		if (nread == 0) {
			fclose(fp);
			nugu_recorder_set_driver_data(rec, NULL);
			nread = rec_100ms;
		}
	}

	if (nread > 0) {
		printf("recorder(%p) push frame (%zd bytes)\n", rec, nread);
		nugu_recorder_push_frame(rec, buf, nread);
	}

	free(buf);

	return TRUE;
}

static int _rec_start(NuguRecorderDriver *driver, NuguRecorder *rec,
		      NuguAudioProperty prop)
{
	FILE *fp;

	printf("recorder(%p) start\n", rec);

	fp = fopen(FILEPATH, "rb");
	if (!fp) {
		printf("file open error\n");
		return -1;
	}

	nugu_recorder_set_driver_data(rec, fp);
	nugu_recorder_set_frame_size(rec, rec_100ms, rec_5sec);

	g_timeout_add(100, on_mic_data, rec);

	return 0;
}

static int _rec_stop(NuguRecorderDriver *driver, NuguRecorder *rec)
{
	FILE *fp;

	printf("recorder(%p) stop\n", rec);

	fp = nugu_recorder_get_driver_data(rec);
	if (fp) {
		fclose(fp);
		nugu_recorder_set_driver_data(rec, NULL);
	}

	return 0;
}

static struct nugu_recorder_driver_ops rec_ops = {
	//
	.start = _rec_start,
	.stop = _rec_stop
};

int register_recorder()
{
	printf("recorder driver register\n");

	rec_driver = nugu_recorder_driver_new("fake_recorder", &rec_ops);
	if (!rec_driver) {
		printf("nugu_recorder_driver_new() failed\n");
		return -1;
	}

	if (nugu_recorder_driver_register(rec_driver) != 0) {
		nugu_recorder_driver_free(rec_driver);
		rec_driver = NULL;
		return -1;
	}

	return 0;
}

int unregister_recorder()
{
	printf("recorder driver unregister\n");

	if (rec_driver) {
		nugu_recorder_driver_remove(rec_driver);
		nugu_recorder_driver_free(rec_driver);
		rec_driver = NULL;
	}

	return 0;
}
