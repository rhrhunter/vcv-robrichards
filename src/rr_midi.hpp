#pragma once

#include <midi.hpp>

using namespace std;

namespace rack {

struct RRMidiOutput : dsp::MidiGenerator<PORT_MAX_CHANNELS>, midi::Output {
  int lastMidiCCValues[128];
  int currProgram;

  RRMidiOutput() {
    reset();
  }

  void reset() {
    // clean up the cache of CC values
    for (int n = 0; n < 128; n++) {
      lastMidiCCValues[n] = -1;
    }
    currProgram = -1;
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

  void incrementProgram(int incrby, int max) {
    // incr the current program modded by the upper limit
    if (currProgram == max)
      currProgram = 0;
    int value = (currProgram + incrby) % max;
    setProgram(value);
  }

  void decrementProgram(int decrby, int max) {
    // decr the current program modded by the upper limit
    if (currProgram == 0)
      currProgram = max;
    int value = (currProgram - decrby) % max;
    setProgram(value);
  }

  bool setProgram(int value) {
    // check the cache to see if we are already on this program
    if (currProgram == value) {
      return false;
    } else {
      currProgram = value;
    }

    // send a Program change message (2 bytes) with -
    // 0x0c as the status byte
    midi::Message m;
    m.setSize(2);
    m.setStatus(0xc);
    m.setNote(value);
    DEBUG("Preamp MKII program change: %d", value);
    sendMessage(m);
    return true;
  }
};

}
