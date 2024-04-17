#include "fake_player.h"
#include "ncb_helper.h"

static NuguPlayerDriver *player_driver;
static int fake_pos = 0;
static int fake_duration = 10;

static int _create(NuguPlayerDriver *driver, NuguPlayer *player)
{
	printf("player create\n");

	return 0;
}

static void _destroy(NuguPlayerDriver *driver, NuguPlayer *player)
{
	printf("player destroy\n");
}

static int _set_source(NuguPlayerDriver *driver, NuguPlayer *player,
		       const char *url)
{
	printf("player set_source(%s)\n", url);
	nugu_player_emit_event(player, NUGU_MEDIA_EVENT_MEDIA_SOURCE_CHANGED);

	return 0;
}

static int _start(NuguPlayerDriver *driver, NuguPlayer *player)
{
	printf("player start\n");
	fake_pos = 0;

	nugu_player_emit_event(player, NUGU_MEDIA_EVENT_MEDIA_LOADED);
	nugu_player_emit_status(player, NUGU_MEDIA_STATUS_PLAYING);

	return 0;
}

static int _stop(NuguPlayerDriver *driver, NuguPlayer *player)
{
	printf("player stop\n");
	nugu_player_emit_status(player, NUGU_MEDIA_STATUS_STOPPED);

	return 0;
}

static int _pause(NuguPlayerDriver *driver, NuguPlayer *player)
{
	printf("player pause\n");
	nugu_player_emit_status(player, NUGU_MEDIA_STATUS_PAUSED);

	return 0;
}

static int _resume(NuguPlayerDriver *driver, NuguPlayer *player)
{
	printf("player resume\n");
	nugu_player_emit_status(player, NUGU_MEDIA_STATUS_PLAYING);

	return 0;
}

static int _seek(NuguPlayerDriver *driver, NuguPlayer *player, int sec)
{
	printf("player seek(%d)\n", sec);
	return 0;
}

static int _set_volume(NuguPlayerDriver *driver, NuguPlayer *player, int vol)
{
	printf("player set_volume(%d)\n", vol);
	return 0;
}

static int _get_duration(NuguPlayerDriver *driver, NuguPlayer *player)
{
	printf("player get_duration return %d\n", fake_duration);
	return fake_duration;
}

static int _get_position(NuguPlayerDriver *driver, NuguPlayer *player)
{
	printf("player get_position return %d\n", fake_pos++);

	if (fake_pos == fake_duration) {
		nugu_player_emit_event(player, NUGU_MEDIA_EVENT_END_OF_STREAM);
		nugu_player_emit_status(player, NUGU_MEDIA_STATUS_STOPPED);
	}

	return fake_pos;
}

static struct nugu_player_driver_ops player_ops = {
	//
	.create = _create,
	.destroy = _destroy,
	.set_source = _set_source,
	.start = _start,
	.stop = _stop,
	.pause = _pause,
	.resume = _resume,
	.seek = _seek,
	.set_volume = _set_volume,
	.get_duration = _get_duration,
	.get_position = _get_position
};

int register_player()
{
	printf("player driver register\n");

	player_driver = nugu_player_driver_new("fake_player", &player_ops);
	if (!player_driver) {
		printf("nugu_player_driver_new() failed\n");
		return -1;
	}

	if (nugu_player_driver_register(player_driver) != 0) {
		nugu_player_driver_free(player_driver);
		player_driver = NULL;
		return -1;
	}

	return 0;
}

int unregister_player()
{
	printf("player driver unregister\n");

	if (player_driver) {
		nugu_player_driver_remove(player_driver);
		nugu_player_driver_free(player_driver);
		player_driver = NULL;
	}

	return 0;
}
