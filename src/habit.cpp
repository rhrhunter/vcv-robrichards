#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"
#include "rr_midiwidget.hpp"
#include <dsp/digital.hpp>

struct Habit : RRModule {
  enum ParamIds {
		 LEVEL_PARAM,
		 REPEATS_PARAM,
		 SIZE_PARAM,
		 MODIFY_PARAM,
		 SPREAD_PARAM,
		 SCAN_PARAM,
		 L_TOGGLE_PARAM,
		 M_TOGGLE_PARAM,
		 R_TOGGLE_PARAM,
		 LOOP_HOLD_PARAM,
		 SCAN_MODE_PARAM,
		 RESET_TOGGLE_PARAM,
		 BYPASS_PARAM,
		 TAP_TEMPO_PARAM,
		 NUM_PARAMS
  };
  enum InputIds  {
		  LEVEL_INPUT,
		  REPEATS_INPUT,
		  SIZE_INPUT,
		  MODIFY_INPUT,
		  SPREAD_INPUT,
		  SCAN_INPUT,
		  CLOCK_INPUT,
		  EXPR_INPUT,
		  TAP_TEMPO_INPUT_HIGH,
		  BYPASS_INPUT_LOW,
		  BYPASS_INPUT_HIGH,
		  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  ENUMS(TAP_TEMPO_LIGHT, 2),
		  ENUMS(BYPASS_LIGHT, 2),
		  NUM_LIGHTS
  };

  // periodic internal clock processing
  dsp::ClockDivider disable_loop_hold_clk;
  dsp::ClockDivider disable_scan_mode_clk;
  int disable_loop_hold_attempts = 0;
  int disable_scan_mode_attempts = 0;

  // tap tempo LED colors
  int curr_tap_tempo_light_color = 1; // 1=red, 0=green

  // bypass LED colors
  int curr_bypass_light_color = 1; // 1=red, 0=green

  dsp::SchmittTrigger tap_tempo_trigger_high, bypass_trigger_low, bypass_trigger_high;

  Habit() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(LEVEL_PARAM, 0.f, 127.f, 0.f, "Volume Level (Wet/Dry)");
    configParam(REPEATS_PARAM, 0.f, 127.f, 64.f, "Repeats (0 -> Infinite)");
    configParam(SIZE_PARAM, 0.f, 127.f, 0.f, "Size of each repeat (50ms -> 60s)");
    configParam(MODIFY_PARAM, 0.f, 127.f, 0.f, "Modify");
    configParam(SPREAD_PARAM, 0.f, 127.f, 64.f, "Spread");
    configParam(SCAN_PARAM, 0.f, 127.f, 64.f, "Scan");

