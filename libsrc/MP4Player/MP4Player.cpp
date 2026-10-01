/* SPDX-License-Identifier: Apache-2.0 */
#include "Arduino.h"
#include "MP4Player.h"
#include "mp4player_core.h"

MP4PlayerClass MP4Player;

static struct mp4p_info info(void)
{
	struct mp4p_info i;
	mp4p_info(&i);
	return i;
}

bool MP4PlayerClass::begin() { return mp4p_begin() == 0; }
bool MP4PlayerClass::start(const char *path) { return mp4p_start(path) == 0; }
bool MP4PlayerClass::update() { return mp4p_update() > 0; }
bool MP4PlayerClass::playing() const { return mp4p_playing() != 0; }
void MP4PlayerClass::stop() { mp4p_stop(); }

bool MP4PlayerClass::play(const char *path)
{
	if (!start(path)) {
		return false;
	}
	while (update()) {
	}
	return mp4p_error()[0] == 0;
}

bool MP4PlayerClass::setVolume(uint8_t volume) { return mp4p_set_volume(volume) == 0; }
bool MP4PlayerClass::mute(bool on) { return mp4p_set_mute(on) == 0; }

uint16_t MP4PlayerClass::width() const { return info().width; }
uint16_t MP4PlayerClass::height() const { return info().height; }
uint32_t MP4PlayerClass::frames() const { return info().frames; }
uint32_t MP4PlayerClass::durationMs() const { return info().duration_ms; }
uint32_t MP4PlayerClass::positionMs() const { return info().position_ms; }
uint32_t MP4PlayerClass::framesShown() const { return info().shown; }
uint32_t MP4PlayerClass::framesDropped() const { return info().dropped; }
uint32_t MP4PlayerClass::audioUnderruns() const { return info().underruns; }
int MP4PlayerClass::avOffsetMs() const { return info().av_offset_ms; }
const char *MP4PlayerClass::error() const { return mp4p_error(); }
