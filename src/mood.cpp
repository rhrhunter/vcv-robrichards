#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "midi_classes.hpp"

struct Mood : Module {
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
                 MIDI_CHANNEL_PARAM,
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
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  BLOOD_LIGHT,
                  ENUMS(LOOP_LIGHT, 2),
                  NUM_LIGHTS
  };

  RRMidiOutput midi_out;

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

    // midi configuration knobs
    configParam(MIDI_CHANNEL_PARAM, 1.f, 16.f, 5.f, "MIDI Channel");

    // bypass buttons
    configParam(BYPASS_BLOOD_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Blood");
    configParam(BYPASS_LOOP_PARAM, 0.f, 1.f, 0.f, "Enable/Bypass Loop");

    // DeviceIds start counting from 0, not 1
    midi_out.setDeviceId(3);
    midi_out.setChannel(5);
  }

  void process(const ProcessArgs& args) override {
    // configure the midi channel, return if it is not set
    int channel = (int) floor(params[MIDI_CHANNEL_PARAM].getValue() + 0.5);
    if (channel <= 0)
      return;
    else
      midi_out.setChannel(channel);

    // knob values
    int time = (int) floor(params[TIME_PARAM].getValue() + 0.5);
    int mix = (int) floor(params[MIX_PARAM].getValue() + 0.5);
    int length = (int) floor(params[LENGTH_PARAM].getValue() + 0.5);
    int modify_blood = (int) floor(params[MODIFY_BLOOD_PARAM].getValue() + 0.5);
    int clock = (int) floor(params[CLOCK_PARAM].getValue() + 0.5);
    int modify_loop = (int) floor(params[MODIFY_LOOP_PARAM].getValue() + 0.5);

    // switch values
    int blood_prog = (int) floor(params[BLOOD_PROGRAM_PARAM].getValue());
    int route_prog = (int) floor(params[ROUTING_PARAM].getValue());
    int loop_prog = (int) floor(params[LOOP_PROGRAM_PARAM].getValue());

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[TIME_INPUT].isConnected()) {
      int time_cv = (int) floor((inputs[TIME_INPUT].getVoltage() * 16.9f));
      if (time_cv > time)
        time_cv = time;
      time = time_cv;
    }

    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = (int) floor((inputs[MIX_INPUT].getVoltage() * 16.9f));
      if (mix_cv > mix)
        mix_cv = mix;
      mix = mix_cv;
    }

    if (inputs[LENGTH_INPUT].isConnected()) {
      int length_cv = (int) floor((inputs[LENGTH_INPUT].getVoltage() * 16.9f));
      if (length_cv > length)
        length_cv = length;
      length = length_cv;
    }

    if (inputs[CLOCK_INPUT].isConnected()) {
      int clock_cv = (int) floor((inputs[CLOCK_INPUT].getVoltage() * 16.9f));
      if (clock_cv > clock)
        clock_cv = clock;
      clock = clock_cv;
    }

    if (inputs[MODIFY_BLOOD_INPUT].isConnected()) {
      int modify_blood_cv = (int) floor((inputs[MODIFY_BLOOD_INPUT].getVoltage() * 16.9f));
      if (modify_blood_cv > modify_blood)
        modify_blood_cv = modify_blood;
      modify_blood = modify_blood_cv;
    }

    if (inputs[MODIFY_LOOP_INPUT].isConnected()) {
      int modify_loop_cv = (int) floor((inputs[MODIFY_LOOP_INPUT].getVoltage() * 16.9f));
      if (modify_loop_cv > modify_loop)
        modify_loop_cv = modify_loop;
      modify_loop = modify_loop_cv;
    }

    int enable_blood = (int) floor(params[BYPASS_BLOOD_PARAM].getValue());
    int enable_loop = (int) floor(params[BYPASS_LOOP_PARAM].getValue());

    // assign values from knobs (or cv)
    midi_out.setValue(time, 14);
    midi_out.setValue(mix, 15);
    midi_out.setValue(length, 16);
    midi_out.setValue(modify_blood, 17);
    midi_out.setValue(clock, 18);
    midi_out.setValue(modify_loop, 19);

    // assign values from switches
    midi_out.setValue(blood_prog, 21);
    midi_out.setValue(route_prog, 22);

    // if the loop program is changed, the loop
    // section gets bypassed, so force a bypass
    if (midi_out.setValue(loop_prog, 23)) {
      enable_loop = 0;
      params[BYPASS_LOOP_PARAM].setValue(0.f);
    }

    int bypass;
    if (enable_loop && enable_blood) {
      bypass = 127;
      // turn loop LED green (on)
      // turn blood LED green
      lights[LOOP_LIGHT + 0].setBrightness(1.f);
      lights[LOOP_LIGHT + 1].setBrightness(0.f);
      lights[BLOOD_LIGHT].setBrightness(1.f);
    } else if (!enable_loop && enable_blood) {
      bypass = 85;
      // turn loop LED red (off)
      // turn blood LED on (green)
      lights[LOOP_LIGHT + 0].setBrightness(0.f);
      lights[LOOP_LIGHT + 1].setBrightness(1.f);
      lights[BLOOD_LIGHT].setBrightness(1.f);
    } else if (enable_loop && !enable_blood) {
      bypass = 45;
      // turn loop LED green (on)
      // turn blood LED off
      lights[LOOP_LIGHT + 0].setBrightness(1.f);
      lights[LOOP_LIGHT + 1].setBrightness(0.f);
      lights[BLOOD_LIGHT].setBrightness(0.f);
    } else {
      bypass = 0;
      // turn loop LED red (off)
      // turn blood LED off
      lights[LOOP_LIGHT + 0].setBrightness(0.f);
      lights[LOOP_LIGHT + 1].setBrightness(1.f);
      lights[BLOOD_LIGHT].setBrightness(0.f);
    }

    // bypass the blood and/or loop channels
    midi_out.setValue(bypass, 103);
  }
};


