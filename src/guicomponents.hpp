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

  struct CBAKnobBlooper : RoundKnob {
    CBAKnobBlooper() {
      setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/cba_knob_blooper.svg")));
    }
  };

  struct CBAKnobTinyBlooper : RoundKnob {
    CBAKnobTinyBlooper() {
      setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/cba_knob_tiny_blooper.svg")));
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

  struct CBAButtonRedMomentary : app::SvgSwitch {
    CBAButtonRedMomentary() {
      momentary = true;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_4.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_2.svg")));
    }
  };

  struct PlusButtonMomentary : app::SvgSwitch {
    PlusButtonMomentary() {
      momentary = true;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/plus_off.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/plus_on.svg")));
    }
  };

  struct MinusButtonMomentary : app::SvgSwitch {
    MinusButtonMomentary() {
      momentary = true;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/minus_off.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/minus_on.svg")));
    }
  };

  struct CBAButtonGrayMomentary : app::SvgSwitch {
    CBAButtonGrayMomentary() {
      momentary = true;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_4.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_1.svg")));
    }
  };

  struct CBAArcadeButtonOffBlueRed : app::SvgSwitch {
    CBAArcadeButtonOffBlueRed() {
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_arcade_off.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_arcade_blue.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_arcade_red.svg")));
   }
  };

  struct CBAArcadeButtonOffBlue : app::SvgSwitch {
    CBAArcadeButtonOffBlue() {
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_arcade_off.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_arcade_blue.svg")));
   }
  };

  struct CBASmallArcadeButtonOffBlueMomentary : app::SvgSwitch {
    CBASmallArcadeButtonOffBlueMomentary() {
      momentary = true;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_arcade_off_small.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_arcade_blue_small.svg")));
   }
  };

  struct CBAButtonGray : app::SvgSwitch {
    CBAButtonGray() {
      momentary = false;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_4.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/cba_button_1.svg")));
    }
  };

  struct AutomatoneSlider : SvgSlider {
    AutomatoneSlider() {
      Vec margin = Vec(1, 1);
      maxHandlePos = Vec(0.0, -13.0).plus(margin);
      minHandlePos = Vec(0.0, 130.0).plus(margin);
      setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/automatone_slider_background.svg")));
      setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/automatone_slider_handle.svg")));

      background->box.pos = margin;

      box.size = background->box.size.plus(margin.mult(2));
    }
  };

}
