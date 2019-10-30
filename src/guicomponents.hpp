#pragma once
#include "componentlibrary.hpp"

#include "widget/Widget.hpp"

using namespace std;

namespace rack {

  struct DWKnob : RoundKnob {
    DWKnob() {
      setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/dwknob.svg")));
    }
  };

  struct CBASwitch : app::SvgSwitch {
    CBASwitch() {
      addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_2.svg")));
      addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_1.svg")));
      addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/CKSSThree_0.svg")));
    }
  };

  struct CBAButtonRedGreen : app::SvgSwitch {
    CBAButtonRedGreen() {
      momentary = false;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dwbutton_2.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dwbutton_3.svg")));
    }
  };

  struct CBAButton : app::SvgSwitch {
    CBAButton() {
      momentary = false;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dwbutton_1.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dwbutton_2.svg")));
    }
  };

  struct CBAMomentaryButton : app::SvgSwitch {
    CBAMomentaryButton() {
      momentary = true;
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dwbutton_1.svg")));
      addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dwbutton_2.svg")));
    }
  };

  struct MidiChannelDisplay : TransparentWidget {
    int *value;
    void *module;
    std::shared_ptr<Font> font;

    MidiChannelDisplay() {
      font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Segment7Standard.ttf"));
    };

    void draw(const DrawArgs &args) override {
      int st;
      if (module) {
        st = value != NULL ? *(value) : 2;
      } else {
        st = 2;
      }

      // Background
      NVGcolor backgroundColor = nvgRGB(0x44, 0x44, 0x44);
      NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
      nvgBeginPath(args.vg);
      nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
      nvgFillColor(args.vg, backgroundColor);
      nvgFill(args.vg);
      nvgStrokeWidth(args.vg, 1.0);
      nvgStrokeColor(args.vg, borderColor);
      nvgStroke(args.vg);

      nvgFontSize(args.vg, 18);
      nvgFontFaceId(args.vg, font->handle);
      nvgTextLetterSpacing(args.vg, 2.5);

      std::string to_display = std::to_string(st);

      while(to_display.length()<3) to_display = ' ' + to_display;

      Vec textPos = Vec(6.0f, 17.0f);

      NVGcolor textColor = nvgRGB(0xdf, 0xd2, 0x2c);
      nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
      nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);

      textColor = nvgRGB(0xda, 0xe9, 0x29);

      nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
      nvgText(args.vg, textPos.x, textPos.y, "\\\\\\", NULL);


      textColor = nvgRGB(0x28, 0xb0, 0xf3);
      nvgFillColor(args.vg, textColor);
      nvgText(args.vg, textPos.x, textPos.y, to_display.c_str(), NULL);
    }
  };

}
