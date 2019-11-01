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
  enum LightIds  {
                  TAP_TEMPO_LIGHT,
                  BYPASS_LIGHT,
                  NUM_LIGHTS
  };

  RRMidiOutput midi_out;
  bool can_tap_tempo;
  struct timeval last_tap_tempo_time;
  double next_blink_usec;
  double next_blink_sec;
  bool start_blinking;
  bool first_tap;
  double curr_rate_sec;
  double curr_rate_usec;
  float rateLimiterPhase = 0.f;

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

    // initialize the tap tempo LED blink measurements
    next_blink_usec = 0;
    next_blink_sec = 0;
    start_blinking = false;
    first_tap = false;
    curr_rate_sec = 0;
    curr_rate_usec = 0;
  }

  void process(const ProcessArgs& args) override {
    // configure the midi channel, return if it is not set
    int channel = (int) floor(params[MIDI_CHANNEL_PARAM].getValue() + 0.5);
    if (channel <= 0)
      return;
    else
      midi_out.setChannel(channel);

    // handle a clock message
    if (inputs[CLOCK_INPUT].isConnected()) {
      bool clock = inputs[CLOCK_INPUT].getVoltage() >= 1.f;
      midi_out.setClock(clock);
    }

    // determine if tap tempo has been pressed
    int tap_tempo = (int) floor(params[TAP_TEMPO_PARAM].getValue());

    // if the tap tempo button was pressed, force a midi message to be sent
    if (can_tap_tempo && tap_tempo) {
      // we are allowing tap tempo actions and they tapped the tempo button
      midi_out.lastMidiCCValues[93] = -1;
      midi_out.setValue(1, 93);
      can_tap_tempo = false;

      // measure the amount of time from the previous tap tempo to this tap tempo.
      // this will be the tap tempo rate (for the tap tempo LED).
      double last_time_usec = (double) last_tap_tempo_time.tv_usec;
      double last_time_sec = (double) last_tap_tempo_time.tv_sec;

      // get a new measurement
      struct timeval ctime;
      gettimeofday(&ctime, NULL);
      double this_time_usec = (double) ctime.tv_usec;
      double this_time_sec = (double) ctime.tv_sec;

      // calculate the next time we need to blink
      next_blink_usec = (this_time_usec - last_time_usec) + this_time_usec;
      next_blink_sec = (this_time_sec - last_time_sec) + this_time_sec;

      // update the current time
      last_tap_tempo_time = ctime;

      // keep track of whether two taps have occurred so far
      if (!start_blinking && !first_tap) {
        // first tap occurred
        first_tap = true;
      } else if (!start_blinking && first_tap) {
        // second tap occurred, start blinking the light
        start_blinking = true;
      }

      // calculate the blink rate based on the last two taps
      // if we were told to start blinking
      if (start_blinking) {
        curr_rate_sec = next_blink_sec - this_time_sec;
        curr_rate_usec = next_blink_usec - this_time_usec;
      }

    } else if (tap_tempo) {
      // they wanted to do a tap tempo, but they did it too fast.
      // calculate if we are allowed to tap next time
      struct timeval ctime;
      gettimeofday(&ctime, NULL);

      // calculate if enough time has elapsed
      double elapsed = (double)(ctime.tv_usec - last_tap_tempo_time.tv_usec) / 1000000 +
        (double)(ctime.tv_sec - last_tap_tempo_time.tv_sec);
      if (elapsed > 0.2)
        can_tap_tempo = true;
    } else if (start_blinking) {
      // no tap tempo button was clicked and we have a stored "next blink time",
      // determine if we should blink the tempo light right now
      struct timeval ctime;
      gettimeofday(&ctime, NULL);

      // if the current time is greater than the next blink time, flash the light.
      // the next process iteration will turn it off
      double this_time_usec = (double) ctime.tv_usec;
      double this_time_sec = (double) ctime.tv_sec;
      double elapsed_usec = (double) (this_time_usec - next_blink_usec);
      double elapsed_sec = (double) (this_time_sec - next_blink_sec);
      double elapsed = (double) ((elapsed_sec) + (elapsed_usec / 1000000));
      if (elapsed > 0) {
        // flash the tap tempo light
        lights[TAP_TEMPO_LIGHT].setBrightness(1.f);

        // store the current time for the next blink, add rate and subtract the amount we went over
        // because this accounts for the drift we may have experienced.
        next_blink_usec = (this_time_usec + curr_rate_usec) - elapsed_usec;
        next_blink_sec = (this_time_sec + curr_rate_sec) - elapsed_sec;

        // slightly scale back the rate limit phase so that we dont shut off the light
        // too quickly
        rateLimiterPhase -= 0.15f;
      }
      else {
        // we dont need to flash the light yet,
        // allow the light to stay on for at least 300ms if it is already on
        const float rateLimiterPeriod = 0.3f;
        rateLimiterPhase += args.sampleTime / rateLimiterPeriod;
        if (rateLimiterPhase >= 1.f) {
          rateLimiterPhase -= 1.f;
          lights[TAP_TEMPO_LIGHT].setBrightness(0.f);
        }
      }
    }

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
    if (enable_pedal) {
      // turn bypass light on (red)
      lights[BYPASS_LIGHT].setBrightness(1.f);
      bypass = 127;
    } else {
      lights[BYPASS_LIGHT].setBrightness(0.f);
      bypass = 0;
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
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 15)), module, WarpedVinyl::TONE_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 15)), module, WarpedVinyl::LAG_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 15)), module, WarpedVinyl::MIX_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 50)), module, WarpedVinyl::RPM_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 50)), module, WarpedVinyl::DEPTH_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 50)), module, WarpedVinyl::WARP_PARAM));

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
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(15, 109)), module, WarpedVinyl::TAP_TEMPO_LIGHT));
    addParam(createParamCentered<CBAMomentaryButtonGray>(mm2px(Vec(15, 118)), module, WarpedVinyl::TAP_TEMPO_PARAM));
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(46, 109)), module, WarpedVinyl::BYPASS_LIGHT));
    addParam(createParamCentered<CBAButtonGray>(mm2px(Vec(46, 118)), module, WarpedVinyl::BYPASS_PARAM));

    // midi configuration displays
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 100)), module, WarpedVinyl::MIDI_CHANNEL_PARAM));
    MidiChannelDisplay *mcd = new MidiChannelDisplay();
    mcd->box.pos = Vec(50, 285);
    mcd->box.size = Vec(32, 20);
    mcd->value = &((module->midi_out).midi_channel);
    mcd->module = (void *) module;
    addChild(mcd);

  }
};


Model* modelWarpedVinyl = createModel<WarpedVinyl, WarpedVinylWidget>("warpedvinyl");
