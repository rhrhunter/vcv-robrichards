#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"
#include "rr_midiwidget.hpp"
#include <sys/time.h>

struct WarpedVinyl : RRModule {
  enum ParamIds {
                 TONE_PARAM,
                 LAG_PARAM,
                 MIX_PARAM,
                 RPM_PARAM,
                 DEPTH_PARAM,
                 WARP_PARAM,
                 NOTE_DIVISION_PARAM,
                 BYPASS_PARAM,
                 TAP_TEMPO_PARAM,
                 NUM_PARAMS
  };
  enum InputIds  {
                  TONE_INPUT,
                  LAG_INPUT,
                  MIX_INPUT,
                  RPM_INPUT,
                  DEPTH_INPUT,
                  WARP_INPUT,
                  CLOCK_INPUT,
                  EXPR_INPUT,
                  TAP_TEMPO_INPUT_HIGH,
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  TAP_TEMPO_LIGHT,
                  BYPASS_LIGHT,
                  NUM_LIGHTS
  };

  dsp::SchmittTrigger tap_tempo_trigger_high;

  WarpedVinyl() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(TONE_PARAM, 0.f, 127.f, 0.f, "Tone");
    configParam(LAG_PARAM, 0.f, 127.f, 0.f, "Lag");
    configParam(MIX_PARAM, 0.f, 127.f, 0.f, "Mix");
    configParam(RPM_PARAM, 0.f, 127.f, 0.f, "RPM");
    configParam(DEPTH_PARAM, 0.f, 127.f, 0.f, "Depth");
    configParam(WARP_PARAM, 0.f, 127.f, 0.f, "Warp");

    // 6 way switches
    // 0.0f is top position
    configParam(NOTE_DIVISION_PARAM, 0, 5, 0, "Midi Note Divisions (whole,half,quarter triplet,quarter,eight,sixteenth)");

    // bypass button
    configParam(BYPASS_PARAM, 0.f, 1.f, 0.f, "Pedal Bypass");

