#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"
#include "rr_midiwidget.hpp"
#include <dsp/digital.hpp>

struct Thermae : RRModule {
  enum ParamIds {
                 MIX_PARAM,
                 LPF_PARAM,
                 REGEN_PARAM,
                 GLIDE_PARAM,
                 INT1_PARAM,
                 INT2_PARAM,
                 L_TOGGLE_PARAM,
                 M_TOGGLE_PARAM,
                 R_TOGGLE_PARAM,
                 HOLD_MODE_PARAM,
                 SLOWDOWN_MODE_PARAM,
                 BYPASS_PARAM,
                 TAP_TEMPO_PARAM,
                 NUM_PARAMS
  };
  enum InputIds  {
                  MIX_INPUT,
                  LPF_INPUT,
                  REGEN_INPUT,
                  GLIDE_INPUT,
                  INT1_INPUT,
                  INT2_INPUT,
                  CLOCK_INPUT,
                  EXPR_INPUT,
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  ENUMS(TAP_TEMPO_LIGHT, 2),
                  BYPASS_LIGHT,
                  NUM_LIGHTS
  };

  // periodic internal clock processing
  dsp::ClockDivider disable_hold_mode_clk;
  int disable_hold_mode_attempts = 0;

  // tap tempo LED colors
  int curr_tap_tempo_light_color = 1; // 1=red, 0=green

  Thermae() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(MIX_PARAM, 0.f, 127.f, 0.f, "Mix (Wet/Dry)");
    configParam(LPF_PARAM, 0.f, 127.f, 64.f, "LPF (Low Pass Filter)");
    configParam(REGEN_PARAM, 0.f, 127.f, 0.f, "Regen");
    configParam(GLIDE_PARAM, 0.f, 127.f, 0.f, "Glide");
    configParam(INT1_PARAM, 0.f, 127.f, 64.f, "Int1");
    configParam(INT2_PARAM, 0.f, 127.f, 64.f, "Int2");

