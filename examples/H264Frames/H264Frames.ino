// Decodes an H.264 file from the SD card with the hardware decoder and counts the frames.
//
// kPath is either an Annex B byte stream (".h264" / ".264", read in chunks) or an MP4 file with an
// H.264 track (".mp4", the packets are taken with the MP4 library).
#include <MP4.h>
#include <SD.h>
#include <VideoDecoder.h>

static const char *kPath = "/video540p.mp4";  // path on the card
static const unsigned long kMaxFrames = 300;  // stop after this many frames (0: the whole file)

// true when s ends with suffix (lower case), ignoring the case of s
static bool endsWith(const char *s, const char *suffix) {
  size_t a = strlen(s), b = strlen(suffix);
  if (a < b) return false;
  for (size_t i = 0; i < b; i++) {
    char c = s[a - b + i];
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (c != suffix[i]) return false;
  }
  return true;
}

// Start of the last Annex B start code (00 00 01, with its leading zero if any) in [from, to), or from.
static size_t lastStartCode(const uint8_t *b, size_t from, size_t to) {
  for (size_t i = to; i >= from + 3; i--) {
    if (b[i - 3] == 0 && b[i - 2] == 0 && b[i - 1] == 1) {
      size_t at = i - 3;
      return (at > from && b[at - 1] == 0) ? at - 1 : at;
    }
  }
  return from;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!SD.begin()) {
    Serial.println("SD.begin() failed");
    return;
  }
  const bool mp4 = endsWith(kPath, ".mp4");
  File raw;
  MP4File movie;
  size_t cap = 128 * 1024;  // read buffer for an Annex B file
  if (mp4) {
    char full[96];
    snprintf(full, sizeof(full), "/SD:%s", kPath);
    if (!movie.open(full) || !movie.hasVideo()) {
      Serial.printf("cannot open %s (needs an H.264 track)\n", kPath);
      return;
    }
    cap = movie.maxVideoPacket() + 1024;
  } else {
    raw = SD.open(kPath);
    if (!raw) {
      Serial.printf("cannot open %s\n", kPath);
      return;
    }
  }
  uint8_t *buf = (uint8_t *)malloc(cap);
  if (!buf) {
    Serial.println("out of memory");
    return;
  }
  if (!VideoDecoder.begin() || !VideoDecoder.openH264(1024 * 1024)) {
    Serial.println("cannot open the decoder");
    return;
  }
  Serial.printf("decoding %s (%s)\n", kPath, mp4 ? "MP4" : "Annex B");

  size_t have = 0, pos = 0;  // bytes in buf, bytes already taken by the decoder
  int64_t pts = -1;
  bool eof = false, flushed = false;
  unsigned long frames = 0, bytes = 0, t0 = millis();
  uint16_t w = 0, h = 0;

  // Read more input behind the part the decoder did not take yet (a NAL unit cut by the end of
  // the buffer is completed by the next read). Returns false at the end of the file.
  auto refill = [&]() -> bool {
    size_t keep = have - pos;
    if (keep) memmove(buf, buf + pos, keep);
    pos = 0;
    have = keep;
    if (mp4) {
      MP4Packet p;
      if (!movie.nextVideo(p)) return eof = true, false;
      int n = movie.readVideoAnnexB(p, buf + keep);
      if (n <= 0) return eof = true, false;
      have += n;
      pts = p.ptsUs;
    } else {
      int n = raw.read(buf + have, cap - have);
      if (n <= 0) return eof = true, false;
      have += n;
    }
    return true;
  };

  while (true) {
    VideoFrame f;
    int r = VideoDecoder.getFrame(f);
    if (r == VideoDecoderClass::OK) {
      if (frames == 0) {
        w = f.width;
        h = f.height;
      }
      frames++;
      VideoDecoder.release(f);  // frames of a stream have to be given back in time
      if (kMaxFrames && frames >= kMaxFrames) eof = true;  // no more input, drain what is decoded
    } else if (r == VideoDecoderClass::NODATA) {
      break;  // flushed and every frame delivered
    } else if (r == VideoDecoderClass::AGAIN) {  // the decoder wants more input
      size_t used = 0;
      // The decoder wants whole NAL units starting at a start code: from a file the last one in the
      // buffer may be cut, keep it back until the next read completes it.
      size_t end = have;
      if (!mp4 && !eof) end = lastStartCode(buf, pos, have);
      if (pos < end) {
        VideoDecoder.feed(buf + pos, end - pos, pts, &used);
        pos += used;
        bytes += used;
      } else if (!mp4 && !eof && pos == 0 && have == cap) {
        Serial.println("a NAL unit is bigger than the read buffer");
        break;
      }
      if (used == 0) {  // nothing taken: give it more data, or finish
        if (!eof) {
          refill();
        } else if (!flushed) {
          VideoDecoder.flush();
          flushed = true;
        } else {
          break;
        }
      }
    } else if (r != VideoDecoderClass::BUSY) {
      Serial.printf("decoder error %d\n", r);
      break;
    }
  }
  unsigned long ms = millis() - t0;
  VideoDecoder.closeStream();
  free(buf);
  Serial.printf("%lu frames %ux%u, %lu bytes in %lu ms (%lu fps)\n", frames, w, h, bytes, ms,
                ms ? frames * 1000 / ms : 0);
}

void loop() {
  delay(1000);
}
