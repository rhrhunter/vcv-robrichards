#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "midi_classes.hpp"

struct Darkworld : Module {
  enum ParamIds {
                 DECAY_PARAM,
                 MIX_PARAM,
                 DWELL_PARAM,
                 MODIFY_PARAM,
                 TONE_PARAM,
                 PRE_DELAY_PARAM,
                 DARK_PROGRAM_PARAM,
                 ROUTING_PARAM,
                 WORLD_PROGRAM_PARAM,
                 MIDI_CHANNEL_PARAM,
                 BYPASS_DARK_PARAM,
                 BYPASS_WORLD_PARAM,
                 NUM_PARAMS
  };
  enum InputIds  {
                  DECAY_INPUT,
                  MIX_INPUT,
                  DWELL_INPUT,
                  MODIFY_INPUT,
                  TONE_INPUT,
                  PRE_DELAY_INPUT,
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  { NUM_LIGHTS };

  RRMidiOutput midi_out;

  Darkworld() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(DECAY_PARAM, 0.f, 127.f, 0.f, "Decay");
    configParam(MIX_PARAM, 0.f, 127.f, 0.f, "Mix");
    configParam(DWELL_PARAM, 0.f, 127.f, 0.f, "Dwell");
    configParam(MODIFY_PARAM, 0.f, 127.f, 0.f, "Modify");
    configParam(TONE_PARAM, 0.f, 127.f, 0.f, "Tone");
    configParam(PRE_DELAY_PARAM, 0.f, 127.f, 0.f, "Pre-Delay");

    // three way switches
    // 0.0f is top position
    configParam(DARK_PROGRAM_PARAM, 1.0f, 3.0f, 2.0f, "Dark Program (Mod, Shim, Black)");
    configParam(ROUTING_PARAM, 1.0f, 3.0f, 2.0f, "Routing Mode (Parallel, D>>W, W>>D)");
    configParam(WORLD_PROGRAM_PARAM, 1.0f, 3.0f, 2.0f, "World Program (Hall, Plate, Spring)");

    // midi configuration knobs
    configParam(MIDI_CHANNEL_PARAM, 1.f, 16.f, 2.f, "MIDI Channel");

    // bypass buttons
    configParam(BYPASS_DARK_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Dark");
    configParam(BYPASS_WORLD_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass World");

    // DeviceIds start counting from 0, not 1
    midi_out.setDeviceId(3);
    midi_out.setChannel(2);
  }

  void process(const ProcessArgs& args) override {
    // configure the midi channel, return if it is not set
    int channel = (int) floor(params[MIDI_CHANNEL_PARAM].getValue() + 0.5);
    if (channel <= 0)
      return;
    else
      midi_out.setChannel(channel);

    // knob values
    int decay = (int) floor(params[DECAY_PARAM].getValue() + 0.5);
    int mix = (int) floor(params[MIX_PARAM].getValue() + 0.5);
    int dwell = (int) floor(params[DWELL_PARAM].getValue() + 0.5);
    int modify = (int) floor(params[MODIFY_PARAM].getValue() + 0.5);
    int tone = (int) floor(params[TONE_PARAM].getValue() + 0.5);
    int pre_delay = (int) floor(params[PRE_DELAY_PARAM].getValue() + 0.5);

    // switch values
    int dark_prog = (int) floor(params[DARK_PROGRAM_PARAM].getValue());
    int route_prog = (int) floor(params[ROUTING_PARAM].getValue());
    int world_prog = (int) floor(params[WORLD_PROGRAM_PARAM].getValue());

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[DECAY_INPUT].isConnected()) {
      int decay_cv = (int) floor((inputs[DECAY_INPUT].getVoltage() * 16.9f));
      if (decay_cv > decay)
        decay_cv = decay;
      decay = decay_cv;
    }

    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = (int) floor((inputs[MIX_INPUT].getVoltage() * 16.9f));
      if (mix_cv > mix)
        mix_cv = mix;
      mix = mix_cv;
    }

