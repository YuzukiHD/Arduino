/* SPDX-License-Identifier: Apache-2.0 */
/*
 * MP4 player engine of the MP4Player library: H.264 on the display video
 * plane through the video engine, AAC decoded in software and played by the
 * on-chip codec. No Zephyr header here: it is included by C++ code.
 */
#ifndef MP4PLAYER_CORE_H_
#define MP4PLAYER_CORE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mp4p_info {
	uint16_t width, height;
	uint32_t frames;	/* frames in the file */
	uint32_t duration_ms;
	uint32_t sample_rate;	/* 0 without sound */
	uint32_t shown, dropped, underruns;
	int32_t av_offset_ms;	/* picture lateness against the sound */
	uint32_t position_ms;
};

/* all return 0 or a negative errno */
int mp4p_begin(void);
int mp4p_start(const char *path);
/* one step of the playback: 1 while it goes on, 0 when it ended, negative on error */
int mp4p_update(void);
void mp4p_stop(void);
int mp4p_playing(void);
void mp4p_info(struct mp4p_info *info);
int mp4p_set_volume(int volume_0_255);
int mp4p_set_mute(int mute);
/* last error text of mp4p_start/update, "" when there is none */
const char *mp4p_error(void);

#ifdef __cplusplus
}
#endif

#endif
