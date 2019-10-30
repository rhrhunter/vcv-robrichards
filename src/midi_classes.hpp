#pragma once

#include <midi.hpp>

using namespace std;

namespace rack {

struct RRMidiOutput : dsp::MidiGenerator<PORT_MAX_CHANNELS>, midi::Output {
  int lastMidiCCValues[128];
  int midi_device_id;
  int midi_channel;

  RRMidiOutput() {
    midi_device_id = -1;
    midi_channel = 0;
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
    if (midi_device_id != id) {
      midi::Output::setDeviceId(id);
      midi_device_id = id;
      std::string dev_name = getDeviceName(id);
      INFO("Set dev id to %d (%s)", id, dev_name.c_str());
      //std::vector<int> ids = getDeviceIds();
      //for(unsigned long i = 0; i < ids.size(); i++){
      //  std::string dev_name = getDeviceName(i);
      //  INFO("id: %d, device: %s", i, dev_name.c_str());
      //}
      reset();
    }
  }

  void setChannel(int channel) {
    // only update the channel if it changed
    if (midi_channel != channel) {
      // midi channels internally start counting at 0, so subtract one
      midi::Port::setChannel(channel-1);
      midi_channel = channel;
      INFO("Set channel to %d", channel);
      reset();
    }
  }

  void setValue(int value, int cc) {
    if (value == lastMidiCCValues[cc]) {
      return;
    }
    lastMidiCCValues[cc] = value;
    INFO("Setting cc:%d to value:%d", cc, value);

    // CC
    midi::Message m;
    m.setStatus(0xb);
    m.setNote(cc);
    m.setValue(value);
    sendMessage(m);
  }
};

}