    if (inputs[DWELL_INPUT].isConnected()) {
      int dwell_cv = (int) floor((inputs[DWELL_INPUT].getVoltage() * 16.9f));
      if (dwell_cv > dwell)
        dwell_cv = dwell;
      dwell = dwell_cv;
    }

    if (inputs[TONE_INPUT].isConnected()) {
      int tone_cv = (int) floor((inputs[TONE_INPUT].getVoltage() * 16.9f));
      if (tone_cv > tone)
        tone_cv = tone;
      tone = tone_cv;
    }

    if (inputs[MODIFY_INPUT].isConnected()) {
      int modify_cv = (int) floor((inputs[MODIFY_INPUT].getVoltage() * 16.9f));
      if (modify_cv > modify)
        modify_cv = modify;
      modify = modify_cv;
    }

    if (inputs[PRE_DELAY_INPUT].isConnected()) {
      int pre_delay_cv = (int) floor((inputs[PRE_DELAY_INPUT].getVoltage() * 16.9f));
      if (pre_delay_cv > pre_delay)
        pre_delay_cv = pre_delay;
      pre_delay = pre_delay_cv;
    }

    int enable_dark = (int) floor(params[BYPASS_DARK_PARAM].getValue());
    int enable_world = (int) floor(params[BYPASS_WORLD_PARAM].getValue());

    int bypass;
    if (enable_world && enable_dark) {
      bypass = 127;
    } else if (!enable_world && enable_dark) {
      bypass = 85;
    } else if (enable_world && !enable_dark) {
      bypass = 45;
    } else {
      bypass = 0;
    }

    // assign values from knobs (or cv)
    midi_out.setValue(decay, 14);
    midi_out.setValue(mix, 15);
    midi_out.setValue(dwell, 16);
    midi_out.setValue(modify, 17);
    midi_out.setValue(tone, 18);
    midi_out.setValue(pre_delay, 19);

    // assign values from switches
    midi_out.setValue(dark_prog, 21);
    midi_out.setValue(route_prog, 22);
    midi_out.setValue(world_prog, 23);

    // bypass the dark and/or world channels
    midi_out.setValue(bypass, 103);
  }
};


struct DarkworldWidget : ModuleWidget {
  DarkworldWidget(Darkworld* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/darkworld_blank.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 15)), module, Darkworld::DECAY_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 15)), module, Darkworld::MIX_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 15)), module, Darkworld::DWELL_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 50)), module, Darkworld::MODIFY_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 50)), module, Darkworld::TONE_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 50)), module, Darkworld::PRE_DELAY_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 30)), module, Darkworld::DECAY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 30)), module, Darkworld::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 30)), module, Darkworld::DWELL_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 65)), module, Darkworld::MODIFY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 65)), module, Darkworld::TONE_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 65)), module, Darkworld::PRE_DELAY_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 80)), module, Darkworld::DARK_PROGRAM_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(30, 80)), module, Darkworld::ROUTING_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(50, 80)), module, Darkworld::WORLD_PROGRAM_PARAM));

    // bypass switches
    addParam(createParamCentered<CBAButtonRed>(mm2px(Vec(15, 118)), module, Darkworld::BYPASS_DARK_PARAM));
    addParam(createParamCentered<CBAButtonRed>(mm2px(Vec(46, 118)), module, Darkworld::BYPASS_WORLD_PARAM));

    // midi configuration displays
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 100)), module, Darkworld::MIDI_CHANNEL_PARAM));
    MidiChannelDisplay *mcd = new MidiChannelDisplay();
    mcd->box.pos = Vec(50, 285);
    mcd->box.size = Vec(50, 20);
    mcd->value = &((module->midi_out).midi_channel);
    mcd->module = (void *) module;
    addChild(mcd);

  }
};


Model* modelDarkworld = createModel<Darkworld, DarkworldWidget>("darkworld");
