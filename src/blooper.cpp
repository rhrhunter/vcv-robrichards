#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "rr_module.hpp"
#include "rr_midiwidget.hpp"
#include <dsp/digital.hpp>
#include <sys/time.h>

struct Blooper : RRModule {
  enum ParamIds {
                 VOLUME_PARAM,
                 LAYERS_PARAM,
                 REPEATS_PARAM,
                 MODA_PARAM,
                 STABILITY_PARAM,
                 MODB_PARAM,
                 L_TOGGLE_PARAM,
                 M_TOGGLE_PARAM,
                 R_TOGGLE_PARAM,
                 TOGGLE_MODA_PARAM,
                 TOGGLE_MODB_PARAM,
                 RECORD_LOOP_PARAM,
                 PLAY_LOOP_PARAM,
                 STOP_LOOP_PARAM,
                 ERASE_LOOP_PARAM,
                 TOGGLE_ONE_SHOT_RECORD_PARAM,
                 TOGGLE_RAMP_PARAM,
                 RAMP_PARAM,
                 LOOP_SELECT_INCR_PARAM,
                 LOOP_SELECT_DECR_PARAM,
                 NUM_PARAMS
  };
  enum InputIds  {
                  VOLUME_INPUT,
                  LAYERS_INPUT,
                  REPEATS_INPUT,
                  MODA_INPUT,
                  STABILITY_INPUT,
                  MODB_INPUT,
                  CLOCK_INPUT,
                  EXPR_INPUT,
                  RAMP_INPUT,
                  STOP_GATE_INPUT,
                  PLAY_GATE_INPUT,
                  RECORD_GATE_INPUT,
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  ENUMS(LEFT_LIGHT, 2),
                  ENUMS(RIGHT_LIGHT, 2),
                  NUM_LIGHTS
  };

  // internal clock measurements
  struct timeval last_blink_time;
  double blink_off_until_usec = 0;
  double blink_off_until_sec = 0;

  // blooper state machine
  int bypass_state;

  // loop select program change number
  bool program_change;

  // cached next value for modA and modB toggles
  int next_moda_toggle_value = 1;
  int next_modb_toggle_value = 1;

  // grace period timevals
  struct timeval erase_grace_period, one_shot_grace_period;
  struct timeval mod_toggle_grace_period, loop_select_grace_period;

  // gate triggers
  dsp::SchmittTrigger stop_gate_trigger, play_gate_trigger, record_gate_trigger;

  Blooper() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(VOLUME_PARAM, 0.f, 127.f, 64.f, "Loop Volume");
    configParam(LAYERS_PARAM, 0.f, 127.f, 127.f, "Layers");
    configParam(REPEATS_PARAM, 0.f, 127.f, 127.f, "Repeats");
    configParam(MODA_PARAM, 0.f, 127.f, 64.f, "Mod A");
    configParam(STABILITY_PARAM, 0.f, 127.f, 0.f, "Stability");
    configParam(MODB_PARAM, 0.f, 127.f, 64.f, "Mod B");

    // ramp knob
    configParam(RAMP_PARAM, 0.f, 127.f, 0.f, "Ramp Amount");

    // toggle ramping switch (default off)
    configParam(TOGGLE_RAMP_PARAM, 0.f, 1.f, 0.f, "Enable/Disable Ramping");

