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

#define FILENAME "/tmp/test.ogg"

struct opus_data {
	NuguBuffer *buf;
	int fd;
	unsigned char *data;
	size_t pos;
	size_t filesize;
};

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

int main(int argc, char *argv[])
{
	struct opus_data *od;

	od = malloc(sizeof(struct opus_data));

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

	while (1) {
		if (encode(od) < 0)
			break;
		nugu_error("buffer size: %zd", nugu_buffer_get_size(od->buf));

		nugu_hexdump(NUGU_LOG_MODULE_DEFAULT, nugu_buffer_peek(od->buf),
			     nugu_buffer_get_size(od->buf), "", "", "");

		nugu_buffer_clear(od->buf);
	}

encode(od);

	return 0;
}
