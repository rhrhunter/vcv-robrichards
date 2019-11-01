#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "midi_classes.hpp"

struct GenerationLoss : Module {
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
                 MIDI_CHANNEL_PARAM,
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
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  AUX_LIGHT,
                  BYPASS_LIGHT,
                  NUM_LIGHTS
  };

  RRMidiOutput midi_out;

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

    // midi configuration knobs
    configParam(MIDI_CHANNEL_PARAM, 1.f, 16.f, 6.f, "MIDI Channel");

    // bypass buttons
    configParam(BYPASS_AUX_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass AUX Function");
    configParam(BYPASS_PEDAL_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Pedal");

    // DeviceIds start counting from 0, not 1
    midi_out.setDeviceId(3);
    midi_out.setChannel(6);
  }

  void process(const ProcessArgs& args) override {
    // configure the midi channel, return if it is not set
    int channel = (int) floor(params[MIDI_CHANNEL_PARAM].getValue() + 0.5);
    if (channel <= 0)
      return;
    else
      midi_out.setChannel(channel);

    // knob values
    int wow = (int) floor(params[WOW_PARAM].getValue() + 0.5);
    int wet = (int) floor(params[WET_PARAM].getValue() + 0.5);
    int hp= (int) floor(params[HP_PARAM].getValue() + 0.5);
    int flutter = (int) floor(params[FLUTTER_PARAM].getValue() + 0.5);
    int gen = (int) floor(params[GEN_PARAM].getValue() + 0.5);
    int lp = (int) floor(params[LP_PARAM].getValue() + 0.5);

    // switch values
    int aux_func = (int) floor(params[AUX_FUNC_PARAM].getValue());
    int dry_func = (int) floor(params[DRY_PARAM].getValue());
    int hiss_func = (int) floor(params[HISS_PARAM].getValue());

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[WOW_INPUT].isConnected()) {
      int wow_cv = (int) floor((inputs[WOW_INPUT].getVoltage() * 16.9f));
      if (wow_cv > wow)
        wow_cv = wow;
      wow = wow_cv;
    }

    if (inputs[WET_INPUT].isConnected()) {
      int wet_cv = (int) floor((inputs[WET_INPUT].getVoltage() * 16.9f));
      if (wet_cv > wet)
        wet_cv = wet;
      wet = wet_cv;
    }

    if (inputs[HP_INPUT].isConnected()) {
      int hp_cv = (int) floor((inputs[HP_INPUT].getVoltage() * 16.9f));
      if (hp_cv > hp)
        hp_cv = hp;
      hp = hp_cv;
    }

    if (inputs[GEN_INPUT].isConnected()) {
      int gen_cv = (int) floor((inputs[GEN_INPUT].getVoltage() * 16.9f));
      if (gen_cv > gen)
        gen_cv = gen;
      gen = gen_cv;
    }

    if (inputs[FLUTTER_INPUT].isConnected()) {
      int flutter_cv = (int) floor((inputs[FLUTTER_INPUT].getVoltage() * 16.9f));
      if (flutter_cv > flutter)
        flutter_cv = flutter;
      flutter = flutter_cv;
    }

    if (inputs[LP_INPUT].isConnected()) {
      int lp_cv = (int) floor((inputs[LP_INPUT].getVoltage() * 16.9f));
      if (lp_cv > lp)
        lp_cv = lp;
      lp = lp_cv;
    }

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

    // assign values from knobs (or cv)
    midi_out.setValue(wow, 14);
    midi_out.setValue(wet, 15);
    midi_out.setValue(hp, 16);
    midi_out.setValue(flutter, 17);
    midi_out.setValue(gen, 18);
    midi_out.setValue(lp, 19);

    // assign values from switches
    midi_out.setValue(aux_func, 21);
    midi_out.setValue(dry_func, 22);
    midi_out.setValue(hiss_func, 23);

    // bypass the aux function and/or pedal
    midi_out.setValue(bypass, 103);
  }
};


struct GenerationLossWidget : ModuleWidget {
  GenerationLossWidget(GenerationLoss* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/genloss_blank.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 15)), module, GenerationLoss::WOW_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 15)), module, GenerationLoss::WET_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 15)), module, GenerationLoss::HP_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 50)), module, GenerationLoss::FLUTTER_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 50)), module, GenerationLoss::GEN_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 50)), module, GenerationLoss::LP_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 30)), module, GenerationLoss::WOW_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 30)), module, GenerationLoss::WET_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 30)), module, GenerationLoss::HP_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 65)), module, GenerationLoss::FLUTTER_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 65)), module, GenerationLoss::GEN_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 65)), module, GenerationLoss::LP_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 80)), module, GenerationLoss::AUX_FUNC_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(30, 80)), module, GenerationLoss::DRY_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(50, 80)), module, GenerationLoss::HISS_PARAM));

    // bypass switches
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(15, 109)), module, GenerationLoss::AUX_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(15, 118)), module, GenerationLoss::BYPASS_AUX_PARAM));
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(46, 109)), module, GenerationLoss::BYPASS_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(46, 118)), module, GenerationLoss::BYPASS_PEDAL_PARAM));

    // midi configuration displays
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 100)), module, GenerationLoss::MIDI_CHANNEL_PARAM));
    MidiChannelDisplay *mcd = new MidiChannelDisplay();
    mcd->box.pos = Vec(50, 285);
    mcd->box.size = Vec(32, 20);
    mcd->value = &((module->midi_out).midi_channel);
    mcd->module = (void *) module;
    addChild(mcd);

  }
};

Model* modelGenerationLoss = createModel<GenerationLoss, GenerationLossWidget>("genloss");
