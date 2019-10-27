#include "plugin.hpp"

#include <midi.hpp>
#include <math.h>
#include "guicomponents.hpp"

struct DWCCMidiOutput : midi::Output {
  int lastValues[128];

  DWCCMidiOutput() {
    reset();
  }

  void reset() {
    for (int n = 0; n < 128; n++) {
      lastValues[n] = -1;
    }
  }

  void setValue(int value, int cc) {
    if (value == lastValues[cc]) {
      return;
    }
    lastValues[cc] = value;
    INFO("Setting cc:%d to value:%d", cc, value);

    // CC
    midi::Message m;
    m.setStatus(0xb);
    m.setNote(cc);
    m.setValue(value);
    sendMessage(m);
  }
};

struct Darkworld : Module {
  enum ParamIds {
                 DECAY_PARAM,
                 MIX_PARAM,
                 DWELL_PARAM,
                 MODIFY_PARAM,
                 TONE_PARAM,
                 PRE_DELAY_PARAM,
                 DARK_PROGRAM_PARAM,
                 ROUTING_PARAM,
                 WORLD_PROGRAM_PARAM,
                 NUM_PARAMS
  };
  enum InputIds  {
                  DECAY_INPUT,
                  MIX_INPUT,                  
                  DWELL_INPUT,
                  MODIFY_INPUT,
                  TONE_INPUT,
                  PRE_DELAY_INPUT,                                                      
                  NUM_INPUTS
  };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds  { NUM_LIGHTS };

  DWCCMidiOutput midi_out;

  Darkworld() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // knob parameters
    configParam(DECAY_PARAM, 0.f, 127.f, 0.f, "");
    configParam(MIX_PARAM, 0.f, 127.f, 0.f, "");
    configParam(DWELL_PARAM, 0.f, 127.f, 0.f, "");
    configParam(MODIFY_PARAM, 0.f, 127.f, 0.f, "");
    configParam(TONE_PARAM, 0.f, 127.f, 0.f, "");
    configParam(PRE_DELAY_PARAM, 0.f, 127.f, 0.f, "");
    
    // three way switches
    configParam(DARK_PROGRAM_PARAM, 0.0f, 2.0f, 2.0f, "Dark Program");   // 0.0f is top position
    configParam(ROUTING_PARAM, 0.0f, 2.0f, 2.0f, "Routing Mode");        // 0.0f is top position
    configParam(WORLD_PROGRAM_PARAM, 0.0f, 2.0f, 2.0f, "World Program"); // 0.0f is top position

    // DeviceIDs start counting from 0, not 1
    midi_out.setDeviceId(3);

    // Channels start counting from 0, not 1
    midi_out.setChannel(1);

