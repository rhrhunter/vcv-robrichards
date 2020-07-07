#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"
#include "rr_midiwidget.hpp"

struct PreampMKII : RRModule {
  enum ParamIds {
                 VOLUME_SLIDER_PARAM,
                 TREBLE_SLIDER_PARAM,
                 MIDS_SLIDER_PARAM,
                 FREQ_SLIDER_PARAM,
                 BASS_SLIDER_PARAM,
                 GAIN_SLIDER_PARAM,
                 JUMP_ARCADE_PARAM,
                 MIDS_ARCADE_PARAM,
                 Q_ARCADE_PARAM,
                 DIODE_ARCADE_PARAM,
                 FUZZ_ARCADE_PARAM,
                 CHANGE_PRESET_PARAM,
                 BYPASS_PARAM,
                 NUM_PARAMS
  };
  enum InputIds  {
                  PRESET_INPUT,
                  BYPASS_INPUT,
                  EXPR_INPUT,
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  BYPASS_LIGHT,
                  NUM_LIGHTS
  };

  PreampMKII() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main slider parameters
    configParam(VOLUME_SLIDER_PARAM, 0.f, 127.f, 0.f, "Volume");
    configParam(TREBLE_SLIDER_PARAM, 0.f, 127.f, 0.f, "Treble");
    configParam(MIDS_SLIDER_PARAM, 0.f, 127.f, 0.f, "Mids");
    configParam(FREQ_SLIDER_PARAM, 0.f, 127.f, 0.f, "Frequency");
    configParam(BASS_SLIDER_PARAM, 0.f, 127.f, 0.f, "Bass");
    configParam(GAIN_SLIDER_PARAM, 0.f, 127.f, 0.f, "Gain");

    // arcade buttons
    // 1.0f is the black text
    // 2.0f is the blue text
    // 3.0f is the red text
    configParam(JUMP_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Preset Jump (Off, 1, 5)");
    configParam(MIDS_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Mids Routine (Off, Pre, Post)");
    configParam(Q_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Frequency Width 'Q' (Low, Mid, High)");
    configParam(DIODE_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Diode Type (Off, Sil, Germ)");
    configParam(FUZZ_ARCADE_PARAM, 1.0f, 3.0f, 1.0f, "Fuzz Type (Off, Open, Gated)");

    // bypass buttons
    configParam(CHANGE_PRESET_PARAM, 0.f, 1.f, 0.f, "Change Preset");
    configParam(BYPASS_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Pedal");
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
      // LED (on) (red)
      lights[BYPASS_LIGHT].setBrightness(1.f);
      bypass = 127;
    } else {
      // LED (off)
      lights[BYPASS_LIGHT].setBrightness(0.f);
      bypass = 0;
    }

    // bypass (or enable) the pedal
    midi_out.setValue(bypass, 102);

    // read the three-way arcade buttons values
    int jump_arcade = (int) floor(params[JUMP_ARCADE_PARAM].getValue());
    int mids_arcade = (int) floor(params[MIDS_ARCADE_PARAM].getValue());
    int q_arcade = (int) floor(params[Q_ARCADE_PARAM].getValue());
    int diode_arcade = (int) floor(params[DIODE_ARCADE_PARAM].getValue());
    int fuzz_arcade = (int) floor(params[FUZZ_ARCADE_PARAM].getValue());

    // assign values from switches
    midi_out.setValue(jump_arcade, 22);
    midi_out.setValue(mids_arcade, 23);
    midi_out.setValue(q_arcade, 24);
    midi_out.setValue(diode_arcade, 25);
    midi_out.setValue(fuzz_arcade, 26);

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the the user.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // slider values
    int volume = (int) std::round(params[VOLUME_SLIDER_PARAM].getValue());
    int treble = (int) std::round(params[TREBLE_SLIDER_PARAM].getValue());
    int mids = (int) std::round(params[MIDS_SLIDER_PARAM].getValue());
    int freq = (int) std::round(params[FREQ_SLIDER_PARAM].getValue());
    int bass = (int) std::round(params[BASS_SLIDER_PARAM].getValue());
    int gain = (int) std::round(params[GAIN_SLIDER_PARAM].getValue());

    // assign values from knobs (or cv)
    midi_out.setValue(volume, 14);
    midi_out.setValue(treble, 15);
    midi_out.setValue(mids, 16);
    midi_out.setValue(freq, 17);
    midi_out.setValue(bass, 18);
    midi_out.setValue(gain, 19);

    // read the expresion input if it is connected and clamp it between 0-127
    int expr = -1;
    if (inputs[EXPR_INPUT].isConnected()) {
      int expr_cv = (int) std::round(inputs[EXPR_INPUT].getVoltage()*2) / 10.f * 127;
      expr = clamp(expr_cv, 0, 127);

      // assign value for expression
      if (expr > 0)
        midi_out.setValue(expr, 100);
    }

    return;
  }
};

struct PreampMKIIWidget : ModuleWidget {
  PreampMKIIWidget(PreampMKII* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/preamp_mk2.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-10, 1)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 10, 1)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH-10, RACK_GRID_HEIGHT - RACK_GRID_WIDTH - 1)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH + 10 , RACK_GRID_HEIGHT - RACK_GRID_WIDTH - 1)));

    // sliders
    addParam(createParam<PreampMKIISlider>(mm2px(Vec(10, 11)), module, PreampMKII::VOLUME_SLIDER_PARAM));
    addParam(createParam<PreampMKIISlider>(mm2px(Vec(25.5, 11)), module, PreampMKII::TREBLE_SLIDER_PARAM));
    addParam(createParam<PreampMKIISlider>(mm2px(Vec(41, 11)), module, PreampMKII::MIDS_SLIDER_PARAM));
    addParam(createParam<PreampMKIISlider>(mm2px(Vec(56.5, 11)), module, PreampMKII::FREQ_SLIDER_PARAM));
    addParam(createParam<PreampMKIISlider>(mm2px(Vec(72, 11)), module, PreampMKII::BASS_SLIDER_PARAM));
    addParam(createParam<PreampMKIISlider>(mm2px(Vec(87.5, 11)), module, PreampMKII::GAIN_SLIDER_PARAM));

    // arcade buttons in the middle
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(25.5, 88)), module, PreampMKII::JUMP_ARCADE_PARAM));
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(41.0, 88)), module, PreampMKII::MIDS_ARCADE_PARAM));
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(56.5, 88)), module, PreampMKII::Q_ARCADE_PARAM));
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(72.0, 88)), module, PreampMKII::DIODE_ARCADE_PARAM));
    addParam(createParamCentered<CBAArcadeButtonOffBlueRed>(mm2px(Vec(87.5, 88)), module, PreampMKII::FUZZ_ARCADE_PARAM));

    // preset change button
    addParam(createParamCentered<CBAMomentaryButtonGray>(mm2px(Vec(25, 113)), module, PreampMKII::CHANGE_PRESET_PARAM));

    // bypass pedal light and button
    addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(75, 113)), module, PreampMKII::BYPASS_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(87.5, 113)), module, PreampMKII::BYPASS_PARAM));

    // expression port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(13.6, 81.5)), module, PreampMKII::EXPR_INPUT));

    // midi configuration displays
    RRMidiWidget* midiWidget = createWidget<RRMidiWidget>(mm2px(Vec(35, 99.5)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};

Model* modelPreampMKII = createModel<PreampMKII, PreampMKIIWidget>("preamp_mk2");
