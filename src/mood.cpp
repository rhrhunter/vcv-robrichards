#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"
#include "rr_midiwidget.hpp"
#include <dsp/digital.hpp>
#include <sys/time.h>

struct Mood : RRModule {
  enum ParamIds {
                 TIME_PARAM,
                 MIX_PARAM,
                 LENGTH_PARAM,
                 MODIFY_BLOOD_PARAM,
                 CLOCK_PARAM,
                 MODIFY_LOOP_PARAM,
                 BLOOD_PROGRAM_PARAM,
                 ROUTING_PARAM,
                 LOOP_PROGRAM_PARAM,
                 BYPASS_BLOOD_PARAM,
                 BYPASS_LOOP_PARAM,
                 NUM_PARAMS
  };
  enum InputIds  {
                  TIME_INPUT,
                  MIX_INPUT,
                  LENGTH_INPUT,
                  MODIFY_BLOOD_INPUT,
                  CLOCK_INPUT,
                  MODIFY_LOOP_INPUT,
                  EXPR_INPUT,
                  BYPASS_BLOOD_INPUT_LOW,
                  BYPASS_BLOOD_INPUT_HIGH,
                  BYPASS_LOOP_INPUT_LOW,
                  BYPASS_LOOP_INPUT_HIGH,
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  BLOOD_LIGHT,
                  ENUMS(LOOP_LIGHT, 2),
                  NUM_LIGHTS
  };

  dsp::SchmittTrigger blood_trigger_low, blood_trigger_high, loop_trigger_low, loop_trigger_high;

  // internal clock measurements
  struct timeval last_blink_time;
  double blink_off_until_usec = 0;
  double blink_off_until_sec = 0;

  Mood() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(TIME_PARAM, 0.f, 127.f, 0.f, "Time");
    configParam(MIX_PARAM, 0.f, 127.f, 0.f, "Mix");
    configParam(LENGTH_PARAM, 0.f, 127.f, 0.f, "Length");
    configParam(MODIFY_BLOOD_PARAM, 0.f, 127.f, 0.f, "Modify (Blood)");
    configParam(CLOCK_PARAM, 0.f, 127.f, 63.f, "Clock");
    configParam(MODIFY_LOOP_PARAM, 0.f, 127.f, 0.f, "Modify (Loop)");

    // three way switches
    // 1.0f is top position
    configParam(BLOOD_PROGRAM_PARAM, 1.0f, 3.0f, 2.0f, "Blood Program (Reverb, Delay, Slip)");
    configParam(ROUTING_PARAM, 1.0f, 3.0f, 2.0f, "Blood Routing (In, Loop+In, Loop)");
    configParam(LOOP_PROGRAM_PARAM, 1.0f, 3.0f, 2.0f, "Loop Program (Env, Tape, Stretch)");

