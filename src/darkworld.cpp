#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"
#include "rr_midiwidget.hpp"

struct Darkworld : RRModule {
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
                  EXPR_INPUT,
                  BYPASS_DARK_INPUT_LOW,
                  BYPASS_DARK_INPUT_HIGH,
                  BYPASS_WORLD_INPUT_LOW,
                  BYPASS_WORLD_INPUT_HIGH,
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  DARK_LIGHT,
                  WORLD_LIGHT,
                  NUM_LIGHTS
  };

  dsp::SchmittTrigger dark_trigger_low, dark_trigger_high, world_trigger_low, world_trigger_high;

  Darkworld() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(DECAY_PARAM, 0.f, 127.f, 0.f, "Decay");
    configParam(MIX_PARAM, 0.f, 127.f, 0.f, "Mix");
    configParam(DWELL_PARAM, 0.f, 127.f, 0.f, "Dwell");
    configParam(MODIFY_PARAM, 0.f, 127.f, 0.f, "Modify");
    configParam(TONE_PARAM, 0.f, 127.f, 64.f, "Tone");
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
    // only proceed if midi is activated
    if (!midi_out.active()) {
      if (!disable_module()) {
        // turn off the lights if the module is not disabled
        lights[DARK_LIGHT].setBrightness(0.f);
        lights[WORLD_LIGHT].setBrightness(0.f);
      }
      return;
    } else {
      // enable_module
      enable_module();
    }

    // read the gate triggers
    if (inputs[BYPASS_DARK_INPUT_HIGH].isConnected()) {
      if (dark_trigger_high.process(rescale(inputs[BYPASS_DARK_INPUT_HIGH].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
        // if the trigger goes high, turn on the dark channel
        params[BYPASS_DARK_PARAM].setValue(1.f);
    }
    if (inputs[BYPASS_DARK_INPUT_LOW].isConnected()) {
      if (dark_trigger_low.process(rescale(inputs[BYPASS_DARK_INPUT_LOW].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
        // if the trigger goes low, turn off the dark channel
        params[BYPASS_DARK_PARAM].setValue(0.f);
    }
    if (inputs[BYPASS_WORLD_INPUT_HIGH].isConnected()) {
      if (world_trigger_high.process(rescale(inputs[BYPASS_WORLD_INPUT_HIGH].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
        // if the trigger goes high, turn on the world channel
        params[BYPASS_WORLD_PARAM].setValue(1.f);
    }
    if (inputs[BYPASS_WORLD_INPUT_LOW].isConnected()) {
      if (world_trigger_low.process(rescale(inputs[BYPASS_WORLD_INPUT_LOW].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
        // if the trigger goes low, turn off the world channel
        params[BYPASS_WORLD_PARAM].setValue(0.f);
    }

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
    midi_out.sendCachedCC(bypass, 103);

    // read the three-way switch values
    int dark_prog = (int) floor(params[DARK_PROGRAM_PARAM].getValue());
    int route_prog = (int) floor(params[ROUTING_PARAM].getValue());
    int world_prog = (int) floor(params[WORLD_PROGRAM_PARAM].getValue());

    // assign values from switches
    midi_out.sendCachedCC(dark_prog, 21);
    midi_out.sendCachedCC(route_prog, 22);
    midi_out.sendCachedCC(world_prog, 23);

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the CV inputs.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // knob values
    int decay = (int) std::round(params[DECAY_PARAM].getValue());
    int mix = (int) std::round(params[MIX_PARAM].getValue());
    int dwell = (int) std::round(params[DWELL_PARAM].getValue());
    int modify = (int) std::round(params[MODIFY_PARAM].getValue());
    int tone = (int) std::round(params[TONE_PARAM].getValue());
    int pre_delay = (int) std::round(params[PRE_DELAY_PARAM].getValue());
    int expr = -1;

    // read cv voltages and override values of knobs,
    // clamp down the cv value to be between 0 and the value of the knob
    if (inputs[DECAY_INPUT].isConnected()) {
      int decay_cv = convertCVtoCC(inputs[DECAY_INPUT].getVoltage());
      decay = clamp(decay_cv, 0, decay);
    }
    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = convertCVtoCC(inputs[MIX_INPUT].getVoltage());
      mix = clamp(mix_cv, 0, mix);
    }
    if (inputs[DWELL_INPUT].isConnected()) {
      int dwell_cv = convertCVtoCC(inputs[DWELL_INPUT].getVoltage());
      dwell = clamp(dwell_cv, 0, dwell);
    }
    if (inputs[TONE_INPUT].isConnected()) {
      int tone_cv = convertCVtoCC(inputs[TONE_INPUT].getVoltage());
      tone = clamp(tone_cv, 0, tone);
    }
    if (inputs[MODIFY_INPUT].isConnected()) {
      int modify_cv = convertCVtoCC(inputs[MODIFY_INPUT].getVoltage());
      modify = clamp(modify_cv, 0, modify);
    }
    if (inputs[PRE_DELAY_INPUT].isConnected()) {
      int pre_delay_cv = convertCVtoCC(inputs[PRE_DELAY_INPUT].getVoltage());
      pre_delay = clamp(pre_delay_cv, 0, pre_delay);
    }
    if (inputs[EXPR_INPUT].isConnected()) {
      int expr_cv = convertCVtoCC(inputs[EXPR_INPUT].getVoltage());
      expr = clamp(expr_cv, 0, 127);
    }

    // assign values from knobs (or cv)
    midi_out.sendCachedCC(decay, 14);
    midi_out.sendCachedCC(mix, 15);
    midi_out.sendCachedCC(dwell, 16);
    midi_out.sendCachedCC(modify, 17);
    midi_out.sendCachedCC(tone, 18);
    midi_out.sendCachedCC(pre_delay, 19);

    // assign value for expression
    if (expr > 0)
      midi_out.sendCachedCC(expr, 100);

    return;
  }
};

struct DarkworldWidget : ModuleWidget {
  DarkworldWidget(Darkworld* module) {
    setModule(module);

#ifdef USE_LOGOS
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/darkworld_panel_logo.svg")));
#else
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/darkworld_panel.svg")));
#endif

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnobDW>(mm2px(Vec(10, 12)), module, Darkworld::DECAY_PARAM));
    addParam(createParamCentered<CBAKnobDW>(mm2px(Vec(30, 12)), module, Darkworld::MIX_PARAM));
    addParam(createParamCentered<CBAKnobDW>(mm2px(Vec(50, 12)), module, Darkworld::DWELL_PARAM));
    addParam(createParamCentered<CBAKnobDW>(mm2px(Vec(10, 40)), module, Darkworld::MODIFY_PARAM));
    addParam(createParamCentered<CBAKnobDW>(mm2px(Vec(30, 40)), module, Darkworld::TONE_PARAM));
    addParam(createParamCentered<CBAKnobDW>(mm2px(Vec(50, 40)), module, Darkworld::PRE_DELAY_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 25)), module, Darkworld::DECAY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 25)), module, Darkworld::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 25)), module, Darkworld::DWELL_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 53)), module, Darkworld::MODIFY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 53)), module, Darkworld::TONE_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 53)), module, Darkworld::PRE_DELAY_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(7, 66)), module, Darkworld::DARK_PROGRAM_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(27, 66)), module, Darkworld::ROUTING_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(47, 66)), module, Darkworld::WORLD_PROGRAM_PARAM));

    // dark channel led / bypass / high & low gate
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(15, 109)), module, Darkworld::DARK_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(15, 118)), module, Darkworld::BYPASS_DARK_PARAM));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25, 109)), module, Darkworld::BYPASS_DARK_INPUT_HIGH));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25, 118)), module, Darkworld::BYPASS_DARK_INPUT_LOW));

    // world channel led / bypass / high & low gate
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(46, 109)), module, Darkworld::WORLD_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(46, 118)), module, Darkworld::BYPASS_WORLD_PARAM));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 109)), module, Darkworld::BYPASS_WORLD_INPUT_HIGH));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 118)), module, Darkworld::BYPASS_WORLD_INPUT_LOW));

    // expression port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(43.5, 92)), module, Darkworld::EXPR_INPUT));

    // midi configuration displays
    RRMidiWidget* midiWidget = createWidget<RRMidiWidget>(mm2px(Vec(3, 75)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};

Model* modelDarkworld = createModel<Darkworld, DarkworldWidget>("darkworld");
