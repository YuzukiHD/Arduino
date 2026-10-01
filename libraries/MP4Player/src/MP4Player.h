/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MP4Player_h
#define MP4Player_h

#include <stdint.h>
#include "Arduino.h"

/*
 * Plays an MP4 file (H.264 video + AAC audio at 48000 or 44100 Hz) from the SD
 * card: the video goes to the display video plane through the hardware
 * decoder, the sound to the headphone output of the on-chip codec.
 *
 *   MP4Player.begin();
 *   MP4Player.play("/SD:/clip.mp4");      // blocks until the end
 *
 * or, without blocking the sketch:
 *
 *   MP4Player.start("/SD:/clip.mp4");
 *   while (MP4Player.update()) { ... }    // call it as often as possible
 *
 * Paths start with the mount point "/SD:". Uses the LCD pins (PD0..PD22,
 * PB0..PB3) and turns the PWM block off.
 */
class MP4PlayerClass {
public:
	bool begin();
	/* start a file; false on error, see error() */
	bool start(const char *path);
	/* one step; true while the playback goes on */
	bool update();
	/* play to the end (or until stop() is called from an interrupt-free context) */
	bool play(const char *path);
	void stop();
	bool playing() const;

	/* 0..255, applies to the headphone output */
	bool setVolume(uint8_t volume);
	bool mute(bool on = true);

	/* information about the current / last file */
	uint16_t width() const;
	uint16_t height() const;
	uint32_t frames() const;
	uint32_t durationMs() const;
	uint32_t positionMs() const;
	uint32_t framesShown() const;
	uint32_t framesDropped() const;
	uint32_t audioUnderruns() const;
	int avOffsetMs() const;
	const char *error() const;
};

extern MP4PlayerClass MP4Player;

#endif
