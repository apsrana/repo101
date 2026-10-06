#include "audio.h"

#include <LittleFS.h>
#include <driver/i2s.h>

#include "babble.h"
#include "config.h"
#include "wav.h"

namespace audio {
namespace {

constexpr i2s_port_t kPort = I2S_NUM_0;
constexpr size_t kFrames = 256;

struct Request {
  uint32_t id;
  furby::Sound sound;
  char path[48];  // non-empty = play this file
};

QueueHandle_t queue_;
// busy = a request newer than the last finished one exists.
volatile uint32_t requested_ = 0;
volatile uint32_t finished_ = 0;
volatile bool stop_ = false;
volatile uint8_t volume_ = 70;
int16_t stereo_[kFrames * 2];

// Writes mono samples as stereo frames with volume applied.
void writeMono(const int16_t* mono, size_t n) {
  int32_t vol = volume_;
  for (size_t i = 0; i < n; ++i) {
    int16_t s = static_cast<int16_t>(static_cast<int32_t>(mono[i]) * vol / 100);
    stereo_[2 * i] = stereo_[2 * i + 1] = s;
  }
  size_t written;
  i2s_write(kPort, stereo_, n * 4, &written, portMAX_DELAY);
}

bool interrupted() { return stop_ || uxQueueMessagesWaiting(queue_) > 0; }

// Streams PCM sample data from a WAV file as mono int16.
class WavReader {
 public:
  bool open(const char* path) {
    f_ = LittleFS.open(path, "r");
    if (!f_) return false;
    uint8_t hdr[512];
    size_t n = f_.read(hdr, sizeof(hdr));
    if (!furby::parseWav(hdr, n, info_)) {
      Serial.printf("[audio] %s: not 8/16-bit PCM WAV\n", path);
      f_.close();
      return false;
    }
    f_.seek(info_.dataOffset);
    left_ = info_.dataSize;
    frameBytes_ = info_.channels * info_.bitsPerSample / 8;
    bufLen_ = bufPos_ = 0;
    return true;
  }
  uint32_t rate() const { return info_.sampleRate; }
  void close() { f_.close(); }

  bool next(int16_t& out) {
    if (bufPos_ + frameBytes_ > bufLen_) {
      if (left_ < frameBytes_) return false;
      size_t want = sizeof(buf_) / frameBytes_ * frameBytes_;
      if (want > left_) want = left_;
      bufLen_ = f_.read(buf_, want);
      bufPos_ = 0;
      left_ -= bufLen_;
      if (bufLen_ < frameBytes_) return false;
    }
    const uint8_t* p = buf_ + bufPos_;
    bufPos_ += frameBytes_;
    int32_t sum = 0;
    for (uint16_t c = 0; c < info_.channels; ++c) {
      if (info_.bitsPerSample == 16) sum += static_cast<int16_t>(p[0] | (p[1] << 8)), p += 2;
      else sum += (static_cast<int32_t>(*p++) - 128) << 8;
    }
    out = static_cast<int16_t>(sum / info_.channels);
    return true;
  }

 private:
  File f_;
  furby::WavInfo info_;
  uint32_t left_ = 0;
  uint16_t frameBytes_ = 2;
  uint8_t buf_[1024];
  size_t bufLen_ = 0, bufPos_ = 0;
};

WavReader reader_;
int16_t mono_[kFrames];

bool streamFile(const char* path) {
  if (!LittleFS.exists(path) || !reader_.open(path)) return false;
  // Nearest-neighbour resample, 16.16 fixed point.
  const uint32_t step = (static_cast<uint64_t>(reader_.rate()) << 16) / AUDIO_SAMPLE_RATE;
  uint32_t acc = 0;
  uint32_t consumed = 0;  // input samples read so far
  int16_t cur = 0;
  bool more = true;
  while (more && !interrupted()) {
    size_t n = 0;
    while (n < kFrames) {
      uint32_t want = (acc >> 16) + 1;
      while (consumed < want && (more = reader_.next(cur))) ++consumed;
      if (!more) break;
      mono_[n++] = cur;
      acc += step;
      // Keep the accumulator small; `consumed` tracks whole samples.
      if (acc >= (1u << 30)) {
        acc -= (consumed - 1) << 16;
        consumed = 1;
      }
    }
    if (n) writeMono(mono_, n);
  }
  reader_.close();
  return true;
}

void synth(furby::Sound s) {
  furby::Babbler b;
  b.start(s, AUDIO_SAMPLE_RATE, esp_random());
  while (!b.done() && !interrupted()) {
    size_t n = b.render(mono_, kFrames);
    writeMono(mono_, n);
  }
}

void task(void*) {
  Request r;
  for (;;) {
    if (xQueueReceive(queue_, &r, portMAX_DELAY) != pdTRUE) continue;
    stop_ = false;
    if (r.path[0]) {
      if (!streamFile(r.path)) Serial.printf("[audio] can't play %s\n", r.path);
    } else {
      char path[48];
      snprintf(path, sizeof(path), "/sounds/%s.wav", furby::soundName(r.sound));
      if (!streamFile(path)) synth(r.sound);
    }
    i2s_zero_dma_buffer(kPort);
    if (static_cast<int32_t>(r.id - finished_) > 0) finished_ = r.id;
  }
}

void enqueue(Request& r) {
  r.id = requested_ + 1;
  requested_ = r.id;  // busy from now on
  xQueueReset(queue_);
  xQueueSend(queue_, &r, 0);
}

}  // namespace

void begin() {
  if (!LittleFS.begin(true)) Serial.println(F("[audio] LittleFS mount failed"));

  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = AUDIO_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = kFrames;
  cfg.tx_desc_auto_clear = true;
  i2s_driver_install(kPort, &cfg, 0, nullptr);

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_LRC;
  pins.data_out_num = PIN_I2S_DIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(kPort, &pins);
  i2s_zero_dma_buffer(kPort);

  queue_ = xQueueCreate(1, sizeof(Request));
  xTaskCreatePinnedToCore(task, "audio", 6144, nullptr, 3, nullptr, 0);
}

void play(furby::Sound s) {
  if (s == furby::Sound::kNone) return;
  Request r = {0, s, ""};
  enqueue(r);
}

bool playFile(const char* path) {
  if (!LittleFS.exists(path)) return false;
  Request r = {0, furby::Sound::kNone, ""};
  strlcpy(r.path, path, sizeof(r.path));
  enqueue(r);
  return true;
}

void stop() {
  xQueueReset(queue_);
  stop_ = true;
  finished_ = requested_;
}

bool busy() { return finished_ != requested_; }
void setVolume(uint8_t percent) { volume_ = percent > 100 ? 100 : percent; }
uint8_t volume() { return volume_; }

}  // namespace audio
