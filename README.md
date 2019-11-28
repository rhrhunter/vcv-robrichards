# vcv-robrichards
VCV Plugins by RobRichards

![RobRichards Plugins](plugins.png)

Here you'll find unofficial Midi controller plugins for various Chase Bliss Pedals.

Current supported interfaces:

* Thermae
* Dark World
* Warped Vinyl
* M O O D
* Generation Loss

**NOTE: You will need the physical CBA pedals (and a Midi-To-USB converter) to use these modules.**

Enjoy.

## Installation Steps

Since these are not distributable on the VCV plug-in library, you'll have to compile
from source and install locally into your Rack Installation. To do this, you'll need to download
the most recent version of the [Rack SDK](https://vcvrack.com/downloads/Rack-SDK-1.1.6.zip), unzip it to some directory (e.g. ~/Downloads/Rack-SDK), and run the following:

`RACK_SDK=~/Downloads/Rack-SDK make install`

Then launch Rack.

## Features
* Dynamic modulation of all knobs using CV inputs, via CV->CC 
* 3-way programmable switches (All)
* Stomp switch controls (All)
* Clock SYNC inputs (Thermae & Warped Vinyl)
* Tap Tempo switch (Thermae & Warped Vinyl)
* "Slow down" mode toggle (Thermae)
* "Self-oscillation" mode via momentary toggle (Thermae)
* Midi tempo divisions (Warped Vinyl)