    // 3 way switches
    // 1.0f is top position
    configParam(L_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Modifier A (Smooth Speed, Dropper, Trimmer)");
    configParam(M_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Loop Program (Normal, Additive, Sampler)");
    configParam(R_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Modifier B (Stepped Speed, Scrambler, Filter)");

    // front buttons
    configParam(TOGGLE_MODA_PARAM, 0.0f, 1.0f, 0.0f, "Modifier A (enable/disable)");
    configParam(TOGGLE_MODB_PARAM, 0.0f, 1.0f, 0.0f, "Modifier B (enable/disable)");

    // buttons
    configParam(RECORD_LOOP_PARAM, 0.f, 1.f, 0.f, "Record/Overdub");
    configParam(TOGGLE_ONE_SHOT_RECORD_PARAM, 0.f, 1.f, 0.f, "Toggle One Shot Record (on/off)");
    configParam(PLAY_LOOP_PARAM, 0.f, 1.f, 0.f, "Play");
    configParam(STOP_LOOP_PARAM, 0.f, 1.f, 0.f, "Stop");
    configParam(ERASE_LOOP_PARAM, 0.f, 1.f, 0.f, "Erase");

    // pedal bypass state (play, recording, overdubbing, stopped, erasing, etc...)
    bypass_state = 0;

    // upon initialization, we won't reset the program change loop
    program_change = false;

    // blinking rate
    gettimeofday(&last_blink_time, NULL);

    // initialize te mod toggle grace period
    gettimeofday(&mod_toggle_grace_period, NULL);

    // initialize the loop select grace period
    gettimeofday(&loop_select_grace_period, NULL);
  }

  void record() {
    // disable one shot record
    reset_one_shot(false);

    // send a record message
    midi_out.setValue(1, 11);
  }

  void play() {
    // disable one shot record
    reset_one_shot(false);

    // send a play message
    midi_out.setValue(2, 11);
  }

  void over_dub() {
    // disable one shot record
    reset_one_shot(false);

    // send an over dub message
    midi_out.setValue(3, 11);
  }

  void stop() {
    // disable one shot record
    reset_one_shot(false);

    // send a stop message
    midi_out.setValue(4, 11);
  }

  void erase() {
    // disable one shot record
    reset_one_shot(false);

    // send an erase message
    midi_out.setValue(7, 11);

    // collect a timestamp to know when the
    // erase grace period starts
    gettimeofday(&erase_grace_period, NULL);
  }

  void one_shot_record() {
    // enable one shot record
    midi_out.setValue(1, 9);

    // collect a timestamp to know when the one shot grace period starts
    gettimeofday(&one_shot_grace_period, NULL);
  }

  void reset_one_shot(bool reset_cache) {
    if (reset_cache)
      midi_out.resetCCCache(9);
    midi_out.setValue(0, 9);
  }

  float flash_led(float blink_rate) {
    // collect a current time stamp and compare it to a previously stored time stamp
    // measure the amount of time from the previous LED flash
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

  void process(const ProcessArgs& args) override {
    // only proceed if midi is activated
    if (!midi_out.active()) {
      if (!disable_module()) {
        // turn off the lights if the module is not disabled
        lights[LEFT_LIGHT].setBrightness(0.f);
        lights[LEFT_LIGHT+1].setBrightness(0.f);
        lights[RIGHT_LIGHT].setBrightness(0.f);
        lights[RIGHT_LIGHT+1].setBrightness(0.f);
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

    // 3way switch values (1,2,3)
    int l_toggle = (int) floor(params[L_TOGGLE_PARAM].getValue());
    int m_toggle = (int) floor(params[M_TOGGLE_PARAM].getValue());
    int r_toggle = (int) floor(params[R_TOGGLE_PARAM].getValue());

    // assign values from switches
    midi_out.setValue(l_toggle, 21);
    midi_out.setValue(m_toggle, 22);
    midi_out.setValue(r_toggle, 23);

    // toggle the either modifier on or off
    int moda_toggle = (int) floor(params[TOGGLE_MODA_PARAM].getValue());
    if (moda_toggle) {
      // allow a 250ms grace period between button presses to prevent spam
      if (should_transition_to_state(0.25f, mod_toggle_grace_period)) {
        midi_out.setValue(next_moda_toggle_value, 30);
        if (next_moda_toggle_value == 1)
          next_moda_toggle_value = 127;
        else
          next_moda_toggle_value = 1;

        // remember for next time how long it as been since the toggle was pressed
        gettimeofday(&mod_toggle_grace_period, NULL);
      }
    }

    int modb_toggle = (int) floor(params[TOGGLE_MODB_PARAM].getValue());
    if (modb_toggle) {
      // allow a 250ms grace period between button presses to prevent spam
      if (should_transition_to_state(0.25f, mod_toggle_grace_period)) {
        midi_out.setValue(next_modb_toggle_value, 31);
        if (next_modb_toggle_value == 1)
          next_modb_toggle_value = 127;
        else
          next_modb_toggle_value = 1;

        // remember for next time how long it as been since the toggle was pressed
        gettimeofday(&mod_toggle_grace_period, NULL);
      }
    }

    // check if the preset button was pressed (protect it from being spammed)
    int loop_change_incr = (int) floor(params[LOOP_SELECT_INCR_PARAM].getValue());
    int loop_change_decr = (int) floor(params[LOOP_SELECT_DECR_PARAM].getValue());

    // only process the loop change if either button was pressed and we are not already
    // performing a loop change request (bypass_state == 6)
    if ((loop_change_incr || loop_change_decr) && bypass_state != 6) {
      // allow a 1s grace period between loop changes
      if (should_transition_to_state(1.0f, loop_select_grace_period)) {
        // stop any existing loops before we do the loop change
        stop();

        // transition to state 6, wich means we are "loading a loop"
        bypass_state = 6;

        if (!program_change) {
          // if we've never done a program change, jump straight to program 0
          midi_out.setProgram(0);
          program_change = true;
        } else {
          // increment or decrement by 1 the program
          // with a max of 16 programs (loops).
          if (loop_change_incr)
            midi_out.incrementProgram(1, 16);
          else
            midi_out.decrementProgram(1, 16);
        }

        // remember for next time when we performed the loop change
        gettimeofday(&loop_select_grace_period, NULL);
      }
    }

    // toggle the lights based on the current state
    if (bypass_state == 0) {
      // pedal is not playing / unknown state
      lights[LEFT_LIGHT + 0].setBrightness(0.f);
      lights[LEFT_LIGHT + 1].setBrightness(0.f);
      lights[RIGHT_LIGHT].setBrightness(0.f);
      lights[RIGHT_LIGHT + 1].setBrightness(0.f);
    } else if (bypass_state == 1) {
      // recording so light will be red
      lights[LEFT_LIGHT].setBrightness(0.f);
      lights[LEFT_LIGHT + 1].setBrightness(1.f);
      lights[RIGHT_LIGHT].setBrightness(0.f);
      lights[RIGHT_LIGHT + 1].setBrightness(0.f);
    } else if (bypass_state == 2) {
      // recording is playing so light will be green
      lights[LEFT_LIGHT].setBrightness(1.f);
      lights[LEFT_LIGHT + 1].setBrightness(0.f);
      lights[RIGHT_LIGHT].setBrightness(0.f);
      lights[RIGHT_LIGHT + 1].setBrightness(0.f);
    } else if (bypass_state == 3) {
      // recording is stopped, so flash green
      lights[LEFT_LIGHT].setBrightness(flash_led(0.50f));
      lights[LEFT_LIGHT + 1].setBrightness(0.f);
      lights[RIGHT_LIGHT].setBrightness(0.f);
      lights[RIGHT_LIGHT + 1].setBrightness(0.f);
    } else if (bypass_state == 4) {
      // recording is being deleted, flash both lights red for 2s
      lights[LEFT_LIGHT+1].setBrightness(flash_led(0.30f));
      lights[RIGHT_LIGHT+1].setBrightness(flash_led(0.30f));

      // turn off the green lights
      lights[LEFT_LIGHT].setBrightness(0);
      lights[RIGHT_LIGHT].setBrightness(0);

      // if 2s has passed since we started deleting, transition to off state
      if (should_transition_to_state(2.0f, erase_grace_period))
        bypass_state = 0;
    } else if (bypass_state == 5) {
      // a one shot recording operation is in progress, flash the left led red for
      // the duration of the one shot record
      lights[LEFT_LIGHT+1].setBrightness(flash_led(0.20f));

      // turn off the green light
      lights[LEFT_LIGHT].setBrightness(0);

      // ideally this should flash the led red for the full duration of the loop
      // but we don't have that measurement right now (TODO)
      // for now, just stay in this state for 3s
      if (should_transition_to_state(3.0f, one_shot_grace_period)) {
        // go straight to a 'playing' state after the one shot recording is done
        bypass_state = 2;

        // force disable one shot mode in case it is still on
        reset_one_shot(true);
      }
    } else if (bypass_state == 6) {
      // a loop select change is in progress, flash both leds green
      // for 4 seconds and then transition to the stopped state

      // turn off the red lights
      lights[LEFT_LIGHT+1].setBrightness(0);
      lights[RIGHT_LIGHT+1].setBrightness(0);

      // flash both lights green
      lights[LEFT_LIGHT].setBrightness(flash_led(0.30f));
      lights[RIGHT_LIGHT].setBrightness(flash_led(0.30f));

      // transition to the stopped state after 4.0 seconds
      if (should_transition_to_state(4.0f, loop_select_grace_period)) {
        bypass_state = 3;
        // force disable one shot mode in case it is still on
        reset_one_shot(true);
      }
    }

    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the CV inputs.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;

    // TODO
    // the length of the first recording, i.e. the time between pressing record and play
    // dictates how often the modifier lights flash.

    // read the gate triggers
    bool stop_triggered = false;
    if (inputs[STOP_GATE_INPUT].isConnected()) {
      if (stop_gate_trigger.process(rescale(inputs[STOP_GATE_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
        // if the trigger goes high, turn on on stop loop param
        INFO("PLAY GATE TRIGGERED");
        params[STOP_LOOP_PARAM].setValue(1.f);
        stop_triggered = true;
      }
    }
    bool play_triggered = false;
    if (inputs[PLAY_GATE_INPUT].isConnected()) {
      if (play_gate_trigger.process(rescale(inputs[PLAY_GATE_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
        // if the trigger goes high, turn on on play loop param
        INFO("PLAY GATE TRIGGERED");
        params[PLAY_LOOP_PARAM].setValue(1.f);
        play_triggered = true;
      }
    }
    bool rec_triggered = false;
    if (inputs[RECORD_GATE_INPUT].isConnected()) {
      if (record_gate_trigger.process(rescale(inputs[RECORD_GATE_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f))) {
        // if the trigger goes high, turn on on rec loop param
        INFO("RECORD GATE TRIGGERED");
        params[RECORD_LOOP_PARAM].setValue(1.f);
        rec_triggered = true;
      }
    }

    // read the bypass button values
    int record_loop = (int) floor(params[RECORD_LOOP_PARAM].getValue());
    int play_loop = (int) floor(params[PLAY_LOOP_PARAM].getValue());
    int stop_loop = (int) floor(params[STOP_LOOP_PARAM].getValue());
    int erase_loop = (int) floor(params[ERASE_LOOP_PARAM].getValue());
    int one_shot = (int) floor(params[TOGGLE_ONE_SHOT_RECORD_PARAM].getValue());

    // turn off the triggers if they are on
    if (stop_triggered)
      params[STOP_LOOP_PARAM].setValue(0);
    if (play_triggered)
      params[PLAY_LOOP_PARAM].setValue(0);
    if (rec_triggered)
      params[RECORD_LOOP_PARAM].setValue(0);

    // first things first, disable one_shot record if it was not turned on
    // don't reset the cache
    if (one_shot == 0)
      reset_one_shot(false);

    // State transitions:
    // 1) first press of record or play makes pedal record
    //    and left led lights changes from off to red
    // 2) 2nd press of record or play makes pedal play
    //    and left led lights changes from red to green
    // 3) pressing stop during play states, makes loop stop
    //    and left led changes from green to flashing green (200ms flashes)
    // 4) pressing record while in play state will allow for overdubs
    //    and left led lights changes from green to red
    // 5) holding record for a duration of time will
    //    enable an overdub that lasts as long as the original loop

    if (bypass_state == 0) {
       // pedal is stopped (or the state is unknown)
      if (record_loop) {
        if (one_shot) {
          // requested to do a one shot record
          bypass_state = 5;
          one_shot_record();
        } else {
          // requested to turn on record
          bypass_state = 1;
          record();
        }
      } else if (play_loop) {
        bypass_state = 2;
        play();
      } else if (stop_loop) {
        bypass_state = 3;
        stop();
      } else if (erase_loop) {
        // requested to erase the loop
        bypass_state = 4;
        erase();
      }
    } else if (bypass_state == 1) {
      // pedal is recording
      if (play_loop) {
        // requested to play the loop
        bypass_state = 2;
        play();
      } else if (stop_loop) {
        // requested to stop the loop
        bypass_state = 3;
        stop();
      } else if (erase_loop) {
        // requested to erase the loop
        bypass_state = 4;
        erase();
      }
    } else if (bypass_state == 2) {
      // pedal is playing
      if (record_loop) {
        if (one_shot) {
          // requested to do a one shot record
          bypass_state = 5;
          one_shot_record();
        } else {
          // requested to overdub something
          bypass_state = 1;
          over_dub();
        }
      } else if (stop_loop) {
        // requested to stop the loop
        bypass_state = 3;
        stop();
      } else if (erase_loop) {
        // requested to erase the loop
        bypass_state = 4;
        erase();
      }
    } else if (bypass_state == 3) {
      // pedal is stopped
      if (play_loop) {
        // requested to play the loop
        bypass_state = 2;
        play();
      } else if (stop_loop) {
        // requested to stop the loop
        bypass_state = 3;
        stop();
      } else if (erase_loop) {
        // requested to erase the loop
        bypass_state = 4;
        erase();
      }
    } else if (bypass_state == 4) {
      // transient state, loop is being erased
      // ignore all state change requests until we transition to
      // the off state.
    } else if (bypass_state == 5) {
      // transient state, currently processing a one-shot record
      // ignore all state changes except for stop and erase state changes
      if (stop_loop) {
        // requested to stop the loop or abort the one shot record
        bypass_state = 3;
        stop();
      } else if (erase_loop) {
        bypass_state = 4;
        erase();
      }
    } else if (bypass_state == 6) {
      // transiet state, a loop change is in progress
      // ignore all state change requests until we complete the transition
      // to the stopped state.
    }

    // knob values
    int volume = (int) std::round(params[VOLUME_PARAM].getValue());
    int layers = (int) std::round(params[LAYERS_PARAM].getValue());
    int repeats = (int) std::round(params[REPEATS_PARAM].getValue());
    int moda = (int) std::round(params[MODA_PARAM].getValue());
    int stability = (int) std::round(params[STABILITY_PARAM].getValue());
    int modb = (int) std::round(params[MODB_PARAM].getValue());
    int ramp = (int) std::round(params[RAMP_PARAM].getValue());
    int expr = -1;

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[VOLUME_INPUT].isConnected()) {
      int volume_cv = (int) std::round(inputs[VOLUME_INPUT].getVoltage()*2) / 10.f * 127;
      volume = clamp(volume_cv, 0, volume);
    }
    if (inputs[LAYERS_INPUT].isConnected()) {
      int layers_cv = (int) std::round(inputs[LAYERS_INPUT].getVoltage()*2) / 10.f * 127;
      layers = clamp(layers_cv, 0, layers);
    }
    if (inputs[REPEATS_INPUT].isConnected()) {
      int repeats_cv = (int) std::round(inputs[REPEATS_INPUT].getVoltage()*2) / 10.f * 127;
      repeats = clamp(repeats_cv, 0, repeats);
    }
    if (inputs[MODA_INPUT].isConnected()) {
      int moda_cv = (int) std::round(inputs[MODA_INPUT].getVoltage()*2) / 10.f * 127;
      moda = clamp(moda_cv, 0, moda);
    }
    if (inputs[STABILITY_INPUT].isConnected()) {
      int stability_cv = (int) std::round(inputs[STABILITY_INPUT].getVoltage()*2) / 10.f * 127;
      stability = clamp(stability_cv, 0, stability);
    }
    if (inputs[MODB_INPUT].isConnected()) {
      int modb_cv = (int) std::round(inputs[MODB_INPUT].getVoltage()*2) / 10.f * 127;
      modb = clamp(modb_cv, 0, modb);
    }
    if (inputs[RAMP_INPUT].isConnected()) {
      int ramp_cv = (int) std::round(inputs[RAMP_INPUT].getVoltage()*2) / 10.f * 127;
      ramp = clamp(ramp_cv, 0, 127);
    }
    if (inputs[EXPR_INPUT].isConnected()) {
      int expr_cv = (int) std::round(inputs[EXPR_INPUT].getVoltage()*2) / 10.f * 127;
      expr = clamp(expr_cv, 0, 127);
    }

    // assign values from knobs (or cv)
    midi_out.setValue(volume, 14);
    midi_out.setValue(layers, 15);
    midi_out.setValue(repeats, 16);
    midi_out.setValue(moda, 17);
    midi_out.setValue(stability, 18);
    midi_out.setValue(modb, 19);

    // assign value for expression
    if (expr > 0)
      midi_out.setValue(expr, 100);

    // assign value for ramping only if ramping is turned on
    int enable_ramp = (int) floor(params[TOGGLE_RAMP_PARAM].getValue());
    if (enable_ramp) {
      // turn on ramping
      midi_out.setValue(1, 52);

      // set the current ramp value
      midi_out.setValue(ramp, 20);
    } else {
      // turn off ramping
      midi_out.setValue(0, 52);
    }

    return;
  }
};

struct BlooperWidget : ModuleWidget {
  BlooperWidget(Blooper* module) {
    setModule(module);

#ifdef USE_LOGOS
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/blooper_panel_ext_logo.svg")));
#else
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/blooper_panel_ext.svg")));
#endif

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnobBlooper>(mm2px(Vec(10, 12)), module, Blooper::VOLUME_PARAM));
    addParam(createParamCentered<CBAKnobBlooper>(mm2px(Vec(30, 12)), module, Blooper::LAYERS_PARAM));
    addParam(createParamCentered<CBAKnobBlooper>(mm2px(Vec(50, 12)), module, Blooper::REPEATS_PARAM));
    addParam(createParamCentered<CBAKnobBlooper>(mm2px(Vec(10, 40)), module, Blooper::MODA_PARAM));
    addParam(createParamCentered<CBAKnobBlooper>(mm2px(Vec(30, 40)), module, Blooper::STABILITY_PARAM));
    addParam(createParamCentered<CBAKnobBlooper>(mm2px(Vec(50, 40)), module, Blooper::MODB_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 25)), module, Blooper::VOLUME_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 25)), module, Blooper::LAYERS_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 25)), module, Blooper::REPEATS_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 53)), module, Blooper::MODA_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 53)), module, Blooper::STABILITY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 53)), module, Blooper::MODB_INPUT));

    // expression port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(43.5, 92)), module, Blooper::EXPR_INPUT));

    // clock port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55, 92)), module, Blooper::CLOCK_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(7, 66)), module, Blooper::L_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(27, 66)), module, Blooper::M_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(46.5, 66)), module, Blooper::R_TOGGLE_PARAM));

    // mod a and mod b enable/disable toggles
    addParam(createParamCentered<CBASmallArcadeButtonOffBlueMomentary>(mm2px(Vec(43.5, 82)), module, Blooper::TOGGLE_MODA_PARAM));
    addParam(createParamCentered<CBASmallArcadeButtonOffBlueMomentary>(mm2px(Vec(55, 82)), module, Blooper::TOGGLE_MODB_PARAM));

    // lights
    addChild(createLightCentered<LargeLight<GreenRedLight>>(mm2px(Vec(24, 109)), module, Blooper::LEFT_LIGHT));
    addChild(createLightCentered<LargeLight<GreenRedLight>>(mm2px(Vec(37, 109)), module, Blooper::RIGHT_LIGHT));

    // toggle one shot record on/off
    addParam(createParamCentered<CBASwitchTwoWay>(mm2px(Vec(5, 109)), module, Blooper::TOGGLE_ONE_SHOT_RECORD_PARAM));

    // foot switches
    addParam(createParamCentered<CBAButtonGrayMomentary>(mm2px(Vec(11, 118)), module, Blooper::RECORD_LOOP_PARAM));
    addParam(createParamCentered<CBAButtonGrayMomentary>(mm2px(Vec(24, 118)), module, Blooper::PLAY_LOOP_PARAM));
    addParam(createParamCentered<CBAButtonGrayMomentary>(mm2px(Vec(37, 118)), module, Blooper::STOP_LOOP_PARAM));
    addParam(createParamCentered<CBAButtonGrayMomentary>(mm2px(Vec(50, 118)), module, Blooper::ERASE_LOOP_PARAM));

    // midi configuration display
    RRMidiWidget* midiWidget = createWidget<RRMidiWidget>(mm2px(Vec(3, 75)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);

    // extension section

    // CV gates
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(72, 17.5)), module, Blooper::STOP_GATE_INPUT));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(72, 31.5)), module, Blooper::PLAY_GATE_INPUT));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(72, 44.5)), module, Blooper::RECORD_GATE_INPUT));

    // loop selection (increment/decrement)
    addParam(createParamCentered<PlusButtonMomentary>(mm2px(Vec(76, 60)), module, Blooper::LOOP_SELECT_INCR_PARAM));
    addParam(createParamCentered<MinusButtonMomentary>(mm2px(Vec(68, 60)), module, Blooper::LOOP_SELECT_DECR_PARAM));

    // toggle ramp mod on/off
    addParam(createParamCentered<CBASwitchTwoWay>(mm2px(Vec(67, 77)), module, Blooper::TOGGLE_RAMP_PARAM));

    // ramp cv input port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(75, 81)), module, Blooper::RAMP_INPUT));

    // ramp tiny knob
    addParam(createParamCentered<CBAKnobTinyBlooper>(mm2px(Vec(75, 73)), module, Blooper::RAMP_PARAM));

  }
};

Model* modelBlooper = createModel<Blooper, BlooperWidget>("blooper");
