#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <opus.h>
#include <ogg/ogg.h>

#include "base/nugu_log.h"
#include "base/nugu_plugin.h"
#include "base/nugu_decoder.h"
#include "base/nugu_encoder.h"
#include "base/nugu_pcm.h"

#define SAMPLERATE 16000
#define CHANNELS 1
//#define FRAME_SIZE 640
#define BIT_RATE 32000
#define FRAME_LENGTH 20 /* 20ms */

#define FRAME_SIZE ((SAMPLERATE / 1000) * FRAME_LENGTH)
#define PACKET_SIZE (((BIT_RATE / 8) / 1000) * FRAME_LENGTH)
#define MAX_PACKET_SIZE (PACKET_SIZE * 2)

#define READSIZE (FRAME_SIZE * 2)
#define MAX_FRAME_SIZE (8 * FRAME_SIZE)

#define OUTPUT_FILENAME "output.opus"
#define DECODE_FILENAME "output.pcm"

OpusEncoder *enc_handle;
OpusDecoder *dec_handle;

int output_fd;
int decode_fd;
opus_int16 frames[MAX_FRAME_SIZE];
unsigned char encoded_buf[MAX_PACKET_SIZE];

ogg_stream_state *os;
ogg_page *og;
int seq;

#if 0
/* 27 bytes header */
struct _ogg_header {
	uint32_t Signature; /* 4 Bytes: "OggS" */
	uint8_t Version; /* 1 Bytes: 0 */
	uint8_t Flags; /* 1 Bytes: header_type: 0 */
	uint64_t GranulePosition; /* 8 Bytes: granule_position */
	uint32_t SerialNumber; /* 4 Bytes: bitstream_serial_number */
	uint32_t SequenceNumber; /* 4 Bytes: page_sequence_number */
	uint32_t Checksum; /* 4 Bytes: CRC_checksum */
	uint8_t TotalSegments; /* 1 Bytes: page_segments */
	uint8_t segment_table; /* (optional) */
} __attribute__((packed));

typedef struct _ogg_header OggHeader;

int SerialNumber = 0xBAB0;
int SequenceNumber = 0;
#endif

/* Decoding verification */
int do_decode(unsigned char *buf, size_t buf_len)
{
	int nbBytes;
	size_t n_written;

	nbBytes = opus_decode(dec_handle, buf, buf_len, frames, MAX_FRAME_SIZE,
			      0);
	if (nbBytes < 0) {
		printf("  opus_decode() failed: %s\n", opus_strerror(nbBytes));
	}

	n_written = write(decode_fd, frames, nbBytes * 2);
	printf("  write_callback: file write %zd bytes\n", n_written);

	return nbBytes;
}

int cnt = 0;

int do_encode(unsigned char *buf, size_t buf_len)
{
	int nbBytes;
	size_t samples = buf_len / 2;
	size_t i;
	size_t n_written;
	ogg_packet op;

	for (i = 0; i < samples; i++)
		frames[i] = (0xFF & buf[2 * i + 1]) << 8 | (0xFF & buf[2 * i]);

	nbBytes = opus_encode(enc_handle, (opus_int16 *)buf, samples,
			      encoded_buf, MAX_PACKET_SIZE);
	if (nbBytes < 0) {
		printf("  opus_encode() failed: %s\n", opus_strerror(nbBytes));
	}

	op.packet = encoded_buf;
	op.bytes = nbBytes;
	if (seq == 0)
		op.b_o_s = 0;
	else
		op.b_o_s = 0x1;

	op.e_o_s = 0;
	op.granulepos = seq * samples;
	op.packetno = seq++;

	ogg_stream_packetin(os, &op);

	nugu_hexdump(NUGU_LOG_MODULE_DEFAULT, encoded_buf, nbBytes, "", "",
		     "    ");


cnt++;
if (cnt < 5)
	return nbBytes;

cnt = 0;
	//	n_written = write(output_fd, encoded_buf, nbBytes);
	//	printf("  write_callback: file write %zd bytes\n", n_written);

#if 0
	while (ogg_stream_pageout(os, og)) {
		printf("ogg_stream_pageout: og->header_len: %ld\tog->body_len: %ld\n",
		       og->header_len, og->body_len);

		n_written = write(output_fd, og->header, og->header_len);
		printf("  write_callback: file write header %zd bytes\n",
		       n_written);
		n_written = write(output_fd, og->body, og->body_len);
		printf("  write_callback: file write body %zd bytes\n",
		       n_written);
	}
#endif
#if 1
	while (ogg_stream_flush(os, og)) {
		printf("ogg_stream_flush: og->header_len: %ld\tog->body_len: %ld\n",
		       og->header_len, og->body_len);

		n_written = write(output_fd, og->header, og->header_len);
		printf("  write_callback: file write header %zd bytes\n",
		       n_written);
		n_written = write(output_fd, og->body, og->body_len);
		printf("  write_callback: file write body %zd bytes\n",
		       n_written);
	}
#endif
	do_decode(encoded_buf, nbBytes);

	return nbBytes;
}

