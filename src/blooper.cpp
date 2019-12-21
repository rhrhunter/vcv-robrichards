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
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  {
                  ENUMS(LEFT_LIGHT, 2),
                  RIGHT_LIGHT,
                  NUM_LIGHTS
  };

  // internal clock measurements
  struct timeval last_blink_time;
  double blink_off_until_usec = 0;
  double blink_off_until_sec = 0;  
  int bypass_state;
  
  Blooper() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(VOLUME_PARAM, 0.f, 127.f, 64.f, "Loop Volume");
    configParam(LAYERS_PARAM, 0.f, 127.f, 127.f, "Layers");
    configParam(REPEATS_PARAM, 0.f, 127.f, 127.f, "Repeats");
    configParam(MODA_PARAM, 0.f, 127.f, 64.f, "Mod A");
    configParam(STABILITY_PARAM, 0.f, 127.f, 0.f, "Stability");
    configParam(MODB_PARAM, 0.f, 127.f, 64.f, "Mod B");

    // 3 way switches
    // 1.0f is top position
    configParam(L_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Modifier A (Smooth Speed, Dropper, Trimmer)");
    configParam(M_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Loop Program (Normal, Additive, Sampler)");
    configParam(R_TOGGLE_PARAM, 1.0f, 3.0f, 2.0f, "Modifier B (Stepped Speed, Scrambler, Filter)");

    // front buttons
    configParam(TOGGLE_MODA_PARAM, 0.0f, 1.0f, 0.0f, "Modifier A (enable/disable)");
    configParam(TOGGLE_MODB_PARAM, 0.0f, 1.0f, 0.0f, "Modifier B (enable/disable)");    
    
    // buttons
    configParam(RECORD_LOOP_PARAM, 0.f, 1.f, 0.f, "Record Loop");
    configParam(PLAY_LOOP_PARAM, 0.f, 1.f, 0.f, "Play Loop");   
    configParam(STOP_LOOP_PARAM, 0.f, 1.f, 0.f, "Stop Loop");
    configParam(ERASE_LOOP_PARAM, 0.f, 1.f, 0.f, "Erase Loop");    

    // led states
    gettimeofday(&last_blink_time, NULL);    
    bypass_state = 0;
  }

  void record() {
    midi_out.setValue(1, 11);
  }

  void play() {
    midi_out.setValue(2, 11);
  }  
  
  void over_dub() {
    midi_out.setValue(3, 11);    
  }
  
  void stop() {
    midi_out.setValue(4, 11);
  }
  
  void erase() {
    midi_out.setValue(7, 11);
  }  
  
  float flash_loop_led(float blink_rate) {
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
  
  void process(const ProcessArgs& args) override {
    // only proceed if midi is activated
    if (!midi_out.active()) {
      if (!disable_module()) {
        // turn off the lights if the module is not disabled
        lights[LEFT_LIGHT].setBrightness(0.f);
        lights[RIGHT_LIGHT].setBrightness(0.f);
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
    int modb_toggle = (int) floor(params[TOGGLE_MODB_PARAM].getValue());
    if (moda_toggle)
      midi_out.setValue(1, 30);
    else
      midi_out.setValue(127,30);
    if (modb_toggle)
      midi_out.setValue(1, 31);
    else
      midi_out.setValue(127,31);

    // turn on lights
    if (bypass_state == 1) {
      // recording so light will be red
      lights[LEFT_LIGHT + 1].setBrightness(1.f);
      lights[LEFT_LIGHT].setBrightness(0.f);
    } else if (bypass_state == 2) {
      // recording is playing so light will be green
      lights[LEFT_LIGHT + 1].setBrightness(0.f);
      lights[LEFT_LIGHT].setBrightness(1.f);            
    } else if (bypass_state == 3) {
      // recording is stopped, so flash green
      lights[LEFT_LIGHT].setBrightness(flash_loop_led(0.50f));      
      lights[LEFT_LIGHT + 1].setBrightness(0.f);
    } else {
      lights[LEFT_LIGHT + 0].setBrightness(0.f);      
      lights[LEFT_LIGHT + 1].setBrightness(0.f);      
    }
               
    // apply rate limiting here so that we do not flood the
    // system with midi messages caused by the CV inputs.
    if (should_rate_limit(0.005f, args.sampleTime))
      return;
    
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
    
    // TODO
    // the length of the first recording dictates when the modifier
    // lights flash
    
    // read the bypass button values
    int record_loop = (int) floor(params[RECORD_LOOP_PARAM].getValue());
    int play_loop = (int) floor(params[PLAY_LOOP_PARAM].getValue());    
    int stop_loop = (int) floor(params[STOP_LOOP_PARAM].getValue());
    int erase_loop = (int) floor(params[ERASE_LOOP_PARAM].getValue());    
    if (bypass_state == 0) {
       // pedal is stopped (or the state is unknown)
      if (record_loop) {
        // requested to turn on record
        bypass_state = 1;
        // send a record message
        record();
      } else if (play_loop) {
        bypass_state = 2;
        play();
      } else if (stop_loop) {
        bypass_state = 3;
        stop();
      } else if (erase_loop) {
        bypass_state = 0;
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
        bypass_state = 0;
        erase();
      }      
    } else if (bypass_state == 2) {
      // pedal is playing
      if (record_loop) {
        // requested to overdub something
        bypass_state = 1;
        over_dub();
      } else if (stop_loop) {
        // requested to stop the loop
        bypass_state = 3;
        stop();
      } else if (erase_loop) {
        bypass_state = 0;
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
        bypass_state = 0;
        erase();
      }            
    }

    // knob values
    int volume = (int) std::round(params[VOLUME_PARAM].getValue());
    int layers = (int) std::round(params[LAYERS_PARAM].getValue());
    int repeats = (int) std::round(params[REPEATS_PARAM].getValue());
    int moda = (int) std::round(params[MODA_PARAM].getValue());
    int stability = (int) std::round(params[STABILITY_PARAM].getValue());
    int modb = (int) std::round(params[MODB_PARAM].getValue());
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

    return;
  }
};

struct BlooperWidget : ModuleWidget {
  BlooperWidget(Blooper* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/blooper_text.svg")));

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
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 66)), module, Blooper::L_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(30, 66)), module, Blooper::M_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(50, 66)), module, Blooper::R_TOGGLE_PARAM));

    // mod a and mod b enable/disable 
    addParam(createParamCentered<CBASwitchTwoWay>(mm2px(Vec(43.5, 82)), module, Blooper::TOGGLE_MODA_PARAM));
    addParam(createParamCentered<CBASwitchTwoWay>(mm2px(Vec(55, 82)), module, Blooper::TOGGLE_MODB_PARAM));

    // foot switches 
    addChild(createLightCentered<LargeLight<GreenRedLight>>(mm2px(Vec(15, 109)), module, Blooper::LEFT_LIGHT));
    addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(46, 109)), module, Blooper::RIGHT_LIGHT));
    
    addParam(createParamCentered<CBAMomentaryButtonGray>(mm2px(Vec(11, 118)), module, Blooper::RECORD_LOOP_PARAM));
    addParam(createParamCentered<CBAMomentaryButtonGray>(mm2px(Vec(24, 118)), module, Blooper::PLAY_LOOP_PARAM));    
    addParam(createParamCentered<CBAMomentaryButtonGray>(mm2px(Vec(37, 118)), module, Blooper::STOP_LOOP_PARAM));
        addParam(createParamCentered<CBAMomentaryButtonGray>(mm2px(Vec(50, 118)), module, Blooper::ERASE_LOOP_PARAM));

    // midi configuration display
    RRMidiWidget* midiWidget = createWidget<RRMidiWidget>(mm2px(Vec(3, 75)));
    midiWidget->box.size = mm2px(Vec(33.840, 28));
    midiWidget->setMidiPort(module ? &module->midi_out : NULL);
    addChild(midiWidget);
  }
};

Model* modelBlooper = createModel<Blooper, BlooperWidget>("blooper");