    // 3 way switches
    // 1.0f is top position
    configParam(L_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Modifier Selection (A1{Stepped Speed}, A2{Stability}, A3{Stepped Trimmer}) (B1{Smooth Speed}, B2{Filter}, B3{Dropper}) ");
    configParam(M_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Modifier BANK (A, OFF, B)");
    configParam(R_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Record Mode (IN, OUT, FEED)");

    // 2 way switches
    configParam(LOOP_HOLD_PARAM, 0.f, 1.f, 0.f, "Loop Hold");
    configParam(SCAN_MODE_PARAM, 0.f, 1.f, 0.f, "Scan Mode");
    configParam(RESET_TOGGLE_PARAM, 0.f, 1.f, 0.f, "Reset Toggle");    

    // bypass button
    configParam(BYPASS_PARAM, 0.f, 1.f, 0.f, "Pedal Bypass");

    // tap tempo buttons
    configParam(TAP_TEMPO_PARAM, 0.f, 1.f, 0.f, "Tap Tempo (Size Selection)");

    // keep an internal clock to disable loop hold mode (~every 1s) for up
    // to 5 times after it has been turned on.
    disable_loop_hold_clk.setDivision(65536);

    // keep an internal clock to disable scan mode (~every 1s) for up
    // to 5 times after it has been turned on.
    disable_scan_mode_clk.setDivision(65536);
  }

  void process(const ProcessArgs& args) override {
    // only proceed if midi is activated
    if (!midi_out.active()) {
      if (!disable_module()) {
	// turn off the lights if the module is not disabled
	lights[TAP_TEMPO_LIGHT + 0].setBrightness(0.f);
	lights[TAP_TEMPO_LIGHT + 1].setBrightness(0.f);
	lights[BYPASS_LIGHT + 0].setBrightness(0.f);
	lights[BYPASS_LIGHT + 1].setBrightness(0.f);
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

    // 2-way switch values (0,127)
    int scan_mode = (int) floor(params[SCAN_MODE_PARAM].getValue());
    if (scan_mode > 0) {
      scan_mode = 127;
      // entering scan mode makes the tap tempo light turn solid green
      curr_tap_tempo_light_color = 0;
    } else {
      // disabling scan mode makes the tap tempo light turn red
      curr_tap_tempo_light_color = 1;
    }

    int loop_hold = (int) floor(params[LOOP_HOLD_PARAM].getValue());
    if (loop_hold > 0) {
      loop_hold = 127;
      // entering loop hold makes the bypass light turn solid green
      curr_bypass_light_color = 0;
    } else {
      // disabling loop hold makes the bypass light turn red
      curr_bypass_light_color = 1;
    }

    int reset_toggle = (int) floor(params[RESET_TOGGLE_PARAM].getValue());
    if (reset_toggle > 0) {
      reset_toggle = 127;
    } else {
      reset_toggle = 0;
    }
    
    // read the gate triggers
    int tap_gate = 0;
    if (inputs[TAP_TEMPO_INPUT_HIGH].isConnected()) {
      if (tap_tempo_trigger_high.process(rescale(inputs[TAP_TEMPO_INPUT_HIGH].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
	// if the trigger goes high, trigger a tap tempo
	tap_gate = 1;
      }
    }
    if (inputs[BYPASS_INPUT_HIGH].isConnected()) {
      if (bypass_trigger_high.process(rescale(inputs[BYPASS_INPUT_HIGH].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
	// if the trigger goes high, enable the pedal
	params[BYPASS_PARAM].setValue(1.f);
    }
    if (inputs[BYPASS_INPUT_LOW].isConnected()) {
      if (bypass_trigger_low.process(rescale(inputs[BYPASS_INPUT_LOW].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
	// if the trigger goes low, bypass the pedal
	params[BYPASS_PARAM].setValue(0.f);
    }

    // process any tap tempo requests
    int tap_tempo = (int) floor(params[TAP_TEMPO_PARAM].getValue());
    float tap_tempo_brightness = process_tap_tempo(tap_gate ? 1 : tap_tempo);
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
      // turn bypass light on (red or green depending on whether loop hold is disabled or enabled)
      lights[BYPASS_LIGHT + curr_bypass_light_color].setBrightness(1.f);
      // turn off the other color in case it is still on
      lights[BYPASS_LIGHT + (!curr_bypass_light_color)].setBrightness(1.f);
      bypass = 127;
    } else {
      // turn off the lights
      lights[BYPASS_LIGHT + curr_bypass_light_color].setBrightness(0.f);
      lights[BYPASS_LIGHT + (!curr_bypass_light_color)].setBrightness(0.f);
      bypass = 0;
    }

    // enable or bypass the pedal
    midi_out.sendCachedCC(bypass, 102);

    // 3way switch values (1,2,3)
    int l_toggle = (int) floor(params[L_TOGGLE_PARAM].getValue());
    int m_toggle = (int) floor(params[M_TOGGLE_PARAM].getValue());
    int r_toggle = (int) floor(params[R_TOGGLE_PARAM].getValue());

    // assign values from switches
    midi_out.sendCachedCC(l_toggle, 21);
    midi_out.sendCachedCC(m_toggle, 22);
    midi_out.sendCachedCC(r_toggle, 23);

    // periodically reset the CC message cache for loop hold if it is
    // not turned on by the user. This is so that it doesn't get stuck turned on.
    if (!loop_hold) {
      if (disable_loop_hold_attempts > 0 && disable_loop_hold_clk.process()) {
	disable_loop_hold_attempts--;
	midi_out.resetCCCache(24);
      }
    } else {
      // hold mode is being requested, reset the hold mode attempts
      disable_loop_hold_attempts = 2;
    }

    // periodically reset the CC message cache for scan mode if it is
    // not turned on by the user. This is so that it doesn't get stuck turned on.
    if (!scan_mode) {
      if (disable_scan_mode_attempts > 0 && disable_scan_mode_clk.process()) {
	disable_scan_mode_attempts--;
	midi_out.resetCCCache(25);
      }
    } else {
      // hold mode is being requested, reset the hold mode attempts
      disable_scan_mode_attempts = 2;
    }

    // enable/disable loop and scan mode
    midi_out.sendCachedCC(loop_hold, 24);
    midi_out.sendCachedCC(scan_mode, 25);

    // reset the memory if toggled
    midi_out.sendCachedCC(reset_toggle, 26);    

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the CV inputs.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // knob values
    int level = (int) std::round(params[LEVEL_PARAM].getValue());
    int repeats = (int) std::round(params[REPEATS_PARAM].getValue());
    int size = (int) std::round(params[SIZE_PARAM].getValue());
    int modify = (int) std::round(params[MODIFY_PARAM].getValue());
    int spread = (int) std::round(params[SPREAD_PARAM].getValue());
    int scan = (int) std::round(params[SCAN_PARAM].getValue());
    int expr = -1;

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[LEVEL_INPUT].isConnected()) {
      int level_cv = convertCVtoCC(inputs[LEVEL_INPUT].getVoltage());
      level = clamp(level_cv, 0, level);
    }
    if (inputs[REPEATS_INPUT].isConnected()) {
      int repeats_cv = convertCVtoCC(inputs[REPEATS_INPUT].getVoltage());
      repeats = clamp(repeats_cv, 0, repeats);
    }
    if (inputs[SIZE_INPUT].isConnected()) {
      int size_cv = convertCVtoCC(inputs[SIZE_INPUT].getVoltage());
      size = clamp(size_cv, 0, size);
    }
    if (inputs[MODIFY_INPUT].isConnected()) {
      int modify_cv = convertCVtoCC(inputs[MODIFY_INPUT].getVoltage());
      modify = clamp(modify_cv, 0, modify);
    }
    if (inputs[SPREAD_INPUT].isConnected()) {
      int spread_cv = convertCVtoCC(inputs[SPREAD_INPUT].getVoltage());
      spread = clamp(spread_cv, 0, spread);
    }
    if (inputs[SCAN_INPUT].isConnected()) {
      int scan_cv = convertCVtoCC(inputs[SCAN_INPUT].getVoltage());
      scan = clamp(scan_cv, 0, scan);
    }
    if (inputs[EXPR_INPUT].isConnected()) {
      int expr_cv = convertCVtoCC(inputs[EXPR_INPUT].getVoltage());
      expr = clamp(expr_cv, 0, 127);
    }

    // assign values from knobs (or cv)
    midi_out.sendCachedCC(level, 14);
    midi_out.sendCachedCC(repeats, 15);
    midi_out.sendCachedCC(size, 16);
    midi_out.sendCachedCC(modify, 17);
    midi_out.sendCachedCC(spread, 18);
    midi_out.sendCachedCC(scan, 19);

    // assign value for expression
    if (expr > 0)
      midi_out.sendCachedCC(expr, 100);

    return;
  }
};

struct HabitWidget : ModuleWidget {
  HabitWidget(Habit* module) {
    setModule(module);

#ifdef USE_LOGOS
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/habit_panel_logo.svg")));
#else
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/habit_panel.svg")));
#endif

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnobHabit>(mm2px(Vec(10, 12)), module, Habit::LEVEL_PARAM));
    addParam(createParamCentered<CBAKnobHabit>(mm2px(Vec(30, 12)), module, Habit::REPEATS_PARAM));
    addParam(createParamCentered<CBAKnobHabit>(mm2px(Vec(50, 12)), module, Habit::SIZE_PARAM));
    addParam(createParamCentered<CBAKnobHabit>(mm2px(Vec(10, 40)), module, Habit::MODIFY_PARAM));
    addParam(createParamCentered<CBAKnobHabit>(mm2px(Vec(30, 40)), module, Habit::SPREAD_PARAM));
    addParam(createParamCentered<CBAKnobHabit>(mm2px(Vec(50, 40)), module, Habit::SCAN_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 25)), module, Habit::LEVEL_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 25)), module, Habit::REPEATS_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 25)), module, Habit::SIZE_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 53)), module, Habit::MODIFY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 53)), module, Habit::SPREAD_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 53)), module, Habit::SCAN_INPUT));

    // expression port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(43.5, 92)), module, Habit::EXPR_INPUT));

    // clock port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55, 92)), module, Habit::CLOCK_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(7, 66)), module, Habit::L_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(27, 66)), module, Habit::M_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(47, 66)), module, Habit::R_TOGGLE_PARAM));

    // scan mode and loop hold momentary toggles
    addParam(createParamCentered<CBASwitchTwoWayMomentary>(mm2px(Vec(43.5, 82)), module, Habit::SCAN_MODE_PARAM));
    addParam(createParamCentered<CBASwitchTwoWayMomentary>(mm2px(Vec(55, 82)), module, Habit::LOOP_HOLD_PARAM));
    addParam(createParamCentered<CBASwitchTwoWayMomentary>(mm2px(Vec(6, 113)), module, Habit::RESET_TOGGLE_PARAM));    

    // bypass and tap tempo LEDs
    addChild(createLightCentered<LargeLight<GreenRedLight>>(mm2px(Vec(15, 109)), module, Habit::TAP_TEMPO_LIGHT));
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(46, 109)), module, Habit::BYPASS_LIGHT));

    // bypass and tap tempo switches
    addParam(createParamCentered<CBAButtonGrayMomentary>(mm2px(Vec(15, 118)), module, Habit::TAP_TEMPO_PARAM));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(46, 118)), module, Habit::BYPASS_PARAM));

    // bypass high and low gates
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 109)), module, Habit::BYPASS_INPUT_HIGH));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 118)), module, Habit::BYPASS_INPUT_LOW));

    // tap tempo high gate
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25, 109)), module, Habit::TAP_TEMPO_INPUT_HIGH));   

    // midi configuration display
    RRMidiWidget* midiWidget = createWidget<RRMidiWidget>(mm2px(Vec(3, 75)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};

Model* modelHabit = createModel<Habit, HabitWidget>("habit");