int main(int argc, char *argv[])
{
	int pcm_fd;
	unsigned char pcm_buf[READSIZE];
	size_t n_read;
	int n_encoded;
	int err = 0;

	if (argc < 2) {
		printf("%s <pcm-file>\n", argv[0]);
		return -1;
	}

	pcm_fd = open(argv[1], O_RDONLY);
	if (pcm_fd < 0) {
		printf("open(%s) failed\n", argv[1]);
		return -1;
	}

	printf("---------------------------------------------------\n");
	printf("SAMPLE_RATE: %8d\tCHANNELS   : %5d\tFRAME_LENGTH: %5d ms\n",
	       SAMPLERATE, CHANNELS, FRAME_LENGTH);
	printf("FRAME_SIZE : %8d\tPACKET_SIZE: %5d\n", FRAME_SIZE, PACKET_SIZE);
	printf("---------------------------------------------------\n");

	output_fd =
		open(OUTPUT_FILENAME, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (output_fd < 0) {
		printf("open(%s) failed\n", OUTPUT_FILENAME);
		return -1;
	}

	decode_fd =
		open(DECODE_FILENAME, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (decode_fd < 0) {
		printf("open(%s) failed\n", DECODE_FILENAME);
		return -1;
	}

	dec_handle = opus_decoder_create(SAMPLERATE, CHANNELS, &err);
	if (!dec_handle) {
		printf("ope_encoder_create() failed: %s\n", opus_strerror(err));
		close(pcm_fd);
		close(output_fd);
		close(decode_fd);
		return -1;
	}

	enc_handle = opus_encoder_create(SAMPLERATE, CHANNELS,
					 OPUS_APPLICATION_VOIP, &err);
	if (!enc_handle) {
		printf("ope_encoder_create() failed: %s\n", opus_strerror(err));
		close(pcm_fd);
		close(output_fd);
		close(decode_fd);
		return -1;
	}

	opus_encoder_ctl(enc_handle, OPUS_SET_BITRATE(32000));

	int bitrate = 0;
	opus_encoder_ctl(enc_handle, OPUS_GET_BITRATE(&bitrate));
	nugu_error("bitrate = %d", bitrate);

	opus_encoder_ctl(enc_handle, OPUS_SET_VBR(0));
	opus_encoder_ctl(enc_handle,
			 OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_20_MS));

	os = malloc(sizeof(ogg_stream_state));
	og = malloc(sizeof(ogg_page));

	err = ogg_stream_init(os, rand());
	if (err < 0) {
		printf("ogg_stream_init() failed\n");
		return -1;
	}

	while (1) {
		n_read = read(pcm_fd, pcm_buf, READSIZE);
		if (n_read <= 0) {
			printf("> read done\n");
			break;
		}

		printf("> read %zd bytes\n", n_read);

		n_encoded = do_encode(pcm_buf, n_read);
		printf("  encoded %d bytes\n", n_encoded);
	}

	printf("> finalize\n");
	opus_encoder_destroy(enc_handle);
	opus_decoder_destroy(dec_handle);

	ogg_stream_destroy(os);
	free(og);

	close(pcm_fd);
	close(output_fd);
	close(decode_fd);

	return 0;
}
