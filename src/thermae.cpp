#include "plugin.hpp"

#include <math.h>
#include "guicomponents.hpp"
#include "midi_classes.hpp"

#include <sys/time.h>

struct Thermae : Module {
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
                 MIDI_CHANNEL_PARAM,
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
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  { NUM_LIGHTS };

  RRMidiOutput midi_out;
  bool can_tap_tempo;
  struct timeval last_tap_tempo_time;

  Thermae() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // main knob parameters
    configParam(MIX_PARAM, 0.f, 127.f, 0.f, "Mix");
    configParam(LPF_PARAM, 0.f, 127.f, 64.f, "Lpf");
    configParam(REGEN_PARAM, 0.f, 127.f, 0.f, "Regen");
    configParam(GLIDE_PARAM, 0.f, 127.f, 0.f, "GLIDE");
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

    // midi configuration knobs
    configParam(MIDI_CHANNEL_PARAM, 1.f, 16.f, 4.f, "MIDI Channel");

    // bypass button
    configParam(BYPASS_PARAM, 0.f, 1.f, 0.f, "Pedal Bypass");

    // tap tempo buttons
    configParam(TAP_TEMPO_PARAM, 0.f, 1.f, 0.f, "Tap Tempo");

    // DeviceIds start counting from 0, not 1
    midi_out.setDeviceId(3);
    midi_out.setChannel(4);

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

    // knob values
    int mix = (int) floor(params[MIX_PARAM].getValue() + 0.5);
    int lpf = (int) floor(params[LPF_PARAM].getValue() + 0.5);
    int regen = (int) floor(params[REGEN_PARAM].getValue() + 0.5);
    int glide = (int) floor(params[GLIDE_PARAM].getValue() + 0.5);
    int int1 = (int) floor(params[INT1_PARAM].getValue() + 0.5);
    int int2 = (int) floor(params[INT2_PARAM].getValue() + 0.5);

    // 3way switch values (1,2,3)
    int l_toggle = (int) floor(params[L_TOGGLE_PARAM].getValue());
    int m_toggle = (int) floor(params[M_TOGGLE_PARAM].getValue());
    int r_toggle = (int) floor(params[R_TOGGLE_PARAM].getValue());

    // 2way switch values (0,127)
    int slowdown_mode = (int) floor(params[SLOWDOWN_MODE_PARAM].getValue());
    int hold_mode = (int) floor(params[HOLD_MODE_PARAM].getValue());
    if (slowdown_mode > 0)
      slowdown_mode = 127;
    if (hold_mode > 0)
      hold_mode = 127;

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = (int) floor((inputs[MIX_INPUT].getVoltage() * 16.9f));
      if (mix_cv > mix)
        mix_cv = mix;
      mix = mix_cv;
    }

    if (inputs[LPF_INPUT].isConnected()) {
      int lpf_cv = (int) floor((inputs[LPF_INPUT].getVoltage() * 16.9f));
      if (lpf_cv > lpf)
        lpf_cv = lpf;
      lpf = lpf_cv;
    }

    if (inputs[REGEN_INPUT].isConnected()) {
      int regen_cv = (int) floor((inputs[REGEN_INPUT].getVoltage() * 16.9f));
      if (regen_cv > regen)
        regen_cv = regen;
      regen = regen_cv;
    }

    if (inputs[GLIDE_INPUT].isConnected()) {
      int glide_cv = (int) floor((inputs[GLIDE_INPUT].getVoltage() * 16.9f));
      if (glide_cv > glide)
        glide_cv = glide;
      glide = glide_cv;
    }

    if (inputs[INT1_INPUT].isConnected()) {
      int int1_cv = (int) floor((inputs[INT1_INPUT].getVoltage() * 16.9f));
      if (int1_cv > int1)
        int1_cv = int1;
      int1 = int1_cv;
    }

    if (inputs[INT2_INPUT].isConnected()) {
      int int2_cv = (int) floor((inputs[INT2_INPUT].getVoltage() * 16.9f));
      if (int2_cv > int2)
        int2_cv = int2;
      int2 = int2_cv;
    }

    // bypass or enable the pedal
    int enable_pedal = (int) floor(params[BYPASS_PARAM].getValue());
    int bypass;
    if (enable_pedal)
      bypass = 127;
    else
      bypass = 0;

    // assign values from knobs (or cv)
    midi_out.setValue(mix, 14);
    midi_out.setValue(lpf, 15);
    midi_out.setValue(regen, 16);
    midi_out.setValue(glide, 17);
    midi_out.setValue(int1, 18);
    midi_out.setValue(int2, 19);

    // assign values from switches
    midi_out.setValue(l_toggle, 21);
    midi_out.setValue(m_toggle, 22);
    midi_out.setValue(r_toggle, 23);

    // enable/disable hold mode and/or slowdown mode
    midi_out.setValue(hold_mode, 24);
    midi_out.setValue(slowdown_mode, 25);

    // enable or bypass the pedal
    midi_out.setValue(bypass, 102);
  }
};

struct ThermaeWidget : ModuleWidget {
  ThermaeWidget(Thermae* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/thermae_blank.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 15)), module, Thermae::MIX_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 15)), module, Thermae::LPF_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 15)), module, Thermae::REGEN_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 50)), module, Thermae::GLIDE_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(30, 50)), module, Thermae::INT1_PARAM));
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(50, 50)), module, Thermae::INT2_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 30)), module, Thermae::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 30)), module, Thermae::LPF_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 30)), module, Thermae::REGEN_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 65)), module, Thermae::GLIDE_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 65)), module, Thermae::INT1_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 65)), module, Thermae::INT2_INPUT));

    // clock port
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(50, 100)), module, Thermae::CLOCK_INPUT));

    // program switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 80)), module, Thermae::L_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(30, 80)), module, Thermae::M_TOGGLE_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(50, 80)), module, Thermae::R_TOGGLE_PARAM));

    // slowdown mode toggle and hold mode toggle
    addParam(createParamCentered<CBASwitchTwoWay>(mm2px(Vec(46, 90)), module, Thermae::SLOWDOWN_MODE_PARAM));
    addParam(createParamCentered<CBASwitchTwoWay>(mm2px(Vec(54, 90)), module, Thermae::HOLD_MODE_PARAM));

    // bypass switches & tap tempo
    addParam(createParamCentered<CBAMomentaryButtonRed>(mm2px(Vec(15, 118)), module, Thermae::TAP_TEMPO_PARAM));
    addParam(createParamCentered<CBAButtonRed>(mm2px(Vec(46, 118)), module, Thermae::BYPASS_PARAM));

    // midi configuration displays
    addParam(createParamCentered<CBAKnob>(mm2px(Vec(10, 100)), module, Thermae::MIDI_CHANNEL_PARAM));
    MidiChannelDisplay *mcd = new MidiChannelDisplay();
    mcd->box.pos = Vec(50, 285);
    mcd->box.size = Vec(50, 20);
    mcd->value = &((module->midi_out).midi_channel);
    mcd->module = (void *) module;
    addChild(mcd);

  }
};


Model* modelThermae = createModel<Thermae, ThermaeWidget>("thermae");
