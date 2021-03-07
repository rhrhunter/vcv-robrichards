#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"
#include "rr_midiwidget.hpp"
#include <dsp/digital.hpp>

struct Cxm1978 : RRModule {
  enum ParamIds {
                 BASS_SLIDER_PARAM,
                 MIDS_SLIDER_PARAM,
                 CROSS_SLIDER_PARAM,
                 TREBLE_SLIDER_PARAM,
                 MIX_SLIDER_PARAM,
                 PREDLY_SLIDER_PARAM,
                 JUMP_ARCADE_PARAM,
                 TYPE_ARCADE_PARAM,
                 DIFFUSION_ARCADE_PARAM,
                 TANK_MOD_ARCADE_PARAM,
                 CLOCK_ARCADE_PARAM,
                 CHANGE_PRESET_PARAM,
                 BYPASS_PARAM,
                 NUM_PARAMS
  };
  enum InputIds  {
                  PRESET_INPUT,
                  BYPASS_INPUT,
                  EXPR_INPUT,
                  BASS_SLIDER_INPUT,
                  MIDS_SLIDER_INPUT,
                  CROSS_SLIDER_INPUT,
                  TREBLE_SLIDER_INPUT,
                  MIX_SLIDER_INPUT,
                  PREDLY_SLIDER_INPUT,
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  BYPASS_LIGHT,
                  NUM_LIGHTS
  };

  // grace period timevals
  struct timeval preset_change_grace_period;

  Cxm1978() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main slider parameters
    configParam(BASS_SLIDER_PARAM, 0.f, 127.f, 0.f, "Bass (Decay Time Below Crossover)");
    configParam(MIDS_SLIDER_PARAM, 0.f, 127.f, 0.f, "Mids (Decay Time Above Crossover)");
    configParam(CROSS_SLIDER_PARAM, 0.f, 127.f, 0.f, "Crossover Frequency (Bass<->Mid)");
    configParam(TREBLE_SLIDER_PARAM, 0.f, 127.f, 0.f, "Treble");
    configParam(MIX_SLIDER_PARAM, 0.f, 127.f, 0.f, "Mix (Wey/Dry)");
    configParam(PREDLY_SLIDER_PARAM, 0.f, 127.f, 0.f, "Pre-Delay");

    // arcade buttons
    // 1.0f is the black text
    // 2.0f is the blue text
    // 3.0f is the red text
    configParam(JUMP_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Preset Jump (Off, 1, 5)");
    configParam(TYPE_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Reverb Type (Room, Plate, Hall)");
    configParam(DIFFUSION_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Diffusion Level (Low, Medium, High)");
    configParam(TANK_MOD_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Tank Modulation (Low, Medium, High)");
    configParam(CLOCK_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Pre-Delay Clock (Hifi, Standard, Lofi)");

    // bypass buttons
    configParam(CHANGE_PRESET_PARAM, 0.f, 1.f, 0.f, "Change Preset");
    configParam(BYPASS_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Pedal");

    // initialize the first preset
    midi_out.setProgram(0);

    // initialize the loop select grace period
    gettimeofday(&preset_change_grace_period, NULL);
  }

  void process(const ProcessArgs& args) override {
    // only proceed if midi is activated
    if (!midi_out.active()) {
      if (!disable_module()) {
        // turn off the lights if the module is not disabled
        lights[BYPASS_LIGHT].setBrightness(0.f);
      }
      return;
    } else {
      // enable_module
      enable_module();
    }

    // read the bypass button values
    int enable_pedal = (int) floor(params[BYPASS_PARAM].getValue());

    int bypass;
    if (enable_pedal) {
      // LED (on) (green)
      lights[BYPASS_LIGHT].setBrightness(1.f);
      bypass = 127;
    } else {
      // LED (off)
      lights[BYPASS_LIGHT].setBrightness(0.f);
      bypass = 0;
    }

    // bypass (or enable) the pedal
    midi_out.sendCachedCC(bypass, 102);

    // read the three-way arcade buttons values
    int jump_arcade = (int) floor(params[JUMP_ARCADE_PARAM].getValue());
    int type_arcade = (int) floor(params[TYPE_ARCADE_PARAM].getValue());
    int diffusion_arcade = (int) floor(params[DIFFUSION_ARCADE_PARAM].getValue());
    int tank_mod_arcade = (int) floor(params[TANK_MOD_ARCADE_PARAM].getValue());
    int clock_arcade = (int) floor(params[CLOCK_ARCADE_PARAM].getValue());

    // assign values from switches
    midi_out.sendCachedCC(jump_arcade, 22);
    midi_out.sendCachedCC(type_arcade, 23);
    midi_out.sendCachedCC(diffusion_arcade, 24);
    midi_out.sendCachedCC(tank_mod_arcade, 25);
    midi_out.sendCachedCC(clock_arcade, 26);

    // check if the preset button was pressed
    // protect it from being spammed by limiting it
    // to once every 500ms.
    int preset_change = (int) floor(params[CHANGE_PRESET_PARAM].getValue());
    if (preset_change && should_transition_to_state(0.5f, preset_change_grace_period)) {
      // increment by 1 and with a max of 30 programs
      midi_out.incrementProgram(1, 30);

      // start the preset change grace period
      gettimeofday(&preset_change_grace_period, NULL);
    }

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the the user.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // slider values
    int bass = (int) std::round(params[BASS_SLIDER_PARAM].getValue());
    int mids = (int) std::round(params[MIDS_SLIDER_PARAM].getValue());
    int cross = (int) std::round(params[CROSS_SLIDER_PARAM].getValue());
    int treble = (int) std::round(params[TREBLE_SLIDER_PARAM].getValue());
    int mix = (int) std::round(params[MIX_SLIDER_PARAM].getValue());
    int predly = (int) std::round(params[PREDLY_SLIDER_PARAM].getValue());

    // read cv voltages and override the values obtained from sliders positions
    // clamp down the cv value to be between 0 and the value of the slider
    if (inputs[BASS_SLIDER_INPUT].isConnected()) {
      int bass_cv = convertCVtoCC(inputs[BASS_SLIDER_INPUT].getVoltage());
      bass = clamp(bass_cv, 0, bass);
      params[BASS_SLIDER_PARAM].setValue((float) bass);
    }
    if (inputs[MIDS_SLIDER_INPUT].isConnected()) {
      int mids_cv = convertCVtoCC(inputs[MIDS_SLIDER_INPUT].getVoltage());
      mids = clamp(mids_cv, 0, mids);
    }
    if (inputs[CROSS_SLIDER_INPUT].isConnected()) {
      int cross_cv = convertCVtoCC(inputs[CROSS_SLIDER_INPUT].getVoltage());
      cross = clamp(cross_cv, 0, cross);
    }
    if (inputs[TREBLE_SLIDER_INPUT].isConnected()) {
      int treble_cv = convertCVtoCC(inputs[TREBLE_SLIDER_INPUT].getVoltage());
      treble = clamp(treble_cv, 0, treble);
    }
    if (inputs[MIX_SLIDER_INPUT].isConnected()) {
      int mix_cv = convertCVtoCC(inputs[MIX_SLIDER_INPUT].getVoltage());
      mix = clamp(mix_cv, 0, mix);
    }
    if (inputs[PREDLY_SLIDER_INPUT].isConnected()) {
      int predly_cv = convertCVtoCC(inputs[PREDLY_SLIDER_INPUT].getVoltage());
      predly = clamp(predly_cv, 0, predly);
    }

    // Assign values from knobs (or cv)
    midi_out.sendCachedCC(bass, 14);
    midi_out.sendCachedCC(mids, 15);
    midi_out.sendCachedCC(cross, 16);
    midi_out.sendCachedCC(treble, 17);
    midi_out.sendCachedCC(mix, 18);
    midi_out.sendCachedCC(predly, 19);

    // read the expresion input if it is connected and clamp it between 0-127
    int expr = -1;
    if (inputs[EXPR_INPUT].isConnected()) {
      int expr_cv = convertCVtoCC(inputs[EXPR_INPUT].getVoltage());
      expr = clamp(expr_cv, 0, 127);

      // assign value for expression
      if (expr > 0)
        midi_out.sendCachedCC(expr, 100);
    }

    return;
  }
};

struct Cxm1978Widget : ModuleWidget {
  Cxm1978Widget(Cxm1978* module) {
    setModule(module);

    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cxm1978.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-10, 1)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 10, 1)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-10, RACK_GRID_HEIGHT - RACK_GRID_WIDTH - 1)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 10 , RACK_GRID_HEIGHT - RACK_GRID_WIDTH - 1)));

