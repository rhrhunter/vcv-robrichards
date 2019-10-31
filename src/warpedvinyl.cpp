#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "midi_classes.hpp"

#include <sys/time.h>

struct WarpedVinyl : Module {
  enum ParamIds {
                 TONE_PARAM,
                 LAG_PARAM,
                 MIX_PARAM,
                 RPM_PARAM,
                 DEPTH_PARAM,
                 WARP_PARAM,
                 TAP_DIVISION_PARAM,
                 MIDI_CHANNEL_PARAM,
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
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  { NUM_LIGHTS };

  RRMidiOutput midi_out;
  bool can_tap_tempo;
  struct timeval last_tap_tempo_time;

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
    configParam(TAP_DIVISION_PARAM, 0.0f, 5.0f, 0.0f, "Tap Divisions (1,2,4,3,6,8)");

    // midi configuration knobs
    configParam(MIDI_CHANNEL_PARAM, 1.f, 16.f, 3.f, "MIDI Channel");

    // bypass button
    configParam(BYPASS_PARAM, 0.f, 1.f, 0.f, "Pedal Bypass");

    // tap tempo buttons
    configParam(TAP_TEMPO_PARAM, 0.f, 1.f, 0.f, "Tap Tempo");

    // DeviceIds start counting from 0, not 1
    midi_out.setDeviceId(3);
    midi_out.setChannel(3);

    // initialize whether we allow tap tempo messages or not
    can_tap_tempo = true;

    // get the current time, this is so we can keep throttle tap tempo messages
    gettimeofday(&last_tap_tempo_time, NULL);
  }

  void process(const ProcessArgs& args) override {
    // configure the midi channel, return if it is not set
    int channel = (int) floor(params[MIDI_CHANNEL_PARAM].getValue() + 0.5);
    if (channel <= 0)
      return;
    else
      midi_out.setChannel(channel);

    // knob values
    int tone = (int) floor(params[TONE_PARAM].getValue() + 0.5);
    int lag = (int) floor(params[LAG_PARAM].getValue() + 0.5);
    int mix = (int) floor(params[MIX_PARAM].getValue() + 0.5);
    int rpm = (int) floor(params[RPM_PARAM].getValue() + 0.5);
    int depth = (int) floor(params[DEPTH_PARAM].getValue() + 0.5);
    int warp = (int) floor(params[WARP_PARAM].getValue() + 0.5);

    // left switch values (0,1,2,3,4,5)
    int tap_division = (int) floor(params[TAP_DIVISION_PARAM].getValue());

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[TONE_INPUT].isConnected()) {
      int tone_cv = (int) floor((inputs[TONE_INPUT].getVoltage() * 16.9f));
      if (tone_cv > tone)
        tone_cv = tone;
      tone = tone_cv;
    }

