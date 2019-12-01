#pragma once
#include "componentlibrary.hpp"

#include "widget/Widget.hpp"

using namespace std;

namespace rack {

  struct CBAKnob : RoundKnob {
    CBAKnob() {
      setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/cba_knob_1.svg")));
    }
  };

  struct CBAKnobDW : RoundKnob {
    CBAKnobDW() {
      setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/cba_knob_darkworld.svg")));
    }
  };

  struct CBAKnobThermae : RoundKnob {
    CBAKnobThermae() {
      setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/cba_knob_thermae.svg")));
    }
  };

  struct CBAKnobMood : RoundKnob {
    CBAKnobMood() {
      setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/cba_knob_mood.svg")));
    }
  };

  struct CBAKnobGL : RoundKnob {
    CBAKnobGL() {
      setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/cba_knob_genloss.svg")));
    }
  };

  struct CBAKnobWV : RoundKnob {
    CBAKnobWV() {
      setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/cba_knob_warpedvinyl.svg")));
    }
  };


  struct CBASwitch : app::SvgSwitch {
    CBASwitch() {
      addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_2.svg")));
      addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_1.svg")));
      addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_0.svg")));
    }
  };

  struct CBASwitchTwoWay : app::SvgSwitch {
    CBASwitchTwoWay() {
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_toggle_0.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_toggle_2.svg")));
    }
  };
  struct CBASwitchTwoWayMomentary : app::SvgSwitch {
    CBASwitchTwoWayMomentary() {
      momentary = true;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_toggle_0.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_toggle_2.svg")));
    }
  };

  struct CBAButtonRedGreen : app::SvgSwitch {
    CBAButtonRedGreen() {
      momentary = false;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_2.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_3.svg")));
    }
  };

  struct CBAButtonGreen : app::SvgSwitch {
    CBAButtonGreen() {
      momentary = false;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_4.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_3.svg")));
    }
  };

  struct CBAButtonRed : app::SvgSwitch {
    CBAButtonRed() {
      momentary = false;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_4.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_2.svg")));
    }
  };

  struct CBAButton : app::SvgSwitch {
    CBAButton() {
      momentary = false;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_4.svg")));
    }
  };

  struct CBAMomentaryButtonRed : app::SvgSwitch {
    CBAMomentaryButtonRed() {
      momentary = true;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_4.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_2.svg")));
    }
  };

  struct CBAMomentaryButtonGray : app::SvgSwitch {
    CBAMomentaryButtonGray() {
      momentary = true;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_4.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_1.svg")));
    }
  };

  struct CBAButtonGray : app::SvgSwitch {
    CBAButtonGray() {
      momentary = false;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_4.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_1.svg")));
    }
  };

}
