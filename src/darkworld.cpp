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
  enum LightIds  {
                  DARK_LIGHT,
                  WORLD_LIGHT,
                  NUM_LIGHTS
  };

  RRMidiOutput midi_out;
  float rate_limiter_phase = 0.f;

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

    // bypass buttons
    configParam(BYPASS_DARK_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Dark");
    configParam(BYPASS_WORLD_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass World");
  }

  void process(const ProcessArgs& args) override {
    // only proceed if a midi channel is set
    if (midi_out.channel <= 0)
      return;

    // read the bypass button values
    int enable_dark = (int) floor(params[BYPASS_DARK_PARAM].getValue());
    int enable_world = (int) floor(params[BYPASS_WORLD_PARAM].getValue());

    int bypass;
    if (enable_world && enable_dark) {
      // world LED red (on)
      // dark LED red (on)
      lights[WORLD_LIGHT].setBrightness(1.f);
      lights[DARK_LIGHT].setBrightness(1.f);
      bypass = 127;
    } else if (!enable_world && enable_dark) {
      // world LED off
      // dark LED red (on)
      lights[WORLD_LIGHT].setBrightness(0.f);
      lights[DARK_LIGHT].setBrightness(1.f);
      bypass = 85;
    } else if (enable_world && !enable_dark) {
      // world LED red (on)
      // dark LED off
      lights[WORLD_LIGHT].setBrightness(1.f);
      lights[DARK_LIGHT].setBrightness(0.f);
      bypass = 45;
    } else {
      // world LED off
      // dark LED off
      lights[WORLD_LIGHT].setBrightness(0.f);
      lights[DARK_LIGHT].setBrightness(0.f);
      bypass = 0;
    }

    // bypass the dark and/or world channels
    midi_out.setValue(bypass, 103);

    // read the three-way switch values
    int dark_prog = (int) floor(params[DARK_PROGRAM_PARAM].getValue());
    int route_prog = (int) floor(params[ROUTING_PARAM].getValue());
    int world_prog = (int) floor(params[WORLD_PROGRAM_PARAM].getValue());

    // assign values from switches
    midi_out.setValue(dark_prog, 21);
    midi_out.setValue(route_prog, 22);
    midi_out.setValue(world_prog, 23);

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the CV inputs.
    const float rate_limiter_period = 0.005f;
    rate_limiter_phase += args.sampleTime / rate_limiter_period;
    if (rate_limiter_phase >= 1.f) {
      // reduce the phase and proceed
      rate_limiter_phase -= 1.f;
    } else {
      // skip this process iteration
      return;
    }

    // knob values
    int decay = (int) std::round(params[DECAY_PARAM].getValue());
    int mix = (int) std::round(params[MIX_PARAM].getValue());
    int dwell = (int) std::round(params[DWELL_PARAM].getValue());
    int modify = (int) std::round(params[MODIFY_PARAM].getValue());
    int tone = (int) std::round(params[TONE_PARAM].getValue());
    int pre_delay = (int) std::round(params[PRE_DELAY_PARAM].getValue());

    // read cv voltages and override values of knobs,
    // clamp down the cv value to be between 0 and the value of the knob
    if (inputs[DECAY_INPUT].isConnected()) {
      int decay_cv = (int) std::round(inputs[DECAY_INPUT].getVoltage()*2) / 10.f * 127;
      decay = clamp(decay_cv, 0, decay);
    }
    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = (int) std::round(inputs[MIX_INPUT].getVoltage()*2) / 10.f * 127;
      mix = clamp(mix_cv, 0, mix);
    }
    if (inputs[DWELL_INPUT].isConnected()) {
      int dwell_cv = (int) std::round(inputs[DWELL_INPUT].getVoltage()*2) / 10.f * 127;
      dwell = clamp(dwell_cv, 0, dwell);
    }
    if (inputs[TONE_INPUT].isConnected()) {
      int tone_cv = (int) std::round(inputs[TONE_INPUT].getVoltage()*2) / 10.f * 127;
      tone = clamp(tone_cv, 0, tone);
    }
    if (inputs[MODIFY_INPUT].isConnected()) {
      int modify_cv = (int) std::round(inputs[MODIFY_INPUT].getVoltage()*2) / 10.f * 127;
      modify = clamp(modify_cv, 0, modify);
    }
    if (inputs[PRE_DELAY_INPUT].isConnected()) {
      int pre_delay_cv = (int) std::round(inputs[PRE_DELAY_INPUT].getVoltage()*2) / 10.f * 127;
      pre_delay = clamp(pre_delay_cv, 0, pre_delay);
    }

    // assign values from knobs (or cv)
    midi_out.setValue(decay, 14);
    midi_out.setValue(mix, 15);
    midi_out.setValue(dwell, 16);
    midi_out.setValue(modify, 17);
    midi_out.setValue(tone, 18);
    midi_out.setValue(pre_delay, 19);
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
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 12)), module, Darkworld::DECAY_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 12)), module, Darkworld::MIX_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 12)), module, Darkworld::DWELL_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 40)), module, Darkworld::MODIFY_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 40)), module, Darkworld::TONE_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 40)), module, Darkworld::PRE_DELAY_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 25)), module, Darkworld::DECAY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 25)), module, Darkworld::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 25)), module, Darkworld::DWELL_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 53)), module, Darkworld::MODIFY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 53)), module, Darkworld::TONE_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 53)), module, Darkworld::PRE_DELAY_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 66)), module, Darkworld::DARK_PROGRAM_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(30, 66)), module, Darkworld::ROUTING_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(50, 66)), module, Darkworld::WORLD_PROGRAM_PARAM));

    // bypass switches
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(15, 109)), module, Darkworld::DARK_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(15, 118)), module, Darkworld::BYPASS_DARK_PARAM));

    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(46, 109)), module, Darkworld::WORLD_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(46, 118)), module, Darkworld::BYPASS_WORLD_PARAM));

    // midi configuration displays
    MidiWidget* midiWidget = createWidget<MidiWidget>(mm2px(Vec(6, 75)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};


Model* modelDarkworld = createModel<Darkworld, DarkworldWidget>("darkworld");