    // initialize all the params
    init_params();
  }

  void process(const ProcessArgs& args) override {
    // knob values
    int decay = (int) floor(params[DECAY_PARAM].getValue() + 0.5);
    int mix = (int) floor(params[MIX_PARAM].getValue() + 0.5);
    int dwell = (int) floor(params[DWELL_PARAM].getValue() + 0.5);
    int modify = (int) floor(params[MODIFY_PARAM].getValue() + 0.5);
    int tone = (int) floor(params[TONE_PARAM].getValue() + 0.5);
    int pre_delay = (int) floor(params[PRE_DELAY_PARAM].getValue() + 0.5);

    // switch values
    int dark_prog = (int) floor(params[DARK_PROGRAM_PARAM].getValue() + 1);
    int route_prog = (int) floor(params[ROUTING_PARAM].getValue() + 1);
    int world_prog = (int) floor(params[WORLD_PROGRAM_PARAM].getValue() + 1);

    // read cv voltages and override values of knobs, use the knob value as a ceiling
    if (inputs[DECAY_INPUT].isConnected()) {
      int decay_cv = (int) floor((inputs[DECAY_INPUT].getVoltage() * 16.9f));
      INFO("DECAY CV: %d, DECAY: %d", decay_cv, decay);
      if (decay_cv > decay)
        decay_cv = decay;
      decay = decay_cv;
    }

    if (inputs[MIX_INPUT].isConnected()) {
      int mix_cv = (int) floor((inputs[MIX_INPUT].getVoltage() * 16.9f));
      if (mix_cv > mix)
        mix_cv = mix;
      mix = mix_cv; 
    }    

    if (inputs[DWELL_INPUT].isConnected()) {
      int dwell_cv = (int) floor((inputs[DWELL_INPUT].getVoltage() * 16.9f));
      if (dwell_cv > dwell)
        dwell_cv = dwell;
      dwell = dwell_cv; 
    }

    if (inputs[TONE_INPUT].isConnected()) {
      int tone_cv = (int) floor((inputs[TONE_INPUT].getVoltage() * 16.9f));
      if (tone_cv > tone)
        tone_cv = tone;
      tone = tone_cv; 
    }

    if (inputs[MODIFY_INPUT].isConnected()) {
      int modify_cv = (int) floor((inputs[MODIFY_INPUT].getVoltage() * 16.9f));
      if (modify_cv > modify)
        modify_cv = modify;
      modify = modify_cv; 
    }
    
    if (inputs[PRE_DELAY_INPUT].isConnected()) {
      int pre_delay_cv = (int) floor((inputs[PRE_DELAY_INPUT].getVoltage() * 16.9f));
      if (pre_delay_cv > pre_delay)
        pre_delay_cv = pre_delay;
      pre_delay = pre_delay_cv; 
    }        
    
    // assign values from knobs (or cv)
    midi_out.setValue(decay, 14);
    midi_out.setValue(mix, 15);
    midi_out.setValue(dwell, 16);
    midi_out.setValue(modify, 17);
    midi_out.setValue(tone, 18);
    midi_out.setValue(pre_delay, 19);

    // assign values from switches
    midi_out.setValue(dark_prog, 21);
    midi_out.setValue(route_prog, 22);
    midi_out.setValue(world_prog, 23);
    
    // std::vector<int> ids = midi_out.getDeviceIds();
    // for(unsigned long i = 0; i < ids.size(); i++){
    //   std::string dev_name = midi_out.getDeviceName(i);
    //   INFO("id: %d, device: %s", i, dev_name.c_str());
    // }



    
  }

  void init_params() {
    midi_out.setValue(0, 14);
    midi_out.setValue(0, 15);
    midi_out.setValue(0, 16);
    midi_out.setValue(0, 17);
    midi_out.setValue(0, 18);
    midi_out.setValue(0, 19);

    midi_out.setValue(3, 21);
    midi_out.setValue(3, 22);
    midi_out.setValue(3, 23);            
  }
};


struct DarkworldWidget : ModuleWidget {
  DarkworldWidget(Darkworld* module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/darkworld.svg")));

    // screws
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // knobs
    addParam(createParamCentered<DWKnob>(mm2px(Vec(10, 15)), module, Darkworld::DECAY_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(30, 15)), module, Darkworld::MIX_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(50, 15)), module, Darkworld::DWELL_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(10, 50)), module, Darkworld::MODIFY_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(30, 50)), module, Darkworld::TONE_PARAM));
    addParam(createParamCentered<DWKnob>(mm2px(Vec(50, 50)), module, Darkworld::PRE_DELAY_PARAM));

    // ports
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 35)), module, Darkworld::DECAY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 35)), module, Darkworld::MIX_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 35)), module, Darkworld::DWELL_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(10, 70)), module, Darkworld::MODIFY_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(30, 70)), module, Darkworld::TONE_INPUT));
    addInput(createInputCentered<CL1362Port>(mm2px(Vec(50, 70)), module, Darkworld::PRE_DELAY_INPUT));            

    // switches
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(10, 85)), module, Darkworld::DARK_PROGRAM_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(30, 85)), module, Darkworld::ROUTING_PARAM));
    addParam(createParamCentered<CBASwitch>(mm2px(Vec(50, 85)), module, Darkworld::WORLD_PROGRAM_PARAM));

  }
};


Model* modelDarkworld = createModel<Darkworld, DarkworldWidget>("darkworld");
