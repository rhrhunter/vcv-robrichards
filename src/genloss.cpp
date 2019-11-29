#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"

struct GenerationLoss : RRModule {
  enum ParamIds {
                 WOW_PARAM,
                 WET_PARAM,
                 HP_PARAM,
                 FLUTTER_PARAM,
                 GEN_PARAM,
                 LP_PARAM,
                 AUX_FUNC_PARAM,
                 DRY_PARAM,
                 HISS_PARAM,
                 BYPASS_AUX_PARAM,
                 BYPASS_PEDAL_PARAM,
                 NUM_PARAMS
  };
  enum InputIds  {
                  WOW_INPUT,
                  WET_INPUT,
                  HP_INPUT,
                  FLUTTER_INPUT,
                  GEN_INPUT,
                  LP_INPUT,
                  EXPR_INPUT,
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  AUX_LIGHT,
                  BYPASS_LIGHT,
                  NUM_LIGHTS
  };

  GenerationLoss() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(WOW_PARAM, 0.f, 127.f, 0.f, "Wow");
    configParam(WET_PARAM, 0.f, 127.f, 0.f, "Wet");
    configParam(HP_PARAM, 0.f, 127.f, 0.f, "HP (High Pass)");
    configParam(FLUTTER_PARAM, 0.f, 127.f, 0.f, "Flutter");
    configParam(GEN_PARAM, 0.f, 127.f, 0.f, "Gen (Generations)");
    configParam(LP_PARAM, 0.f, 127.f, 0.f, "LP (Low Pass)");

    // three way switches
    // 1.0f is top position
    configParam(AUX_FUNC_PARAM, 1.0f, 3.0f, 2.0f, "AUX Function (Mod, Gen, Filter)");
    configParam(DRY_PARAM, 1.0f, 3.0f, 2.0f, "Dry Selection (None, Small, Unity)");
    configParam(HISS_PARAM, 1.0f, 3.0f, 2.0f, "Hiss Selection (None, Mild, Heavy)");

