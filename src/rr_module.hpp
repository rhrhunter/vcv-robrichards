#pragma once

#include <sys/time.h>
#include "plugin.hpp"
#include "rr_midi.hpp"
#include <dsp/digital.hpp>

using namespace std;

namespace rack {

struct RRModule : Module {
  // MIDI controller
  RRMidiOutput midi_out;

  // rate limiting
  float rate_limiter_phase = 0.f;

  // tap tempo variables
  bool can_tap_tempo = true;
  struct timeval last_tap_tempo_time;
  double next_blink_usec = 0;
  double next_blink_sec = 0;
  bool start_blinking = false;
  bool first_tap = false;
  double curr_rate_sec = 0;
  double curr_rate_usec = 0;
  float next_brightness = 0.f;

  // random things
  bool lights_off = true;

  // periodic internal clock processing
  dsp::ClockDivider enable_midi_clk;

  RRModule() {
    // get the current time, this is so we can keep throttle tap
    // tempo messages.
    gettimeofday(&last_tap_tempo_time, NULL);

    // keep an internal clock to re-enable the midi clock (~every 6s)
    enable_midi_clk.setDivision(524288);
  }

  bool should_rate_limit(const float period, float sample_time) {
    rate_limiter_phase += sample_time / period;
    if (rate_limiter_phase >= 1.f) {
      // no rate limiting needed, reduce the phase by 1
      rate_limiter_phase -= 1.f;
      return false;
    } else {
      // apply rate limiting
      return true;
    }
  }

  bool disable_module() {
    bool was_disabled = lights_off;
    lights_off = true;
    return was_disabled;
  }

  void enable_module() {
    lights_off = false;
  }

  void process_midi_clock(bool enable_clock) {
      // turn on midi clock (just in case it is off)
      midi_out.setValue(127, 51);

      // send a clock pulse
      midi_out.setClock(enable_clock);
  }

  void reset_midi_clock_cc_cache() {
    midi_out.resetCCCache(51);
  }

  float process_tap_tempo(int tap_tempo) {
    float ret_brightness = -1.f;

    // if the tap tempo button was pressed, force a midi message to be sent
    if (can_tap_tempo && tap_tempo) {
      // we are allowing tap tempo actions and they tapped the tempo button
      midi_out.resetCCCache(93);
      midi_out.setValue(1, 93);
      can_tap_tempo = false;

      // measure the amount of time from the previous tap tempo to this tap tempo.
      // this will be the tap tempo rate (for the tap tempo LED).
      double last_time_usec = (double) last_tap_tempo_time.tv_usec;
      double last_time_sec = (double) last_tap_tempo_time.tv_sec;

      // get a new measurement
      struct timeval ctime;
      gettimeofday(&ctime, NULL);
      double this_time_usec = (double) ctime.tv_usec;
      double this_time_sec = (double) ctime.tv_sec;

      // calculate the next time we need to blink (dont allow slower than a 2s rate)
      next_blink_usec = this_time_usec - last_time_usec;
      if (next_blink_usec > 1000000)
        next_blink_usec = 1000000;
      next_blink_usec /= 2;
      next_blink_usec += this_time_usec;

      next_blink_sec = this_time_sec  - last_time_sec;
      if (next_blink_sec > 2)
        next_blink_sec = 2;
      next_blink_sec /= 2;
      next_blink_sec += this_time_sec;

      // update the current time
      last_tap_tempo_time = ctime;

      // keep track of whether two taps have occurred so far
      if (!start_blinking && !first_tap) {
        // first tap occurred
        first_tap = true;
      } else if (!start_blinking && first_tap) {
        // second tap occurred, start blinking the light
        start_blinking = true;
        // the next time we blink, turn on the light
        next_brightness = 1.f;
      }

      // calculate the blink rate based on the last two taps
      // if we were told to start blinking
      if (start_blinking) {
        curr_rate_sec = next_blink_sec - this_time_sec;
        curr_rate_usec = next_blink_usec - this_time_usec;
      }

    } else if (tap_tempo) {
      // they wanted to do a tap tempo, but they did it too fast.
      // calculate if we are allowed to tap next time
      struct timeval ctime;
      gettimeofday(&ctime, NULL);

      // calculate if enough time has elapsed (>100ms)
      double elapsed = (double)(ctime.tv_usec - last_tap_tempo_time.tv_usec) / 1000000 +
        (double)(ctime.tv_sec - last_tap_tempo_time.tv_sec);
      if (elapsed > 0.1)
        can_tap_tempo = true;
    } else if (start_blinking) {
      // no tap tempo button was clicked and we have a stored "next blink time",
      // determine if we should blink the tempo light right now
      struct timeval ctime;
      gettimeofday(&ctime, NULL);

      // if the current time is greater than the next blink time, flash the light.
      // the next process iteration will turn it off
      double this_time_usec = (double) ctime.tv_usec;
      double this_time_sec = (double) ctime.tv_sec;
      double elapsed_usec = (double) (this_time_usec - next_blink_usec);
      double elapsed_sec = (double) (this_time_sec - next_blink_sec);
      double elapsed = (double) ((elapsed_sec) + (elapsed_usec / 1000000));
      if (elapsed > 0) {
        // flash the tap tempo light for the active color
        ret_brightness = next_brightness;

        // flip the brightness value for the next blink
        next_brightness = !next_brightness;

        // store the current time for the next blink, add rate and
        // subtract the amount we went over because this accounts for
        // the drift we may have experienced.
        next_blink_usec = (this_time_usec + curr_rate_usec) - elapsed_usec;
        next_blink_sec = (this_time_sec + curr_rate_sec) - elapsed_sec;
      }
    }

    // return values:
    //   ret_brightness < 0  --> don't change brightness.
    //   ret_brightness >= 0 --> change brightness according
    //                           to the retuned value.
    return ret_brightness;
  }

  bool should_transition_to_state(float time_until, struct timeval grace_period) {
    // get a new measurement
    struct timeval ctime;
    gettimeofday(&ctime, NULL);
    double this_time_usec = (double) ctime.tv_usec;
    double this_time_sec = (double) ctime.tv_sec;

    // calculate whether we should transition to the next state
    double last_time_usec = (double) grace_period.tv_usec;
    double last_time_sec = (double) grace_period.tv_sec;
    double diff_sec = this_time_sec - last_time_sec;
    double diff_usec = this_time_usec - last_time_usec;
    float diff = (float) (diff_sec + (diff_usec/1000000));
    if (diff > time_until) {
      // transition to next state
      return true;
    } else {
      // stay in current state
      return false;
    }
  }

};

}