    // bypass buttons
    configParam(BYPASS_BLOOD_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Blood");
    configParam(BYPASS_LOOP_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Loop");

    gettimeofday(&last_blink_time, NULL);
  }

  void process(const ProcessArgs& args) override {
    // only proceed if midi is activated
    if (!midi_out.active()) {
      if (!disable_module()) {
        // turn off the lights if the module is not disabled
        lights[LOOP_LIGHT + 0].setBrightness(0.f);
        lights[LOOP_LIGHT + 1].setBrightness(0.f);
        lights[BLOOD_LIGHT].setBrightness(0.f);
      }
      return;
    } else {
      // enable_module
      enable_module();
    }

    // read the gate triggers
    if (inputs[BYPASS_BLOOD_INPUT_HIGH].isConnected()) {
      if (blood_trigger_high.process(rescale(inputs[BYPASS_BLOOD_INPUT_HIGH].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
        // if the trigger goes high, turn on the blood channel
        params[BYPASS_BLOOD_PARAM].setValue(1.f);
    }
    if (inputs[BYPASS_BLOOD_INPUT_LOW].isConnected()) {
      if (blood_trigger_low.process(rescale(inputs[BYPASS_BLOOD_INPUT_LOW].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
        // if the trigger goes low, turn off the blood channel
        params[BYPASS_BLOOD_PARAM].setValue(0.f);
    }
    if (inputs[BYPASS_LOOP_INPUT_HIGH].isConnected()) {
      if (loop_trigger_high.process(rescale(inputs[BYPASS_LOOP_INPUT_HIGH].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
        // if the trigger goes high, turn on the loop channel
        params[BYPASS_LOOP_PARAM].setValue(1.f);
    }
    if (inputs[BYPASS_LOOP_INPUT_LOW].isConnected()) {
      if (loop_trigger_low.process(rescale(inputs[BYPASS_LOOP_INPUT_LOW].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
        // if the trigger goes low, turn off the loop channel
        params[BYPASS_LOOP_PARAM].setValue(0.f);
    }

    // pedal bypass switches
    int enable_blood = (int) floor(params[BYPASS_BLOOD_PARAM].getValue());
    int enable_loop = (int) floor(params[BYPASS_LOOP_PARAM].getValue());

    // switch values
    int blood_prog = (int) floor(params[BLOOD_PROGRAM_PARAM].getValue());
    int route_prog = (int) floor(params[ROUTING_PARAM].getValue());
    int loop_prog = (int) floor(params[LOOP_PROGRAM_PARAM].getValue());

    // assign values from switches
    midi_out.setValue(blood_prog, 21);
    midi_out.setValue(route_prog, 22);

    // if the loop program is changed, the loop
    // section gets bypassed, so force a bypass
    if (midi_out.setValue(loop_prog, 23)) {
      enable_loop = 0;
      params[BYPASS_LOOP_PARAM].setValue(0.f);
    }

    // read the clock value early so that we can use it for flashing the LEDs
    // at the correct rate.
    int clock = (int) std::round(params[CLOCK_PARAM].getValue());
    if (inputs[CLOCK_INPUT].isConnected()) {
      int clock_cv = (int) std::round(inputs[CLOCK_INPUT].getVoltage()*2) / 10.f * 127;
      clock = clamp(clock_cv, 0, clock);
    }

    int bypass;
    if (enable_loop && enable_blood) {
      bypass = 127;
      // turn loop LED green (on)
      // turn blood LED green
      lights[LOOP_LIGHT + 0].setBrightness(1.f); // turn on green
      lights[LOOP_LIGHT + 1].setBrightness(0.f); // turn off red
      lights[BLOOD_LIGHT].setBrightness(1.f);
    } else if (!enable_loop && enable_blood) {
      bypass = 85;
      // turn loop LED red (off)
      // turn blood LED on (green)
      lights[LOOP_LIGHT + 0].setBrightness(0.f); // turn off green
      lights[BLOOD_LIGHT].setBrightness(1.f);
    } else if (enable_loop && !enable_blood) {
      bypass = 45;
      // turn loop LED green (on)
      // turn blood LED off
      lights[LOOP_LIGHT + 0].setBrightness(1.f); // turn on green
      lights[LOOP_LIGHT + 1].setBrightness(0.f); // turn off red
      lights[BLOOD_LIGHT].setBrightness(0.f);
    } else {
      bypass = 0;
      // turn loop LED red (off)
      // turn blood LED off
      lights[LOOP_LIGHT + 0].setBrightness(0.f); // turn off green
      lights[BLOOD_LIGHT].setBrightness(0.f);
    }

    // bypass the blood and/or loop channels
    midi_out.setValue(bypass, 103);

    // if the loop is not on, flash the loop LED off-to-red
    // based on the sample rate of the clock knob.
    if (!enable_loop) {
      lights[LOOP_LIGHT + 1].setBrightness(flash_loop_led(clock));
    }

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the CV inputs.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // knob values
    int time = (int) std::round(params[TIME_PARAM].getValue());
    int mix = (int) std::round(params[MIX_PARAM].getValue());
    int length = (int) std::round(params[LENGTH_PARAM].getValue());
    int modify_blood = (int) std::round(params[MODIFY_BLOOD_PARAM].getValue());
    int modify_loop = (int) std::round(params[MODIFY_LOOP_PARAM].getValue());
    int expr = -1;

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[TIME_INPUT].isConnected()) {
      int time_cv = (int) std::round(inputs[TIME_INPUT].getVoltage()*2) / 10.f * 127;
      time = clamp(time_cv, 0, time);
    }
    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = (int) std::round(inputs[MIX_INPUT].getVoltage()*2) / 10.f * 127;
      mix = clamp(mix_cv, 0, mix);
    }
    if (inputs[LENGTH_INPUT].isConnected()) {
      int length_cv = (int) std::round(inputs[LENGTH_INPUT].getVoltage()*2) / 10.f * 127;
      length = clamp(length_cv, 0, length);
    }
    if (inputs[MODIFY_BLOOD_INPUT].isConnected()) {
      int modify_blood_cv = (int) std::round(inputs[MODIFY_BLOOD_INPUT].getVoltage()*2) / 10.f * 127;
      modify_blood = clamp(modify_blood_cv, 0, modify_blood);
    }
    if (inputs[MODIFY_LOOP_INPUT].isConnected()) {
      int modify_loop_cv = (int) std::round(inputs[MODIFY_LOOP_INPUT].getVoltage()*2) / 10.f * 127;
      modify_loop = clamp(modify_loop_cv, 0, modify_loop);
    }
    if (inputs[EXPR_INPUT].isConnected()) {
      int expr_cv = (int) std::round(inputs[EXPR_INPUT].getVoltage()*2) / 10.f * 127;
      expr = clamp(expr_cv, 0, 127);
    }

    // assign values from knobs (or cv)
    midi_out.setValue(time, 14);
    midi_out.setValue(mix, 15);
    midi_out.setValue(length, 16);
    midi_out.setValue(modify_blood, 17);
    midi_out.setValue(clock, 18);
    midi_out.setValue(modify_loop, 19);

    // assign value for expression
    if (expr > 0)
      midi_out.setValue(expr, 100);

    return;
  }

  float flash_loop_led(int clock) {
    // clock translation:
    //    116-127 => 64k 0.5s
    //    104-115 => 48k 0.75s
    //    92-103  => 32k 1s
    //    80-91   => 24k 1.5s
    //    68-79   => 16k 2s
    //    58-67   => 12k 3s
    //    46-57   => 8k  4s
    //    34-45   => 6k  6s
    //    22-33   => 4k  8s
    //    11-21   => 3k  12s
    //    0-10    => 2k  16s

    float blink_rate;
    if (clock >= 116)
      blink_rate = 0.5f;
    else if (clock >= 104)
      blink_rate = 0.75f;
    else if (clock >= 92)
      blink_rate = 1.0f;
    else if (clock >= 80)
      blink_rate = 1.5f;
    else if (clock >= 68)
      blink_rate = 2.0f;
    else if (clock >= 58)
      blink_rate = 3.0f;
    else if (clock >= 46)
      blink_rate = 4.0f;
    else if (clock >= 34)
      blink_rate = 6.0f;
    else if (clock >= 22)
      blink_rate = 8.0f;
    else if (clock >= 11)
      blink_rate = 12.0f;
    else
      blink_rate = 16.0f;

    // collect a current time stamp and compare it to a previously stored time stamp
    // measure the amount of time from the previous tap tempo to this tap tempo.
    // this will be the tap tempo rate (for the tap tempo LED).
    double last_time_usec = (double) last_blink_time.tv_usec;
    double last_time_sec = (double) last_blink_time.tv_sec;

    // get a new measurement
    struct timeval ctime;
    gettimeofday(&ctime, NULL);
    double this_time_usec = (double) ctime.tv_usec;
    double this_time_sec = (double) ctime.tv_sec;

    // determine if we are in the blink off period by checking
    // if the current time has not breached the blink off time window
    double blink_off_diff_sec = this_time_sec - blink_off_until_sec;
    double blink_off_diff_usec = this_time_usec - blink_off_until_usec;
    float blink_off_diff = (float) (blink_off_diff_sec + (blink_off_diff_usec/1000000));
    if (blink_off_diff < 0) {
      // we are in the quiet period
      return 0.f;
    }

    // calculate how much time has gone by
    double diff_sec = this_time_sec - last_time_sec;
    double diff_usec = this_time_usec - last_time_usec;
    float diff = (float) (diff_sec + (diff_usec/1000000));

    // if the time diff has exceeded the blink rate, then make now be the last time
    // we blinked, and return a brightness of 0
    if (diff > blink_rate) {
      last_blink_time = ctime;
      blink_off_until_usec = (double) this_time_usec + 100000;
      blink_off_until_sec = (double) this_time_sec;
      return 0.f;
    } else {
      // we don't need to blink yet, keep the light on
      return 1.f;
    }
  }

};

struct MoodWidget : ModuleWidget {
  MoodWidget(Mood* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/mood_panel.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnobMood>(mm2px(Vec(10, 12)), module, Mood::TIME_PARAM));
    addParam(createParamCentered<CBAKnobMood>(mm2px(Vec(30, 12)), module, Mood::MIX_PARAM));
    addParam(createParamCentered<CBAKnobMood>(mm2px(Vec(50, 12)), module, Mood::LENGTH_PARAM));
    addParam(createParamCentered<CBAKnobMood>(mm2px(Vec(10, 40)), module, Mood::MODIFY_BLOOD_PARAM));
    addParam(createParamCentered<CBAKnobMood>(mm2px(Vec(30, 40)), module, Mood::CLOCK_PARAM));
    addParam(createParamCentered<CBAKnobMood>(mm2px(Vec(50, 40)), module, Mood::MODIFY_LOOP_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 25)), module, Mood::TIME_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 25)), module, Mood::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 25)), module, Mood::LENGTH_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 53)), module, Mood::MODIFY_BLOOD_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 53)), module, Mood::CLOCK_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 53)), module, Mood::MODIFY_LOOP_INPUT));

    // expression port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(43.5, 92)), module, Mood::EXPR_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(7, 66)), module, Mood::BLOOD_PROGRAM_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(17, 66)), module, Mood::ROUTING_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(47, 66)), module, Mood::LOOP_PROGRAM_PARAM));

    // blood channel led / bypass / high & low gate
    addChild(createLightCentered<LargeLight<GreenLight>>(mm2px(Vec(15, 109)), module, Mood::BLOOD_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(15, 118)), module, Mood::BYPASS_BLOOD_PARAM));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25, 109)), module, Mood::BYPASS_BLOOD_INPUT_HIGH));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25, 118)), module, Mood::BYPASS_BLOOD_INPUT_LOW));

    // loop channel led / bypass / high & low gate
    addChild(createLightCentered<LargeLight<GreenRedLight>>(mm2px(Vec(46, 109)), module, Mood::LOOP_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(46, 118)), module, Mood::BYPASS_LOOP_PARAM));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 109)), module, Mood::BYPASS_LOOP_INPUT_HIGH));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 118)), module, Mood::BYPASS_LOOP_INPUT_LOW));

    // midi configuration displays
    RRMidiWidget* midiWidget = createWidget<RRMidiWidget>(mm2px(Vec(3, 75)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};

Model* modelMood = createModel<Mood, MoodWidget>("mood");
