#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <opusenc.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_decoder.h"
#include "base/nugu_encoder.h"
#include "base/nugu_pcm.h"

#define SAMPLERATE 16000
#define CHANNELS 1
#define FRAME_SIZE 640

#define READSIZE (FRAME_SIZE * 2)
#define MAX_PACKET_SIZE (3 * 1276)
#define MAX_FRAME_SIZE 16 * 960

#define OUTPUT_FILENAME "output.opus"
#define PACKET_FILENAME "output.packet"

OggOpusEnc *enc_handle;
int is_ready;
int output_fd;
int packet_fd;
opus_int16 frames[MAX_FRAME_SIZE];

int do_encode(unsigned char *buf, size_t buf_len)
{
	int nbBytes;
	size_t samples = buf_len / 2;
	size_t i;
	int bitrate;

	printf("  samples: %zd\n", samples);

	for (i = 0; i < samples; i++)
		frames[i] = (0xFF & buf[2 * i + 1]) << 8 | (0xFF & buf[2 * i]);

	//	nbBytes = ope_encoder_write(enc_handle, frames, samples);
	nbBytes = ope_encoder_write(enc_handle, (opus_int16 *)buf, buf_len / 2);
	if (nbBytes < 0) {
		printf("  ope_encoder_write() failed: %s\n",
		       ope_strerror(nbBytes));
	}

	ope_encoder_ctl(enc_handle, OPUS_GET_BITRATE(&bitrate));
	nugu_error("bitrate = %d", bitrate);

	return nbBytes;
}

void packet_callback(void *user_data, const unsigned char *packet_ptr,
		     opus_int32 packet_len, opus_uint32 flags)
{
	size_t n_written;

	printf("  packet_callback: %d bytes (flags=0x%X)\n", packet_len, flags);
	//	nugu_hexdump(NUGU_LOG_MODULE_DEFAULT, packet_ptr, packet_len, "", "",
	//		     "    ");

	if (is_ready == 0)
		return;

	n_written = write(packet_fd, packet_ptr, packet_len);
	printf("  packet_callback: file write %zd bytes\n", n_written);
}

int write_func(void *user_data, const unsigned char *ptr, opus_int32 len)
{
	size_t n_written;

	printf("  write_callback: %d bytes\n", len);

	if (is_ready == 0)
		return 0;

	nugu_hexdump(NUGU_LOG_MODULE_DEFAULT, ptr, len, "", "", "    ");

	n_written = write(output_fd, ptr, len);
	printf("  write_callback: file write %zd bytes\n", n_written);

	return 0;
}

int close_func(void *user_data)
{
	printf("  close_callback\n");
	return 0;
}

int main(int argc, char *argv[])
{
	int pcm_fd;
	unsigned char pcm_buf[READSIZE];
	size_t n_read;
	int n_encoded;
	int err = 0;
	OggOpusComments *comments;
	OpusEncCallbacks callbacks;

	if (argc < 2) {
		printf("%s <pcm-file>\n", argv[0]);
		return -1;
	}

	pcm_fd = open(argv[1], O_RDONLY);
	if (pcm_fd < 0) {
		printf("open(%s) failed\n", argv[1]);
		return -1;
	}

#if 1
	output_fd =
		open(OUTPUT_FILENAME, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (output_fd < 0) {
		printf("open(%s) failed\n", OUTPUT_FILENAME);
		return -1;
	}

	packet_fd =
		open(PACKET_FILENAME, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (packet_fd < 0) {
		printf("open(%s) failed\n", PACKET_FILENAME);
		return -1;
	}
#endif

	comments = ope_comments_create();

	callbacks.write = write_func;
	callbacks.close = close_func;

	is_ready = 1;

#if 0
	/* Direct file write */
	enc_handle = ope_encoder_create_file(OUTPUT_FILENAME, comments,
					     SAMPLERATE, CHANNELS, 0, &err);
#else
	/* Callback */
	enc_handle = ope_encoder_create_callbacks(
		&callbacks, NULL, comments, SAMPLERATE, CHANNELS, 0, &err);
#endif

	if (!enc_handle) {
		printf("ope_encoder_create() failed: %s\n", ope_strerror(err));
		close(pcm_fd);
		close(output_fd);
		close(packet_fd);
		return -1;
	}

	ope_encoder_ctl(enc_handle, OPUS_SET_BITRATE(32000));

	ope_encoder_ctl(enc_handle, OPUS_SET_BITRATE_REQUEST, 32000);

	int bitrate = 0;
	ope_encoder_ctl(enc_handle, OPUS_GET_BITRATE(&bitrate));
	nugu_error("bitrate = %d", bitrate);

#if 0
	ope_encoder_ctl(
		enc_handle,
		OPUS_SET_APPLICATION(OPUS_APPLICATION_RESTRICTED_LOWDELAY));
#endif
//	ope_encoder_ctl(enc_handle, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
	ope_encoder_ctl(enc_handle, OPUS_SET_VBR(0));
//	ope_encoder_ctl(enc_handle, OPUS_SET_DTX(1));
//	ope_encoder_ctl(enc_handle, OPUS_SET_PREDICTION_DISABLED(1));
	ope_encoder_ctl(enc_handle,
			OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_20_MS));

	ope_encoder_ctl(enc_handle, OPUS_GET_BITRATE(&bitrate));
	nugu_error("bitrate = %d", bitrate);

	//	ope_encoder_ctl(enc_handle,
	//			OPE_SET_PACKET_CALLBACK(packet_callback, NULL));

	//	ope_encoder_flush_header(enc_handle);
	is_ready = 1;

	while (1) {
		n_read = read(pcm_fd, pcm_buf, READSIZE);
		if (n_read <= 0) {
			printf("> read done\n");
			break;
		}

		printf("> read %zd bytes\n", n_read);

		n_encoded = do_encode(pcm_buf, n_read);
		printf("  encoded %d bytes\n", n_encoded);

		usleep(50 * 1000);
	}

	printf("> finalize\n");
	ope_encoder_drain(enc_handle);
	ope_encoder_destroy(enc_handle);
	ope_comments_destroy(comments);

	close(pcm_fd);
	close(output_fd);
	close(packet_fd);
	return 0;
}