    if (inputs[LAG_INPUT].isConnected()) {
      int lag_cv = (int) floor((inputs[LAG_INPUT].getVoltage() * 16.9f));
      if (lag_cv > lag)
        lag_cv = lag;
      lag = lag_cv;
    }

    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = (int) floor((inputs[MIX_INPUT].getVoltage() * 16.9f));
      if (mix_cv > mix)
        mix_cv = mix;
      mix = mix_cv;
    }

    if (inputs[RPM_INPUT].isConnected()) {
      int rpm_cv = (int) floor((inputs[RPM_INPUT].getVoltage() * 16.9f));
      if (rpm_cv > rpm)
        rpm_cv = rpm;
      rpm = rpm_cv;
    }

    if (inputs[DEPTH_INPUT].isConnected()) {
      int depth_cv = (int) floor((inputs[DEPTH_INPUT].getVoltage() * 16.9f));
      if (depth_cv > depth)
        depth_cv = depth;
      depth = depth_cv;
    }

    if (inputs[WARP_INPUT].isConnected()) {
      int warp_cv = (int) floor((inputs[WARP_INPUT].getVoltage() * 16.9f));
      if (warp_cv > warp)
        warp_cv = warp;
      warp = warp_cv;
    }

    // bypass or enable the pedal
    int enable_pedal = (int) floor(params[BYPASS_PARAM].getValue());
    int bypass;
    if (enable_pedal)
      bypass = 127;
    else
      bypass = 0;

    // handle a clock message
    if (inputs[CLOCK_INPUT].isConnected()) {
      bool clock = inputs[CLOCK_INPUT].getVoltage() >= 1.f;
      midi_out.setClock(clock);
    }

    // assign values from knobs (or cv)
    midi_out.setValue(tone, 14);
    midi_out.setValue(lag, 15);
    midi_out.setValue(mix, 16);
    midi_out.setValue(rpm, 17);
    midi_out.setValue(depth, 18);
    midi_out.setValue(warp, 19);

    // assign values from switches
    midi_out.setValue(tap_division, 21);

    // enable or bypass the pedal
    midi_out.setValue(bypass, 102);

    // determine if tap tempo has been pressed
    int tap_tempo = (int) floor(params[TAP_TEMPO_PARAM].getValue());
    
    // if the tap tempo button was pressed, force a midi message to be sent
    if (can_tap_tempo && tap_tempo) {
      // we are allowing tap tempo actions and they tapped the tempo button
      midi_out.lastMidiCCValues[93] = -1;
      midi_out.setValue(1, 93);
      can_tap_tempo = false;

      // take a time stamp of the last tap tempo that was performed
      gettimeofday(&last_tap_tempo_time, NULL);
    } else if (tap_tempo) {
      // they wanted to do a tap tempo, but they did it too fast.
      // calculate if we are allowed to tap next time
      struct timeval ctime;
      gettimeofday(&ctime, NULL);

      // calculate if enough time has elapsed
      double elapsed = (double)(ctime.tv_usec - last_tap_tempo_time.tv_usec) / 1000000 +
        (double)(ctime.tv_sec - last_tap_tempo_time.tv_sec);
      if (elapsed > 0.05)
        can_tap_tempo = true;
    }
  }
};

struct WarpedVinylWidget : ModuleWidget {
  WarpedVinylWidget(WarpedVinyl* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/warpedvinyl_blank.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<DWKnob>(mm2px(Vec(10, 15)), module, WarpedVinyl::TONE_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(30, 15)), module, WarpedVinyl::LAG_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(50, 15)), module, WarpedVinyl::MIX_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(10, 50)), module, WarpedVinyl::RPM_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(30, 50)), module, WarpedVinyl::DEPTH_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(50, 50)), module, WarpedVinyl::WARP_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 30)), module, WarpedVinyl::TONE_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 30)), module, WarpedVinyl::LAG_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 30)), module, WarpedVinyl::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 65)), module, WarpedVinyl::RPM_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 65)), module, WarpedVinyl::DEPTH_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 65)), module, WarpedVinyl::WARP_INPUT));

    // clock port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(50, 100)), module, WarpedVinyl::CLOCK_INPUT));
    
    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 80)), module, WarpedVinyl::TAP_DIVISION_PARAM));

    // bypass switches & tap tempo
    addParam(createParamCentered<CBAMomentaryButtonRed>(mm2px(Vec(15, 118)), module, WarpedVinyl::TAP_TEMPO_PARAM));
    addParam(createParamCentered<CBAButtonRed>(mm2px(Vec(46, 118)), module, WarpedVinyl::BYPASS_PARAM));

    // midi configuration displays
    addParam(createParamCentered<DWKnob>(mm2px(Vec(10, 100)), module, WarpedVinyl::MIDI_CHANNEL_PARAM));
    MidiChannelDisplay *mcd = new MidiChannelDisplay();
    mcd->box.pos = Vec(50, 285);
    mcd->box.size = Vec(50, 20);
    mcd->value = &((module->midi_out).midi_channel);
    mcd->module = (void *) module;
    addChild(mcd);

  }
};


Model* modelWarpedVinyl = createModel<WarpedVinyl, WarpedVinylWidget>("warpedvinyl");