    // bypass buttons
    configParam(BYPASS_AUX_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass AUX Function");
    configParam(BYPASS_PEDAL_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Pedal");
  }

  void process(const ProcessArgs& args) override {
    // only proceed if midi is activated
    if (!midi_out.active())
      return;

    // read the bypass button values
    int enable_aux = (int) floor(params[BYPASS_AUX_PARAM].getValue());
    int enable_pedal = (int) floor(params[BYPASS_PEDAL_PARAM].getValue());

    int bypass;
    if (enable_pedal && enable_aux) {
      // bypass LED red (on)
      // aux LED red (on)
      lights[BYPASS_LIGHT].setBrightness(1.f);
      lights[AUX_LIGHT].setBrightness(1.f);
      bypass = 127;
    } else if (!enable_pedal && enable_aux) {
      // bypass LED off
      // aux LED red (on)
      lights[BYPASS_LIGHT].setBrightness(0.f);
      lights[AUX_LIGHT].setBrightness(1.f);
      bypass = 85;
    } else if (enable_pedal && !enable_aux) {
      // bypass LED red (on)
      // aux LED off
      lights[BYPASS_LIGHT].setBrightness(1.f);
      lights[AUX_LIGHT].setBrightness(0.f);
      bypass = 45;
    } else {
      // bypass LED off
      // aux LED off
      lights[BYPASS_LIGHT].setBrightness(0.f);
      lights[AUX_LIGHT].setBrightness(0.f);
      bypass = 0;
    }

    // bypass the aux function and/or pedal
    midi_out.setValue(bypass, 103);

    // read the three-way switch values
    int aux_func = (int) floor(params[AUX_FUNC_PARAM].getValue());
    int dry_func = (int) floor(params[DRY_PARAM].getValue());
    int hiss_func = (int) floor(params[HISS_PARAM].getValue());

    // assign values from switches
    midi_out.setValue(aux_func, 21);
    midi_out.setValue(dry_func, 22);
    midi_out.setValue(hiss_func, 23);

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the CV inputs.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // knob values
    int wow = (int) std::round(params[WOW_PARAM].getValue());
    int wet = (int) std::round(params[WET_PARAM].getValue());
    int hp= (int) std::round(params[HP_PARAM].getValue());
    int flutter = (int) std::round(params[FLUTTER_PARAM].getValue());
    int gen = (int) std::round(params[GEN_PARAM].getValue());
    int lp = (int) std::round(params[LP_PARAM].getValue());
    int expr = -1;

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[WOW_INPUT].isConnected()) {
      int wow_cv = (int) std::round(inputs[WOW_INPUT].getVoltage()*2) / 10.f * 127;
      wow = clamp(wow_cv, 0, wow);
    }
    if (inputs[WET_INPUT].isConnected()) {
      int wet_cv = (int) std::round(inputs[WET_INPUT].getVoltage()*2) / 10.f * 127;
      wet = clamp(wet_cv, 0, wet);
    }
    if (inputs[HP_INPUT].isConnected()) {
      int hp_cv = (int) std::round(inputs[HP_INPUT].getVoltage()*2) / 10.f * 127;
      hp = clamp(hp_cv, 0, hp);
    }
    if (inputs[GEN_INPUT].isConnected()) {
      int gen_cv = (int) std::round(inputs[GEN_INPUT].getVoltage()*2) / 10.f * 127;
      gen = clamp(gen_cv, 0, gen);
    }
    if (inputs[FLUTTER_INPUT].isConnected()) {
      int flutter_cv = (int) std::round(inputs[FLUTTER_INPUT].getVoltage()*2) / 10.f * 127;
      flutter = clamp(flutter_cv, 0, flutter);
    }
    if (inputs[LP_INPUT].isConnected()) {
      int lp_cv = (int) std::round(inputs[LP_INPUT].getVoltage()*2) / 10.f * 127;
      lp = clamp(lp_cv, 0, lp);
    }
    if (inputs[EXPR_INPUT].isConnected()) {
      int expr_cv = (int) std::round(inputs[EXPR_INPUT].getVoltage()*2) / 10.f * 127;
      expr = clamp(expr_cv, 0, 127);
    }

    // assign values from knobs (or cv)
    midi_out.setValue(wow, 14);
    midi_out.setValue(wet, 15);
    midi_out.setValue(hp, 16);
    midi_out.setValue(flutter, 17);
    midi_out.setValue(gen, 18);
    midi_out.setValue(lp, 19);

    // assign value for expression
    if (expr > 0)
      midi_out.setValue(expr, 100);
  }
};


struct GenerationLossWidget : ModuleWidget {
  GenerationLossWidget(GenerationLoss* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/genloss_text.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 12)), module, GenerationLoss::WOW_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 12)), module, GenerationLoss::WET_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 12)), module, GenerationLoss::HP_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 40)), module, GenerationLoss::FLUTTER_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 40)), module, GenerationLoss::GEN_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 40)), module, GenerationLoss::LP_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 25)), module, GenerationLoss::WOW_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 25)), module, GenerationLoss::WET_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 25)), module, GenerationLoss::HP_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 53)), module, GenerationLoss::FLUTTER_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 53)), module, GenerationLoss::GEN_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 53)), module, GenerationLoss::LP_INPUT));

    // expression port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(43.5, 92)), module, GenerationLoss::EXPR_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 66)), module, GenerationLoss::AUX_FUNC_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(30, 66)), module, GenerationLoss::DRY_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(50, 66)), module, GenerationLoss::HISS_PARAM));

    // bypass switches
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(15, 109)), module, GenerationLoss::AUX_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(15, 118)), module, GenerationLoss::BYPASS_AUX_PARAM));
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(46, 109)), module, GenerationLoss::BYPASS_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(46, 118)), module, GenerationLoss::BYPASS_PEDAL_PARAM));

    // midi configuration displays
    MidiWidget* midiWidget = createWidget<MidiWidget>(mm2px(Vec(3, 75)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};

Model* modelGenerationLoss = createModel<GenerationLoss, GenerationLossWidget>("genloss");
