# vcv-robrichards
[![CircleCI](https://circleci.com/gh/rhrhunter/vcv-robrichards/tree/master.svg?style=svg)](https://circleci.com/gh/rhrhunter/vcv-robrichards/tree/master)

VCV Plugins by RobRichards

![RobRichards Plugins](plugins.png)

Here you'll find unofficial MIDI controller plugins for various Chase Bliss Audio Pedals.

Current supported interfaces:

* **Thermae**
* **Dark World**
* **Warped Vinyl**
* **M O O D**
* **Generation Loss**
* **Blooper** *(Firmware (v3.0)*
* **Preamp MKII**
* **CXM 1978**

NOTE: In order to use these modules, you will need:

1. One of the physical Chase Bliss Audio Pedals listed above.
2. A MIDI-to-TRS converter (e.g. 'Chase Bliss Audio Midibox' or a 'Disaster Area Designs Midibox'). (**not needed for Preamp MKII**)
3. A means to connect the MIDI-to-TRS converter to your computer via a MIDI Interface (e.g. MOTU Express XT) or special Midi to USB cable.
4. A computer with VCV Rack installed.

Enjoy.

## Tested Configurations

I've verified that these modules work properly with:

* macOS Mojave with VCVRack 1.1.6 using a Disaster Area Designs Midibox and a MOTU Express XT MIDI Interface

Y.M.M.V. with Windows and Linux, but I'm fairly certain they should work as well.

## Installation Steps

Since these are not distributable on the VCV plug-in library (yet), you'll have to compile
from source and install locally into your Rack Installation.

To do this, first set up your development environment for your platform using this guide: https://vcvrack.com/manual/Building.html#setting-up-your-development-environment.

**Note: you do not need to compile Rack.**

You will then need to download the most recent version of the [Rack SDK](https://vcvrack.com/downloads/Rack-SDK-1.1.6.zip) and unzip it to some directory (e.g. ~/Downloads/Rack-SDK).

Finally, clone this git repo, change directories into it, and run the following to compile and install the plugin in your Rack installation.

* On macOS: `RACK_DIR=~/Downloads/Rack-SDK make install`

Then launch Rack.

## Features
* Dynamic modulation of all knobs using CV inputs, via CV -> CC
* 3-way programmable switches (All)
* Stomp switch controls (All)
* Expression CV Input (All)
* Bypass high and low gate triggers (All Channels -> **Dark World** & **M O O D**) (AUX -> **Generation Loss**)
* Clock SYNC inputs (**Thermae**, **Warped Vinyl**, & **Blooper**)
* Tap Tempo switch (**Thermae** & **Warped Vinyl**)
* Tap Tempo high gate trigger (**Thermae** & **Warped Vinyl**)
* "Slow-down" mode toggle (**Thermae**)
* "Self-oscillation/Hold" mode via momentary toggle (**Thermae**)
* Midi Note tempo divisions (**Warped Vinyl**)
* Modifier (A & B) Toggles (**Blooper**)
* Start/Stop/Record/Overdub/Modifier CV gate triggers (**Blooper**)
* Loop Selection (Up/Down) (**Blooper**)
* Ramping: Enable/Disable, CV, and knob control (**Blooper**)
* One-Shot Record On/Off toggle (**Blooper**)
* Remote control of motorized faders (**Preamp MKII** & **CXM 1978**)
* Preset cycling (**Preamp MK2** & **CXM 1978**)

## Notes

### Supported Voltage Range for CV Modulation of Knobs

These plugins support a voltage range of 0-5V. When connecting an LFO to the CV ports, you will want to adjust the offset and scale of the LFO to fall into this range to avoid plateauing your wave forms (min or max). For example, if using the Bogaudio LFO this means setting the `offset` to 2.5V and the `scale` to 50%.

### Protection Mechanisms

As a precaution, the high and low gate triggers for pedal bypass are not implemented on pedals that contain bypass relay switches. This is primarily to prevent wear & tear that could physically damage the pedal. Therefore, the MIDI controllers for **Thermae**, **Warped Vinyl**, and **Generation Loss** do not have this implemented.

### Known Issues (at least with my hardware)
1. **M O O D** - I suspect it does not respond correctly to Midi Message **CC15=127** (Mix knob fully-CW). Instead of having a completely wet mix, it appears to be identical to sending **CC15=126**, where a small bit of the dry signal is still audible.
2. **Preamp MKII** & **CXM 1978** - The 'Jump' arcade button does not respond to **CC22=1** nor **CC22=2**. It only appears to respond to **CC22=3**, which instead of sending the state of the arcade button to "5 (Blue Light)", it actually toggles *between* the three options. This is contrary to the specification in the Midi Implementation Manual.

### Caveats & Limitations

Chase Bliss Pedals do not respond well to the Running Status feature in the Midi protocol. To overcome this, every midi message that is sent to a CBA device is followed with dummy midi message to invalid the Running Status optimization feature. Although this means that double the amount of messages are being sent to the devices, it ensures that no messages are dropped by the devices.

# Disclaimer/License

The products and logos for *M O O D*, *Dark World*, *Thermae*, *Warped Vinyl Hifi*, *Generation Loss*, *Blooper*, and *Preamp MKII* are registered trademarks of *Chase Bliss Audio (www.chaseblissaudio.com)*. This software is free and distributed under the GNU GPLv3 license. It is not associated with the *Chase Bliss Audio* brand.
