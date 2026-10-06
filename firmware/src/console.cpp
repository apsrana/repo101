// Serial console (115200 baud) for wiring bring-up and pose tuning.
// Type `help` for the command list.

#include <LittleFS.h>

#include "app.h"
#include "audio.h"
#include "config.h"
#include "inputs.h"

namespace {

char line_[96];
size_t len_ = 0;

const char* modeName(Body::Mode m) {
  switch (m) {
    case Body::Mode::kIdle: return "idle";
    case Body::Mode::kManual: return "manual";
    case Body::Mode::kHoming: return "homing";
    case Body::Mode::kCalibrating: return "calibrating";
    case Body::Mode::kMoving: return "moving";
  }
  return "?";
}

void help() {
  Serial.println(F(
      "Bring-up:\n"
      "  probe              toggle printing raw switch / mic / light changes\n"
      "  status             position, mode, sensors, settings\n"
      "Motor:\n"
      "  fwd [pwm] | rev [pwm] | stop    manual drive (pwm 0-255)\n"
      "  home               find the home switch and zero the encoder\n"
      "  cal                measure counts per cam revolution (then `save`)\n"
      "  goto <count>       move cam to an encoder count\n"
      "Poses & animations:\n"
      "  poses              list poses and their counts\n"
      "  pose <name>        move to a pose\n"
      "  setpose <name> [count]  set pose to count (default: current position)\n"
      "  resetposes         evenly spaced default poses\n"
      "  anims              list animations\n"
      "  anim <name>        play an animation\n"
      "Sound:\n"
      "  say <sound>        chatter happy giggle yawn whoa purr yum hello\n"
      "  play <path>        play a WAV from LittleFS, e.g. /sounds/hello.wav\n"
      "  ls                 list files on LittleFS\n"
      "  vol <0-100>        speaker volume\n"
      "Behaviour:\n"
      "  auto on|off        run / pause the autonomous personality\n"
      "  sleep | wake\n"
      "  save               store calibration, poses and volume in flash"));
}

void status() {
  Serial.printf("mode=%s homed=%d count=%ld pos=%ld/%ld home_switch=%s inverted=%d\n",
                modeName(body.mode()), body.homed(), (long)body.count(), (long)body.position(),
                (long)body.countsPerRev(), body.homeActive() ? "ACTIVE" : "open",
                body.encoderInvert());
  Serial.printf("brain=%s auto=%d anim=%s audio=%s vol=%u mic=%d light=%d\n",
                brain.state() == furby::Brain::State::kAwake ? "awake" : "asleep", autonomous,
                sequencer.busy() ? sequencer.current()->name : "-",
                audio::busy() ? "playing" : "idle", audio::volume(), inputs::micLevel(),
                inputs::lightLevel());
  Serial.printf("last reset: %s  motor duty=%d%%\n", resetReason(),
                body.dutyPercent());
}

void listPoses() {
  for (uint8_t i = 0; i < furby::kPoseCount; ++i) {
    auto p = static_cast<furby::Pose>(i);
    Serial.printf("  %-13s %ld\n", furby::poseName(p), (long)body.poses().get(p));
  }
}

bool parseSound(const char* name, furby::Sound& out) {
  for (uint8_t i = 1; i < static_cast<uint8_t>(furby::Sound::kCount); ++i) {
    if (strcmp(name, furby::soundName(static_cast<furby::Sound>(i))) == 0) {
      out = static_cast<furby::Sound>(i);
      return true;
    }
  }
  return false;
}

void manualMode() {
  // Console motion takes over from the personality until `auto on`.
  if (autonomous) Serial.println(F("(autonomous behaviour paused - `auto on` to resume)"));
  autonomous = false;
  sequencer.cancel();
}

void run(char* cmd) {
  char* arg = strchr(cmd, ' ');
  if (arg) {
    *arg++ = '\0';
    while (*arg == ' ') ++arg;
  } else {
    arg = cmd + strlen(cmd);
  }
  const bool hasArg = *arg != '\0';

  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
    help();
  } else if (!strcmp(cmd, "status")) {
    status();
  } else if (!strcmp(cmd, "probe")) {
    inputs::setProbe(!inputs::probe());
    Serial.printf("probe %s\n", inputs::probe() ? "on" : "off");
  } else if (!strcmp(cmd, "fwd") || !strcmp(cmd, "rev")) {
    manualMode();
    int pwm = hasArg ? constrain(atoi(arg), 0, 255) : MOTOR_PWM_JOG;
    body.drive(cmd[0] == 'f' ? 1 : -1, pwm);
  } else if (!strcmp(cmd, "stop")) {
    stopAll();
    Serial.printf("stopped at count %ld (pos %ld)\n", (long)body.count(), (long)body.position());
  } else if (!strcmp(cmd, "home")) {
    manualMode();
    body.startHoming();
  } else if (!strcmp(cmd, "cal")) {
    manualMode();
    body.startCalibration();
    Serial.println(F("calibrating: one full cam turn..."));
  } else if (!strcmp(cmd, "goto") && hasArg) {
    manualMode();
    if (!body.moveTo(atol(arg))) Serial.println(F("not homed - run `home` first"));
  } else if (!strcmp(cmd, "poses")) {
    listPoses();
  } else if (!strcmp(cmd, "pose") && hasArg) {
    furby::Pose p;
    if (!furby::poseFromName(arg, p)) {
      Serial.println(F("unknown pose (see `poses`)"));
    } else {
      manualMode();
      if (!body.moveTo(p)) Serial.println(F("not homed - run `home` first"));
    }
  } else if (!strcmp(cmd, "setpose") && hasArg) {
    char* val = strchr(arg, ' ');
    if (val) *val++ = '\0';
    furby::Pose p;
    if (!furby::poseFromName(arg, p)) {
      Serial.println(F("unknown pose (see `poses`)"));
    } else {
      int32_t c = val ? atol(val) : body.position();
      body.poses().set(p, furby::wrap(c, body.countsPerRev()));
      Serial.printf("%s = %ld (not saved until `save`)\n", arg, (long)body.poses().get(p));
    }
  } else if (!strcmp(cmd, "resetposes")) {
    body.poses().setDefaults(body.countsPerRev());
    listPoses();
  } else if (!strcmp(cmd, "anims")) {
    for (uint8_t i = 0; i < furby::anim::kAllCount; ++i)
      Serial.printf("  %s\n", furby::anim::kAll[i]->name);
  } else if (!strcmp(cmd, "anim") && hasArg) {
    const furby::Animation* a = furby::anim::find(arg);
    if (!a) {
      Serial.println(F("unknown animation (see `anims`)"));
    } else if (!body.homed()) {
      Serial.println(F("not homed - run `home` first"));
    } else {
      playAnimation(a);
    }
  } else if (!strcmp(cmd, "say") && hasArg) {
    furby::Sound s;
    if (parseSound(arg, s)) audio::play(s);
    else Serial.println(F("unknown sound"));
  } else if (!strcmp(cmd, "play") && hasArg) {
    if (!audio::playFile(arg)) Serial.println(F("no such file (see `ls`)"));
  } else if (!strcmp(cmd, "ls")) {
    File dir = LittleFS.open("/sounds");
    if (!dir || !dir.isDirectory()) {
      Serial.println(F("no /sounds directory (upload with `pio run -t uploadfs`)"));
    } else {
      for (File f = dir.openNextFile(); f; f = dir.openNextFile())
        Serial.printf("  /sounds/%s  %u bytes\n", f.name(), (unsigned)f.size());
    }
  } else if (!strcmp(cmd, "vol") && hasArg) {
    audio::setVolume(constrain(atoi(arg), 0, 100));
    Serial.printf("volume %u\n", audio::volume());
  } else if (!strcmp(cmd, "auto")) {
    autonomous = strcmp(arg, "off") != 0;
    Serial.printf("autonomous %s\n", autonomous ? "on" : "off");
  } else if (!strcmp(cmd, "sleep")) {
    brain.sleep();
  } else if (!strcmp(cmd, "wake")) {
    brain.wake(millis());
  } else if (!strcmp(cmd, "save")) {
    saveSettings();
    Serial.println(F("saved"));
  } else if (*cmd) {
    Serial.printf("unknown command '%s' - try `help`\n", cmd);
  }
}

}  // namespace

void consoleBegin() {
  Serial.println(F("\nFurby ESP32 brain. Type `help`."));
}

void consoleUpdate() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (len_ == 0) continue;
      line_[len_] = '\0';
      len_ = 0;
      Serial.printf("> %s\n", line_);
      run(line_);
    } else if (c == 8 || c == 127) {
      if (len_) --len_;
    } else if (len_ < sizeof(line_) - 1) {
      line_[len_++] = c;
    }
  }
}
