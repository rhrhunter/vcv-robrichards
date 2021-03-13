#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"

struct P6MPE : RRModule {
  enum ParamIds { NUM_PARAMS };
  enum InputIds  {
    MPE1_YAXIS_INPUT,
    MPE1_PWHEEL_INPUT,
    MPE2_YAXIS_INPUT,
    MPE2_PWHEEL_INPUT,
    MPE3_YAXIS_INPUT,
    MPE3_PWHEEL_INPUT,
    MPE4_YAXIS_INPUT,
    MPE4_PWHEEL_INPUT,
    MPE5_YAXIS_INPUT,
    MPE5_PWHEEL_INPUT,
    MPE6_YAXIS_INPUT,
    MPE6_PWHEEL_INPUT,
    NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  { NUM_LIGHTS };

  RRMidiOutput midiout_mpe1;
  RRMidiOutput midiout_mpe2;
  RRMidiOutput midiout_mpe3;
  RRMidiOutput midiout_mpe4;
  RRMidiOutput midiout_mpe5;
  RRMidiOutput midiout_mpe6;

  P6MPE() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // hardcode the channels
    midiout_mpe1.setChannel(1);
    midiout_mpe2.setChannel(2);
    midiout_mpe3.setChannel(3);
    midiout_mpe4.setChannel(4);
    midiout_mpe5.setChannel(5);
    midiout_mpe6.setChannel(6);

    // hardcode the device IDs
    midiout_mpe1.setDeviceId(2);
    midiout_mpe2.setDeviceId(2);
    midiout_mpe3.setDeviceId(2);
    midiout_mpe4.setDeviceId(2);
    midiout_mpe5.setDeviceId(2);
    midiout_mpe6.setDeviceId(2);
  }

  void process(const ProcessArgs& args) override {
    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the the user.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // read the cv ports and relay to the appropriate midi channel

    // Channel 1
    if (inputs[MPE1_YAXIS_INPUT].isConnected()) {
      int mpe1_yaxis = clamp(convertCVtoCC(inputs[MPE1_YAXIS_INPUT].getVoltage()), 0, 127);
      midiout_mpe1.sendCachedCC(mpe1_yaxis, 74);
    }
    if (inputs[MPE1_PWHEEL_INPUT].isConnected()) {
      int pw = (int) std::round((inputs[MPE1_PWHEEL_INPUT].getVoltage() + 5.f) / 10.f * 0x4000);
      int mpe1_pwheel = clamp(pw, 0, 0x3fff);
      midiout_mpe1.setPitchWheel(mpe1_pwheel);
    }

    // Channel 2
    if (inputs[MPE2_YAXIS_INPUT].isConnected()) {
      int mpe2_yaxis = clamp(convertCVtoCC(inputs[MPE2_YAXIS_INPUT].getVoltage()), 0, 127);
      midiout_mpe2.sendCachedCC(mpe2_yaxis, 74);
    }
    if (inputs[MPE2_PWHEEL_INPUT].isConnected()) {
      int pw = (int) std::round((inputs[MPE2_PWHEEL_INPUT].getVoltage() + 5.f) / 10.f * 0x4000);
      int mpe2_pwheel = clamp(pw, 0, 0x3fff);
      midiout_mpe2.setPitchWheel(mpe2_pwheel);
    }

    // Channel 3
    if (inputs[MPE3_YAXIS_INPUT].isConnected()) {
      int mpe3_yaxis = clamp(convertCVtoCC(inputs[MPE3_YAXIS_INPUT].getVoltage()), 0, 127);
      midiout_mpe3.sendCachedCC(mpe3_yaxis, 74);
    }
    if (inputs[MPE3_PWHEEL_INPUT].isConnected()) {
      int pw = (int) std::round((inputs[MPE3_PWHEEL_INPUT].getVoltage() + 5.f) / 10.f * 0x4000);
      int mpe3_pwheel = clamp(pw, 0, 0x3fff);
      midiout_mpe3.setPitchWheel(mpe3_pwheel);
    }

    // Channel 4
    if (inputs[MPE4_YAXIS_INPUT].isConnected()) {
      int mpe4_yaxis = clamp(convertCVtoCC(inputs[MPE4_YAXIS_INPUT].getVoltage()), 0, 127);
      midiout_mpe4.sendCachedCC(mpe4_yaxis, 74);
    }
    if (inputs[MPE4_PWHEEL_INPUT].isConnected()) {
      int pw = (int) std::round((inputs[MPE4_PWHEEL_INPUT].getVoltage() + 5.f) / 10.f * 0x4000);
      int mpe4_pwheel = clamp(pw, 0, 0x3fff);
      midiout_mpe4.setPitchWheel(mpe4_pwheel);
    }

    // Channel 5
    if (inputs[MPE5_YAXIS_INPUT].isConnected()) {
      int mpe5_yaxis = clamp(convertCVtoCC(inputs[MPE5_YAXIS_INPUT].getVoltage()), 0, 127);
      midiout_mpe5.sendCachedCC(mpe5_yaxis, 74);
    }
    if (inputs[MPE5_PWHEEL_INPUT].isConnected()) {
      int pw = (int) std::round((inputs[MPE5_PWHEEL_INPUT].getVoltage() + 5.f) / 10.f * 0x4000);
      int mpe5_pwheel = clamp(pw, 0, 0x3fff);
      midiout_mpe5.setPitchWheel(mpe5_pwheel);
    }

    // Channel 6
    if (inputs[MPE6_YAXIS_INPUT].isConnected()) {
      int mpe6_yaxis = clamp(convertCVtoCC(inputs[MPE6_YAXIS_INPUT].getVoltage()), 0, 127);
      midiout_mpe6.sendCachedCC(mpe6_yaxis, 74);
    }
    if (inputs[MPE6_PWHEEL_INPUT].isConnected()) {
      int pw = (int) std::round((inputs[MPE6_PWHEEL_INPUT].getVoltage() + 5.f) / 10.f * 0x4000);
      int mpe6_pwheel = clamp(pw, 0, 0x3fff);
      midiout_mpe6.setPitchWheel(mpe6_pwheel);
    }

    return;
  }
};

struct P6MPEWidget : ModuleWidget {
  P6MPEWidget(P6MPE* module) {
    setModule(module);

    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/core.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-10, 1)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 10, 1)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-10, RACK_GRID_HEIGHT - RACK_GRID_WIDTH - 1)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 10 , RACK_GRID_HEIGHT - RACK_GRID_WIDTH - 1)));

    // CV inputs
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 10)), module, P6MPE::MPE1_YAXIS_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(25, 10)), module, P6MPE::MPE1_PWHEEL_INPUT));

    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 30)), module, P6MPE::MPE2_YAXIS_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(25, 30)), module, P6MPE::MPE2_PWHEEL_INPUT));

    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 50)), module, P6MPE::MPE3_YAXIS_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(25, 50)), module, P6MPE::MPE3_PWHEEL_INPUT));

    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 70)), module, P6MPE::MPE4_YAXIS_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(25, 70)), module, P6MPE::MPE4_PWHEEL_INPUT));

    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 90)), module, P6MPE::MPE5_YAXIS_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(25, 90)), module, P6MPE::MPE5_PWHEEL_INPUT));

    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 110)), module, P6MPE::MPE6_YAXIS_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(25, 110)), module, P6MPE::MPE6_PWHEEL_INPUT));

  }
};

Model* modelP6MPE = createModel<P6MPE, P6MPEWidget>("p6mpe");
