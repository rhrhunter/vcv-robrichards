#pragma once

#include <midi.hpp>

using namespace std;

namespace rack {

struct RRMidiOutput : dsp::MidiGenerator<PORT_MAX_CHANNELS>, midi::Output {
  int lastMidiCCValues[128];

  RRMidiOutput() {
    reset();
  }

  void reset() {
    // clean up the cache of CC values
    for (int n = 0; n < 128; n++) {
      lastMidiCCValues[n] = -1;
    }
    MidiGenerator::reset();
  }

  void onMessage(midi::Message message) override {
    midi::Output::sendMessage(message);
  }

  void setDeviceId(int id) override {
    // only update the channel if it changed
    if (deviceId != id) {
      midi::Output::setDeviceId(id);
      std::string dev_name = getDeviceName(id);
      reset();
    }
  }

  void resetCCCache(int cc) {
    lastMidiCCValues[cc] = -1;
  }

  int getCachedCCValue(int cc) {
    return lastMidiCCValues[cc];
  }

  bool active() {
    return deviceId > -1 && channel > -1;
  }

  bool setValue(int value, int cc) {
    // check the cache for cc messages
    if (value == lastMidiCCValues[cc]) {
      return false;
    }
    lastMidiCCValues[cc] = value;

    // send CC message
    midi::Message m;
    m.setStatus(0xb);
    m.setNote(cc);
    m.setValue(value);
    sendMessage(m);
    return true;
  }

  bool setValueNoCache(int value, int cc) {
    // send CC message
    midi::Message m;
    m.setStatus(0xb);
    m.setNote(cc);
    m.setValue(value);
    sendMessage(m);
    return true;
  }
};

}