    // 3 way switches
    // 1.0f is top position
    configParam(L_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Pre-delay (Quarter Note, Dotted Eighth Note, Eighth Note)");
    configParam(M_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Int1 delay (Quarter Note, Dotted Eighth Note, Eighth Note)");
    configParam(R_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Int2 delay (Quarter Note, Dotted Eighth Note, Eighth Note");

    // 2 way switches
    configParam(HOLD_MODE_PARAM, 0.f, 1.f, 0.f, "Hold Mode (Self Oscillation)");
    configParam(SLOWDOWN_MODE_PARAM, 0.f, 1.f, 0.f, "Slowdown Mode");

    // bypass button
    configParam(BYPASS_PARAM, 0.f, 1.f, 0.f, "Pedal Bypass");

    // tap tempo buttons
    configParam(TAP_TEMPO_PARAM, 0.f, 1.f, 0.f, "Tap Tempo");

    // keep an internal clock to disable hold mode (~every 1s) for up
    // to 5 times after it has been turned on.
    disable_hold_mode_clk.setDivision(65536);
  }

  void process(const ProcessArgs& args) override {
    // only proceed if midi is activated
    if (!midi_out.active()) {
      if (!disable_module()) {
        // turn off the lights if the module is not disabled
        lights[TAP_TEMPO_LIGHT + 0].setBrightness(0.f);
        lights[TAP_TEMPO_LIGHT + 1].setBrightness(0.f);
        lights[BYPASS_LIGHT].setBrightness(0.f);
      }
      return;
    } else {
      // enable_module
      enable_module();
    }

    // handle a clock message
    if (inputs[CLOCK_INPUT].isConnected()) {
      bool clock = inputs[CLOCK_INPUT].getVoltage() >= 1.f;
      process_midi_clock(clock);
    }

    // 2way switch values (0,127)
    int slowdown_mode = (int) floor(params[SLOWDOWN_MODE_PARAM].getValue());
    int hold_mode = (int) floor(params[HOLD_MODE_PARAM].getValue());
    if (slowdown_mode > 0) {
      slowdown_mode = 127;
      // entering slowdown mode makes the tap tempo light turn green
      curr_tap_tempo_light_color = 0;
    } else {
      // disabling slowdown mode makes the tap tempo light turn red
      curr_tap_tempo_light_color = 1;
    }
    if (hold_mode > 0)
      hold_mode = 127;

    // process any tap tempo requests
    int tap_tempo = (int) floor(params[TAP_TEMPO_PARAM].getValue());
    float tap_tempo_brightness = process_tap_tempo(tap_tempo);
    if (tap_tempo_brightness >= 0) {
      // flash the current color using the processed brightness value
      lights[TAP_TEMPO_LIGHT + curr_tap_tempo_light_color].setBrightness(tap_tempo_brightness);
      // turn off the other color in case it is still on
      lights[TAP_TEMPO_LIGHT + (!curr_tap_tempo_light_color)].setBrightness(0.f);
    }

    // bypass or enable the pedal
    int enable_pedal = (int) floor(params[BYPASS_PARAM].getValue());
    int bypass;
    if (enable_pedal) {
      // turn bypass light on (red)
      lights[BYPASS_LIGHT].setBrightness(1.f);
      bypass = 127;
    } else {
      lights[BYPASS_LIGHT].setBrightness(0.f);
      bypass = 0;
    }

    // enable or bypass the pedal
    midi_out.setValue(bypass, 102);

    // 3way switch values (1,2,3)
    int l_toggle = (int) floor(params[L_TOGGLE_PARAM].getValue());
    int m_toggle = (int) floor(params[M_TOGGLE_PARAM].getValue());
    int r_toggle = (int) floor(params[R_TOGGLE_PARAM].getValue());

    // assign values from switches
    midi_out.setValue(l_toggle, 21);
    midi_out.setValue(m_toggle, 22);
    midi_out.setValue(r_toggle, 23);

    // periodically reset the CC message cache for hold mode if it is
    // not turned on by the user. This is so that it doesn't get stuck turned on.
    if (!hold_mode) {
      if (disable_hold_mode_attempts > 0 && disable_hold_mode_clk.process()) {
        disable_hold_mode_attempts--;
        midi_out.resetCCCache(24);
      }
    } else {
      // hold mode is being requested, reset the hold mode attempts
      disable_hold_mode_attempts = 5;
    }

    // enable/disable hold mode and/or slowdown mode
    midi_out.setValue(hold_mode, 24);
    midi_out.setValue(slowdown_mode, 25);

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the CV inputs.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // knob values
    int mix = (int) std::round(params[MIX_PARAM].getValue());
    int lpf = (int) std::round(params[LPF_PARAM].getValue());
    int regen = (int) std::round(params[REGEN_PARAM].getValue());
    int glide = (int) std::round(params[GLIDE_PARAM].getValue());
    int int1 = (int) std::round(params[INT1_PARAM].getValue());
    int int2 = (int) std::round(params[INT2_PARAM].getValue());
    int expr = -1;

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = (int) std::round(inputs[MIX_INPUT].getVoltage()*2) / 10.f * 127;
      mix = clamp(mix_cv, 0, mix);
    }
    if (inputs[LPF_INPUT].isConnected()) {
      int lpf_cv = (int) std::round(inputs[LPF_INPUT].getVoltage()*2) / 10.f * 127;
      lpf = clamp(lpf_cv, 0, lpf);
    }
    if (inputs[REGEN_INPUT].isConnected()) {
      int regen_cv = (int) std::round(inputs[REGEN_INPUT].getVoltage()*2) / 10.f * 127;
      regen = clamp(regen_cv, 0, regen);
    }
    if (inputs[GLIDE_INPUT].isConnected()) {
      int glide_cv = (int) std::round(inputs[GLIDE_INPUT].getVoltage()*2) / 10.f * 127;
      glide = clamp(glide_cv, 0, glide);
    }
    if (inputs[INT1_INPUT].isConnected()) {
      int int1_cv = (int) std::round(inputs[INT1_INPUT].getVoltage()*2) / 10.f * 127;
      int1 = clamp(int1_cv, 0, int1);
    }
    if (inputs[INT2_INPUT].isConnected()) {
      int int2_cv = (int) std::round(inputs[INT2_INPUT].getVoltage()*2) / 10.f * 127;
      int2 = clamp(int2_cv, 0, int2);
    }
    if (inputs[EXPR_INPUT].isConnected()) {
      int expr_cv = (int) std::round(inputs[EXPR_INPUT].getVoltage()*2) / 10.f * 127;
      expr = clamp(expr_cv, 0, 127);
    }

    // assign values from knobs (or cv)
    midi_out.setValue(mix, 14);
    midi_out.setValue(lpf, 15);
    midi_out.setValue(regen, 16);
    midi_out.setValue(glide, 17);
    midi_out.setValue(int1, 18);
    midi_out.setValue(int2, 19);

    // assign value for expression
    if (expr > 0)
      midi_out.setValue(expr, 100);

    return;
  }
};

struct ThermaeWidget : ModuleWidget {
  ThermaeWidget(Thermae* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/thermae_text.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnobThermae>(mm2px(Vec(10, 12)), module, Thermae::MIX_PARAM));
    addParam(createParamCentered<CBAKnobThermae>(mm2px(Vec(30, 12)), module, Thermae::LPF_PARAM));
    addParam(createParamCentered<CBAKnobThermae>(mm2px(Vec(50, 12)), module, Thermae::REGEN_PARAM));
    addParam(createParamCentered<CBAKnobThermae>(mm2px(Vec(10, 40)), module, Thermae::GLIDE_PARAM));
    addParam(createParamCentered<CBAKnobThermae>(mm2px(Vec(30, 40)), module, Thermae::INT1_PARAM));
    addParam(createParamCentered<CBAKnobThermae>(mm2px(Vec(50, 40)), module, Thermae::INT2_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 25)), module, Thermae::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 25)), module, Thermae::LPF_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 25)), module, Thermae::REGEN_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 53)), module, Thermae::GLIDE_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 53)), module, Thermae::INT1_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 53)), module, Thermae::INT2_INPUT));

    // expression port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(43.5, 92)), module, Thermae::EXPR_INPUT));

    // clock port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55, 92)), module, Thermae::CLOCK_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 66)), module, Thermae::L_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(30, 66)), module, Thermae::M_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(50, 66)), module, Thermae::R_TOGGLE_PARAM));

    // slowdown mode toggle and hold mode toggle
    addParam(createParamCentered<CBASwitchTwoWay>(mm2px(Vec(43.5, 82)), module, Thermae::SLOWDOWN_MODE_PARAM));
    addParam(createParamCentered<CBASwitchTwoWayMomentary>(mm2px(Vec(55, 82)), module, Thermae::HOLD_MODE_PARAM));

    // bypass switches & tap tempo
    addChild(createLightCentered<LargeLight<GreenRedLight>>(mm2px(Vec(15, 109)), module, Thermae::TAP_TEMPO_LIGHT));
    addParam(createParamCentered<CBAMomentaryButtonGray>(mm2px(Vec(15, 118)), module, Thermae::TAP_TEMPO_PARAM));
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(46, 109)), module, Thermae::BYPASS_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(46, 118)), module, Thermae::BYPASS_PARAM));

    // midi configuration display
    RRMidiWidget* midiWidget = createWidget<RRMidiWidget>(mm2px(Vec(3, 75)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};

Model* modelThermae = createModel<Thermae, ThermaeWidget>("thermae");