struct MoodWidget : ModuleWidget {
  MoodWidget(Mood* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/mood_blank.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 15)), module, Mood::TIME_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 15)), module, Mood::MIX_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 15)), module, Mood::LENGTH_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 50)), module, Mood::MODIFY_BLOOD_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 50)), module, Mood::CLOCK_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 50)), module, Mood::MODIFY_LOOP_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 30)), module, Mood::TIME_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 30)), module, Mood::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 30)), module, Mood::LENGTH_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 65)), module, Mood::MODIFY_BLOOD_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 65)), module, Mood::CLOCK_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 65)), module, Mood::MODIFY_LOOP_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 80)), module, Mood::BLOOD_PROGRAM_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(30, 80)), module, Mood::ROUTING_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(50, 80)), module, Mood::LOOP_PROGRAM_PARAM));

    // bypass switches and Leds
    addChild(createLightCentered<LargeLight<GreenLight>>(mm2px(Vec(15.0, 109)), module, Mood::BLOOD_LIGHT));
    addParam(createParamCentered<CBAButton>(mm2px(Vec(15, 118)), module, Mood::BYPASS_BLOOD_PARAM));
    addChild(createLightCentered<LargeLight<GreenRedLight>>(mm2px(Vec(46, 109)), module, Mood::LOOP_LIGHT));
    addParam(createParamCentered<CBAButton>(mm2px(Vec(46, 118)), module, Mood::BYPASS_LOOP_PARAM));

    // midi configuration displays
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 100)), module, Mood::MIDI_CHANNEL_PARAM));
    MidiChannelDisplay *mcd = new MidiChannelDisplay();
    mcd->box.pos = Vec(50, 285);
    mcd->box.size = Vec(50, 20);
    mcd->value = &((module->midi_out).midi_channel);
    mcd->module = (void *) module;
    addChild(mcd);

  }
};

Model* modelMood = createModel<Mood, MoodWidget>("mood");
