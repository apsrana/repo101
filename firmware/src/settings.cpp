#include <Preferences.h>

#include "app.h"
#include "audio.h"
#include "config.h"

namespace {
constexpr const char* kNamespace = "furby";
// Bump when the stored layout changes; older data is then ignored.
constexpr uint8_t kVersion = 1;
}  // namespace

void loadSettings(int32_t& countsPerRev, bool& encoderInvert) {
  Preferences p;
  p.begin(kNamespace, true);
  bool valid = p.getUChar("ver", 0) == kVersion;
  countsPerRev = valid ? p.getInt("cpr", DEFAULT_COUNTS_PER_REV) : DEFAULT_COUNTS_PER_REV;
  encoderInvert = valid && p.getBool("inv", false);
  if (valid) audio::setVolume(p.getUChar("vol", audio::volume()));
  p.end();
}

// Call after body.begin(), which fills in default poses.
void loadPoses() {
  Preferences p;
  p.begin(kNamespace, true);
  if (p.getUChar("ver", 0) == kVersion &&
      p.getBytesLength("poses") == sizeof(int32_t) * furby::kPoseCount) {
    p.getBytes("poses", body.poses().raw(), sizeof(int32_t) * furby::kPoseCount);
  }
  p.end();
}

void saveSettings() {
  Preferences p;
  p.begin(kNamespace, false);
  p.putUChar("ver", kVersion);
  p.putInt("cpr", body.countsPerRev());
  p.putBool("inv", body.encoderInvert());
  p.putUChar("vol", audio::volume());
  p.putBytes("poses", body.poses().raw(), sizeof(int32_t) * furby::kPoseCount);
  p.end();
}