    // tap tempo buttons
    configParam(TAP_TEMPO_PARAM, 0.f, 1.f, 0.f, "Tap Tempo");
  }

  void process(const ProcessArgs& args) override {
    // only proceed if midi is activated
    if (!midi_out.active()) {
      if (!disable_module()) {
        // turn off the lights if the module is not disabled
        lights[TAP_TEMPO_LIGHT].setBrightness(0.f);
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
    } else {
      // clock is not connected, reset the cache for enabling "listen for clock"
      reset_midi_clock_cc_cache();
    }

    // read the gate triggers
    int tap_gate = 0;
    if (inputs[TAP_TEMPO_INPUT_HIGH].isConnected()) {
      if (tap_tempo_trigger_high.process(rescale(inputs[TAP_TEMPO_INPUT_HIGH].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
        // if the trigger goes high, trigger a tap tempo
        tap_gate = 1;
      }
    }

    // process any tap tempo requests
    int tap_tempo = (int) floor(params[TAP_TEMPO_PARAM].getValue());
    float tap_tempo_brightness = process_tap_tempo(tap_gate ? 1 : tap_tempo);
    if (tap_tempo_brightness >= 0) {
      // flash the current color using the processed brightness value
      lights[TAP_TEMPO_LIGHT].setBrightness(tap_tempo_brightness);
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

    // left switch values (0,1,2,3,4,5)
    int note_division = (int) floor(params[NOTE_DIVISION_PARAM].getValue());

    // assign values from switches
    midi_out.setValue(note_division, 21);

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the CV inputs.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // knob values
    int tone = (int) std::round(params[TONE_PARAM].getValue());
    int lag = (int) std::round(params[LAG_PARAM].getValue());
    int mix = (int) std::round(params[MIX_PARAM].getValue());
    int rpm = (int) std::round(params[RPM_PARAM].getValue());
    int depth = (int) std::round(params[DEPTH_PARAM].getValue());
    int warp = (int) std::round(params[WARP_PARAM].getValue());
    int expr = -1;

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[TONE_INPUT].isConnected()) {
      int tone_cv = (int) std::round(inputs[TONE_INPUT].getVoltage()*2) / 10.f * 127;
      tone = clamp(tone_cv, 0, tone);
    }
    if (inputs[LAG_INPUT].isConnected()) {
      int lag_cv = (int) std::round(inputs[LAG_INPUT].getVoltage()*2) / 10.f * 127;
      lag = clamp(lag_cv, 0, lag);
    }
    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = (int) std::round(inputs[MIX_INPUT].getVoltage()*2) / 10.f * 127;
      mix = clamp(mix_cv, 0, mix);
    }
    if (inputs[RPM_INPUT].isConnected()) {
      int rpm_cv = (int) std::round(inputs[RPM_INPUT].getVoltage()*2) / 10.f * 127;
      rpm = clamp(rpm_cv, 0, rpm);
    }
    if (inputs[DEPTH_INPUT].isConnected()) {
      int depth_cv = (int) std::round(inputs[DEPTH_INPUT].getVoltage()*2) / 10.f * 127;
      depth = clamp(depth_cv, 0, depth);
    }
    if (inputs[WARP_INPUT].isConnected()) {
      int warp_cv = (int) std::round(inputs[WARP_INPUT].getVoltage()*2) / 10.f * 127;
      warp = clamp(warp_cv, 0, warp);
    }
    if (inputs[EXPR_INPUT].isConnected()) {
      int expr_cv = (int) std::round(inputs[EXPR_INPUT].getVoltage()*2) / 10.f * 127;
      expr = clamp(expr_cv, 0, 127);
    }

    // assign values from knobs (or cv)
    midi_out.setValue(tone, 14);
    midi_out.setValue(lag, 15);
    midi_out.setValue(mix, 16);
    midi_out.setValue(rpm, 17);
    midi_out.setValue(depth, 18);
    midi_out.setValue(warp, 19);

    // assign value for expression
    if (expr > 0)
      midi_out.setValue(expr, 100);

    return;
  }
};

struct WarpedVinylWidget : ModuleWidget {
  WarpedVinylWidget(WarpedVinyl* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/warpedvinyl_panel.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnobWV>(mm2px(Vec(10, 12)), module, WarpedVinyl::TONE_PARAM));
    addParam(createParamCentered<CBAKnobWV>(mm2px(Vec(30, 12)), module, WarpedVinyl::LAG_PARAM));
    addParam(createParamCentered<CBAKnobWV>(mm2px(Vec(50, 12)), module, WarpedVinyl::MIX_PARAM));
    addParam(createParamCentered<CBAKnobWV>(mm2px(Vec(10, 40)), module, WarpedVinyl::RPM_PARAM));
    addParam(createParamCentered<CBAKnobWV>(mm2px(Vec(30, 40)), module, WarpedVinyl::DEPTH_PARAM));
    addParam(createParamCentered<CBAKnobWV>(mm2px(Vec(50, 40)), module, WarpedVinyl::WARP_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 25)), module, WarpedVinyl::TONE_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 25)), module, WarpedVinyl::LAG_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 25)), module, WarpedVinyl::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 53)), module, WarpedVinyl::RPM_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 53)), module, WarpedVinyl::DEPTH_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 53)), module, WarpedVinyl::WARP_INPUT));

    // expression port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(43.5, 92)), module, WarpedVinyl::EXPR_INPUT));

    // clock port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55, 92)), module, WarpedVinyl::CLOCK_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(7, 66)), module, WarpedVinyl::NOTE_DIVISION_PARAM));

    // bypass switches & tap tempo
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(15, 109)), module, WarpedVinyl::TAP_TEMPO_LIGHT));
    addParam(createParamCentered<CBAButtonGrayMomentary>(mm2px(Vec(15, 118)), module, WarpedVinyl::TAP_TEMPO_PARAM));
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(46, 109)), module, WarpedVinyl::BYPASS_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(46, 118)), module, WarpedVinyl::BYPASS_PARAM));

    // tap tempo high gate
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25, 109)), module, WarpedVinyl::TAP_TEMPO_INPUT_HIGH));

    // midi configuration display
    RRMidiWidget* midiWidget = createWidget<RRMidiWidget>(mm2px(Vec(3, 75)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};

Model* modelWarpedVinyl = createModel<WarpedVinyl, WarpedVinylWidget>("warpedvinyl");
