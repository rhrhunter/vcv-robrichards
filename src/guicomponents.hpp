#pragma once
#include "componentlibrary.hpp"
#include "random.hpp"
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
  
}
