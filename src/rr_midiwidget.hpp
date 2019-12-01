#pragma once

#include <app/LedDisplay.hpp>
#include <app/common.hpp>
#include <helpers.hpp>

using namespace std;

namespace rack {

  struct RRMidiDriverItem : ui::MenuItem {
    midi::Port* port;
    int driverId;
    void onAction(const event::Action& e) override {
      port->setDriverId(driverId);
    }
  };

  struct RRMidiDriverChoice : LedDisplayChoice {
    midi::Port* port;
    int chosenDriverId = -1;
    void onAction(const event::Action& e) override {
      if (!port) {
        return;
      }

      ui::Menu* menu = createMenu();
      menu->addChild(createMenuLabel("MIDI driver"));
      for (int driverId : port->getDriverIds()) {
        RRMidiDriverItem* item = new RRMidiDriverItem;
        item->port = port;
        item->driverId = driverId;
        item->text = port->getDriverName(driverId);
        item->rightText = CHECKMARK(item->driverId == port->driverId);
        menu->addChild(item);
      }
    }
    void step() override {
      if (!text.empty() && port && chosenDriverId == port->driverId) {
        return;
      }

      text = port ? port->getDriverName(port->driverId) : "";
      if (text.empty()) {
        text = "(No driver)";
        chosenDriverId = -1;
        color.a = 0.5f;
      }
      else {
        chosenDriverId = port->driverId;
        color.a = 1.f;
      }
    }
  };

  struct RRMidiDeviceItem : ui::MenuItem {
    midi::Port* port;
    int deviceId;
    void onAction(const event::Action& e) override {
      port->setDeviceId(deviceId);
    }
  };

  struct RRMidiChannelItem : ui::MenuItem {
    midi::Port* port;
    int channel;
    void onAction(const event::Action& e) override {
      port->channel = channel;
    }
  };

  struct RRMidiChannelChoice : LedDisplayChoice {
    midi::Port* port;
    int chosenChannel = -1;
    void onAction(const event::Action& e) override {
      if (!port) {
        return;
      }

      ui::Menu* menu = createMenu();
      menu->addChild(createMenuLabel("MIDI channel"));
      for (int channel : port->getChannels()) {
        RRMidiChannelItem* item = new RRMidiChannelItem;
        item->port = port;
        item->channel = channel;
        item->text = port->getChannelName(channel);
        item->rightText = CHECKMARK(item->channel == port->channel);
        menu->addChild(item);
      }
    }
    void step() override {
      if (!text.empty() && port && chosenChannel == port->channel)
        return;
      text = port ? port->getChannelName(port->channel) : "Channel 1";
      if (port)
        chosenChannel = port->channel;
    }
  };

  struct RRMidiDeviceChoice : LedDisplayChoice {
    int chosenDeviceId = -1;
    midi::Port* port;
    void onAction(const event::Action& e) override {
      if (!port)
        return;

      ui::Menu* menu = createMenu();
      menu->addChild(createMenuLabel("MIDI Device"));
      {
        RRMidiDeviceItem* item = new RRMidiDeviceItem;
        item->port = port;
        item->deviceId = -1;
        item->text = "(No Device)";
        item->rightText = CHECKMARK(item->deviceId == port->deviceId);
        menu->addChild(item);
      }
      for (int deviceId : port->getDeviceIds()) {
        RRMidiDeviceItem* item = new RRMidiDeviceItem;
        item->port = port;
        item->deviceId = deviceId;
        item->text = port->getDeviceName(deviceId);
        item->rightText = CHECKMARK(item->deviceId == port->deviceId);
        menu->addChild(item);
      }
    }

    void step() override {
      // cache the device name until the deviceId changes.
      if (!text.empty() && port && chosenDeviceId == port->deviceId)
        return;

      text = port ? port->getDeviceName(port->deviceId) : "";
      if (text.empty()) {
        text = "(No Device)";
        chosenDeviceId = -1;
        color.a = 0.5f;
      } else {
        chosenDeviceId = port->deviceId;
        color.a = 1.f;
      }
    }

  };

  struct RRMidiWidget : LedDisplay {
    RRMidiDriverChoice* driverChoice;
    LedDisplaySeparator* driverSeparator;
    RRMidiDeviceChoice* deviceChoice;
    LedDisplaySeparator* deviceSeparator;
    RRMidiChannelChoice* channelChoice;

    void setMidiPort(midi::Port* port) {
      clearChildren();

      math::Vec pos;
      driverChoice = createWidget<RRMidiDriverChoice>(pos);
      driverChoice->box.size.x = box.size.x;
      driverChoice->port = port;
      addChild(driverChoice);
      pos = driverChoice->box.getBottomLeft();

      driverSeparator = createWidget<LedDisplaySeparator>(pos);
      driverSeparator->box.size.x = box.size.x;
      addChild(driverSeparator);

      deviceChoice = createWidget<RRMidiDeviceChoice>(pos);
      deviceChoice->box.size.x = box.size.x;
      deviceChoice->port = port;
      addChild(deviceChoice);

      pos = deviceChoice->box.getBottomLeft();
      deviceSeparator = createWidget<LedDisplaySeparator>(pos);
      deviceSeparator->box.size.x = box.size.x;
      addChild(deviceSeparator);

      channelChoice = createWidget<RRMidiChannelChoice>(pos);
      channelChoice->box.size.x = box.size.x;
      channelChoice->port = port;
      addChild(channelChoice);

    }

  };
}