    // sliders
    addParam(createParam<AutomatoneSlider>(mm2px(Vec(10, 11)), module, Cxm1978::BASS_SLIDER_PARAM));
    addParam(createParam<AutomatoneSlider>(mm2px(Vec(25.5, 11)), module, Cxm1978::MIDS_SLIDER_PARAM));
    addParam(createParam<AutomatoneSlider>(mm2px(Vec(41, 11)), module, Cxm1978::CROSS_SLIDER_PARAM));
    addParam(createParam<AutomatoneSlider>(mm2px(Vec(56.5, 11)), module, Cxm1978::TREBLE_SLIDER_PARAM));
    addParam(createParam<AutomatoneSlider>(mm2px(Vec(72, 11)), module, Cxm1978::MIX_SLIDER_PARAM));
    addParam(createParam<AutomatoneSlider>(mm2px(Vec(87.5, 11)), module, Cxm1978::PREDLY_SLIDER_PARAM));

    // CV inputs
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(17.5, 65)), module, Cxm1978::BASS_SLIDER_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(33, 65)), module, Cxm1978::MIDS_SLIDER_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(48.5, 65)), module, Cxm1978::CROSS_SLIDER_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(64, 65)), module, Cxm1978::TREBLE_SLIDER_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(79.5, 65)), module, Cxm1978::MIX_SLIDER_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(95, 65)), module, Cxm1978::PREDLY_SLIDER_INPUT));

    // arcade buttons in the middle
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(25.5, 88)), module, Cxm1978::JUMP_ARCADE_PARAM));
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(41.0, 88)), module, Cxm1978::TYPE_ARCADE_PARAM));
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(56.5, 88)), module, Cxm1978::DIFFUSION_ARCADE_PARAM));
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(72.0, 88)), module, Cxm1978::TANK_MOD_ARCADE_PARAM));
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(87.5, 88)), module, Cxm1978::CLOCK_ARCADE_PARAM));

    // preset change button
    addParam(createParamCentered<CBAButtonGrayMomentary>(mm2px(Vec(25, 113)), module, Cxm1978::CHANGE_PRESET_PARAM));

    // bypass pedal light and button
    addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(75, 113)), module, Cxm1978::BYPASS_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(87.5, 113)), module, Cxm1978::BYPASS_PARAM));

    // expression port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(13.6, 81.5)), module, Cxm1978::EXPR_INPUT));

    // midi configuration displays
    RRMidiWidget* midiWidget = createWidget<RRMidiWidget>(mm2px(Vec(35, 99.5)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};

Model* modelCxm1978 = createModel<Cxm1978, Cxm1978Widget>("cxm1978");
