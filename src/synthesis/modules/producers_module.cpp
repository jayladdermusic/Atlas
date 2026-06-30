/* Copyright 2013-2019 Matt Tytel
 *
 * vital is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * vital is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with vital.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "producers_module.h"

#include <algorithm>

namespace vital {
  ProducersModule::ProducersModule() :
      SynthModule(kNumInputs, kNumOutputs), sample_destination_(nullptr),
      sample_key_zone_start_(nullptr), sample_key_zone_end_(nullptr),
      sample_velocity_zone_start_(nullptr), sample_velocity_zone_end_(nullptr),
      sample_pitch_clamp_(nullptr),
      granular_destination_(nullptr), granular_key_zone_start_(nullptr), granular_key_zone_end_(nullptr),
      granular_velocity_zone_start_(nullptr), granular_velocity_zone_end_(nullptr),
      granular_pitch_clamp_(nullptr), portamento_glide_zones_(nullptr), zone_crossfade_(nullptr),
      filter1_on_(nullptr), filter2_on_(nullptr) {
    sample_zone_gain_ = 0.0f;
    granular_zone_gain_ = 0.0f;
    for (int i = 0; i < kNumOscillators; ++i) {
      std::string number = std::to_string(i + 1);
      oscillators_[i] = new OscillatorModule("osc_" + number);
      addSubmodule(oscillators_[i]);
      addProcessor(oscillators_[i]);
      oscillators_[i]->enable(false);
      oscillator_destinations_[i] = nullptr;
      oscillator_key_zone_start_[i] = nullptr;
      oscillator_key_zone_end_[i] = nullptr;
      oscillator_velocity_zone_start_[i] = nullptr;
      oscillator_velocity_zone_end_[i] = nullptr;
      oscillator_pitch_clamp_[i] = new cr::ZonePitchClamp();
      addProcessor(oscillator_pitch_clamp_[i]);
      oscillator_zone_gain_[i] = 0.0f;
    }

    sampler_ = new SampleModule();
    addSubmodule(sampler_);
    addProcessor(sampler_);
    sampler_->enable(false);
    sample_pitch_clamp_ = new cr::ZonePitchClamp();
    addProcessor(sample_pitch_clamp_);

    granular_ = new GranularModule();
    addSubmodule(granular_);
    addProcessor(granular_);
    granular_->enable(false);
    granular_pitch_clamp_ = new cr::ZonePitchClamp();
    addProcessor(granular_pitch_clamp_);
  }

  void ProducersModule::init() {
    portamento_glide_zones_ = createBaseControl("portamento_glide_zones");
    zone_crossfade_ = createBaseControl("zone_crossfade");

    for (int i = 0; i < kNumOscillators; ++i) {
      std::string number = std::to_string(i + 1);
      oscillator_destinations_[i] = createBaseControl("osc_" + number + "_destination");
      oscillator_key_zone_start_[i] = createBaseControl("osc_" + number + "_key_zone_start");
      oscillator_key_zone_end_[i] = createBaseControl("osc_" + number + "_key_zone_end");
      oscillator_velocity_zone_start_[i] = createBaseControl("osc_" + number + "_velocity_zone_start");
      oscillator_velocity_zone_end_[i] = createBaseControl("osc_" + number + "_velocity_zone_end");

      plugPitchClamp(oscillator_pitch_clamp_[i], oscillator_key_zone_start_[i], oscillator_key_zone_end_[i]);

      oscillators_[i]->useInput(input(kReset), OscillatorModule::kReset);
      oscillators_[i]->useInput(input(kRetrigger), OscillatorModule::kRetrigger);
      oscillators_[i]->plug(oscillator_pitch_clamp_[i]->output(), OscillatorModule::kMidi);
      oscillators_[i]->useInput(input(kActiveVoices), OscillatorModule::kActiveVoices);
      oscillators_[i]->useInput(input(kNotePressed), OscillatorModule::kNotePressed);
    }

    sample_destination_ = createBaseControl("sample_destination");
    sample_key_zone_start_ = createBaseControl("sample_key_zone_start");
    sample_key_zone_end_ = createBaseControl("sample_key_zone_end");
    sample_velocity_zone_start_ = createBaseControl("sample_velocity_zone_start");
    sample_velocity_zone_end_ = createBaseControl("sample_velocity_zone_end");
    plugPitchClamp(sample_pitch_clamp_, sample_key_zone_start_, sample_key_zone_end_);
    sampler_->useInput(input(kReset), SampleModule::kReset);
    sampler_->useInput(input(kRetrigger), SampleModule::kRetrigger);
    sampler_->useInput(input(kVoiceEvent), SampleModule::kVoiceEvent);
    sampler_->useInput(input(kNoteCount), SampleModule::kNoteCount);
    sampler_->useInput(input(kNotePressed), SampleModule::kNotePressed);
    sampler_->plug(sample_pitch_clamp_->output(), SampleModule::kMidi);

    granular_destination_ = createBaseControl("granular_destination");
    granular_key_zone_start_ = createBaseControl("granular_key_zone_start");
    granular_key_zone_end_ = createBaseControl("granular_key_zone_end");
    granular_velocity_zone_start_ = createBaseControl("granular_velocity_zone_start");
    granular_velocity_zone_end_ = createBaseControl("granular_velocity_zone_end");
    plugPitchClamp(granular_pitch_clamp_, granular_key_zone_start_, granular_key_zone_end_);
    granular_->useInput(input(kReset), GranularModule::kReset);
    granular_->useInput(input(kVoiceEvent), GranularModule::kVoiceEvent);
    granular_->useInput(input(kNoteCount), GranularModule::kNoteCount);
    granular_->plug(granular_pitch_clamp_->output(), GranularModule::kMidi);

    SynthModule::init();
    for (int i = 0; i < kNumOscillators; ++i) {
      int index1 = getFirstModulationIndex(i);
      int index2 = getSecondModulationIndex(i);
      int index3 = getThirdModulationIndex(i);
      oscillators_[i]->oscillator()->setFirstOscillatorOutput(oscillators_[index1]->output(OscillatorModule::kRaw));
      oscillators_[i]->oscillator()->setSecondOscillatorOutput(oscillators_[index2]->output(OscillatorModule::kRaw));
      oscillators_[i]->oscillator()->setThirdOscillatorOutput(oscillators_[index3]->output(OscillatorModule::kRaw));
      oscillators_[i]->oscillator()->setSampleOutput(sampler_->output(SampleModule::kRaw));
      oscillators_[i]->oscillator()->setGranularOutput(granular_->output(GranularModule::kRaw));
    }
  }

  void ProducersModule::plugPitchClamp(cr::ZonePitchClamp* clamp, Value* key_start, Value* key_end) {
    clamp->useInput(input(kMidi), cr::ZonePitchClamp::kMidi);
    clamp->useInput(input(kZoneGlideMidi), cr::ZonePitchClamp::kGlideMidi);
    clamp->plug(key_start, cr::ZonePitchClamp::kZoneStart);
    clamp->plug(key_end, cr::ZonePitchClamp::kZoneEnd);
    clamp->plug(portamento_glide_zones_, cr::ZonePitchClamp::kEnable);
  }

  poly_float ProducersModule::getZoneMask(Value* key_start, Value* key_end,
                                          Value* velocity_start, Value* velocity_end) {
    poly_float note = (portamento_glide_zones_->value() != 0.0f) ? input(kZoneGlideMidi)->at(0)
                                                                  : input(kZoneMidi)->at(0);
    poly_float velocity = input(kVelocity)->at(0);
    poly_float key_min = std::min(key_start->value(), key_end->value());
    poly_float key_max = std::max(key_start->value(), key_end->value());
    poly_float velocity_min = std::min(velocity_start->value(), velocity_end->value());
    poly_float velocity_max = std::max(velocity_start->value(), velocity_end->value());

    poly_mask key_mask = poly_float::greaterThanOrEqual(note, key_min) &
                         poly_float::lessThanOrEqual(note, key_max);
    poly_mask velocity_mask = poly_float::greaterThanOrEqual(velocity, velocity_min) &
                              poly_float::lessThanOrEqual(velocity, velocity_max);
    return poly_float(1.0f) & key_mask & velocity_mask;
  }

  ProducersModule::ZoneGain ProducersModule::advanceZoneGain(poly_float& gain_state, poly_float target,
                                                             poly_mask reset_mask, bool crossfade_on,
                                                             mono_float max_move) {
    ZoneGain gain;
    if (crossfade_on) {
      // Snap to the target on note-on so fresh notes don't crossfade in; glides ramp.
      gain.start = utils::maskLoad(gain_state, target, reset_mask);
      poly_float diff = target - gain.start;
      gain.end = gain.start + utils::clamp(diff, -max_move, max_move);
    }
    else {
      gain.start = target;
      gain.end = target;
    }
    gain_state = gain.end;
    return gain;
  }

  void ProducersModule::addZonedBuffer(poly_float* dest, const poly_float* source,
                                       const ZoneGain& gain, int num_samples) const {
    poly_float current = gain.start;
    poly_float delta = (gain.end - gain.start) * (1.0f / num_samples);
    for (int i = 0; i < num_samples; ++i) {
      dest[i] += source[i] * current;
      current += delta;
    }
  }

  void ProducersModule::process(int num_samples) {
    SynthModule::process(num_samples);

    getLocalProcessor(sampler_)->process(num_samples);
    getLocalProcessor(granular_)->process(num_samples);

    SynthOscillator::DistortionType distortion_types[kNumOscillators];
    bool processed[kNumOscillators];
    for (int i = 0; i < kNumOscillators; ++i) {
      distortion_types[i] = oscillators_[i]->getDistortionType();
      processed[i] = false;
    }
   
    int num_processed = 0;
    int index = 0;
    for (int i = 0; i < kNumOscillators * kNumOscillators && num_processed < kNumOscillators; ++i) {
      OscillatorModule* module = oscillators_[index];
      int first_source = getFirstModulationIndex(index);
      int second_source = getSecondModulationIndex(index);
      int third_source = getThirdModulationIndex(index);
      if ((!SynthOscillator::isFirstModulation(distortion_types[index]) || processed[first_source]) &&
          (!SynthOscillator::isSecondModulation(distortion_types[index]) || processed[second_source]) &&
          (!SynthOscillator::isThirdModulation(distortion_types[index]) || processed[third_source]) &&
          !processed[index]) {
        num_processed++;
        processed[index] = true;
        getLocalProcessor(module)->process(num_samples);
      }
      index = (index + 1) % kNumOscillators;
    }

    poly_float* filter1_output = output(kToFilter1)->buffer;
    poly_float* filter2_output = output(kToFilter2)->buffer;
    poly_float* raw_output = output(kRawOut)->buffer;
    poly_float* bus1_output = output(kBus1Out)->buffer;
    poly_float* bus2_output = output(kBus2Out)->buffer;
    poly_float* bus3_output = output(kBus3Out)->buffer;
    poly_float* direct_output = output(kDirectOut)->buffer;
    utils::zeroBuffer(filter1_output, num_samples);
    utils::zeroBuffer(filter2_output, num_samples);
    utils::zeroBuffer(raw_output, num_samples);
    utils::zeroBuffer(bus1_output, num_samples);
    utils::zeroBuffer(bus2_output, num_samples);
    utils::zeroBuffer(bus3_output, num_samples);
    utils::zeroBuffer(direct_output, num_samples);

    bool filter1_on = isFilter1On();
    bool filter2_on = isFilter2On();

    // Zone crossfade smooths the gain when a producer enters or leaves its zone during a glide.
    poly_mask zone_reset_mask = getResetMask(kReset);
    mono_float crossfade_seconds = zone_crossfade_->value();
    bool crossfade_on = portamento_glide_zones_->value() != 0.0f && crossfade_seconds > 0.0f;
    mono_float zone_max_move = crossfade_on ? (num_samples / (crossfade_seconds * getSampleRate())) : 0.0f;

    for (int i = 0; i < kNumOscillators; ++i) {
      const poly_float* buffer = oscillators_[i]->output(OscillatorModule::kLevelled)->buffer;
      ZoneGain zone_gain = advanceZoneGain(oscillator_zone_gain_[i],
          getZoneMask(oscillator_key_zone_start_[i], oscillator_key_zone_end_[i],
                      oscillator_velocity_zone_start_[i], oscillator_velocity_zone_end_[i]),
          zone_reset_mask, crossfade_on, zone_max_move);

      int destination = oscillator_destinations_[i]->value();
      bool raw = destination == constants::kEffects;
      bool filter1 = destination == constants::kFilter1 || destination == constants::kDualFilters;
      bool filter2 = destination == constants::kFilter2 || destination == constants::kDualFilters;
      bool bus1 = destination == constants::kBus1;
      bool bus2 = destination == constants::kBus2;
      bool bus3 = destination == constants::kBus3;
      bool direct_out = destination == constants::kDirectOut;
      if (raw || (!filter2 && filter1 && !filter1_on) ||
                 (!filter1 && filter2 && !filter2_on) ||
                 (filter1 && filter2 && !filter1_on && !filter2_on)) {
        addZonedBuffer(raw_output, buffer, zone_gain, num_samples);
      }
      if (filter1)
        addZonedBuffer(filter1_output, buffer, zone_gain, num_samples);
      if (filter2)
        addZonedBuffer(filter2_output, buffer, zone_gain, num_samples);
      if (bus1)
        addZonedBuffer(bus1_output, buffer, zone_gain, num_samples);
      if (bus2)
        addZonedBuffer(bus2_output, buffer, zone_gain, num_samples);
      if (bus3)
        addZonedBuffer(bus3_output, buffer, zone_gain, num_samples);
      if (direct_out)
        addZonedBuffer(direct_output, buffer, zone_gain, num_samples);
    }

    const poly_float* sample = sampler_->output(SampleModule::kLevelled)->buffer;
    ZoneGain sample_zone_gain = advanceZoneGain(sample_zone_gain_,
        getZoneMask(sample_key_zone_start_, sample_key_zone_end_,
                    sample_velocity_zone_start_, sample_velocity_zone_end_),
        zone_reset_mask, crossfade_on, zone_max_move);

    int sample_destination = sample_destination_->value();
    bool sample_raw = sample_destination == constants::kEffects;
    bool filter1_sample = sample_destination == constants::kFilter1 || sample_destination == constants::kDualFilters;
    bool filter2_sample = sample_destination == constants::kFilter2 || sample_destination == constants::kDualFilters;
    bool sample_bus1 = sample_destination == constants::kBus1;
    bool sample_bus2 = sample_destination == constants::kBus2;
    bool sample_bus3 = sample_destination == constants::kBus3;
    bool sample_direct_out = sample_destination == constants::kDirectOut;
    if (sample_raw || (!filter2_sample && filter1_sample && !filter1_on) ||
                      (!filter1_sample && filter2_sample && !filter2_on) ||
                      (filter1_sample && filter2_sample && !filter1_on && !filter2_on)) {
      addZonedBuffer(raw_output, sample, sample_zone_gain, num_samples);
    }
    if (filter1_sample)
      addZonedBuffer(filter1_output, sample, sample_zone_gain, num_samples);
    if (filter2_sample)
      addZonedBuffer(filter2_output, sample, sample_zone_gain, num_samples);
    if (sample_bus1)
      addZonedBuffer(bus1_output, sample, sample_zone_gain, num_samples);
    if (sample_bus2)
      addZonedBuffer(bus2_output, sample, sample_zone_gain, num_samples);
    if (sample_bus3)
      addZonedBuffer(bus3_output, sample, sample_zone_gain, num_samples);
    if (sample_direct_out)
      addZonedBuffer(direct_output, sample, sample_zone_gain, num_samples);

    const poly_float* granular = granular_->output(GranularModule::kLevelled)->buffer;
    ZoneGain granular_zone_gain = advanceZoneGain(granular_zone_gain_,
        getZoneMask(granular_key_zone_start_, granular_key_zone_end_,
                    granular_velocity_zone_start_, granular_velocity_zone_end_),
        zone_reset_mask, crossfade_on, zone_max_move);

    int granular_destination = granular_destination_->value();
    bool granular_raw = granular_destination == constants::kEffects;
    bool filter1_granular = granular_destination == constants::kFilter1 ||
                            granular_destination == constants::kDualFilters;
    bool filter2_granular = granular_destination == constants::kFilter2 ||
                            granular_destination == constants::kDualFilters;
    bool granular_bus1 = granular_destination == constants::kBus1;
    bool granular_bus2 = granular_destination == constants::kBus2;
    bool granular_bus3 = granular_destination == constants::kBus3;
    bool granular_direct_out = granular_destination == constants::kDirectOut;
    if (granular_raw || (!filter2_granular && filter1_granular && !filter1_on) ||
                        (!filter1_granular && filter2_granular && !filter2_on) ||
                        (filter1_granular && filter2_granular && !filter1_on && !filter2_on)) {
      addZonedBuffer(raw_output, granular, granular_zone_gain, num_samples);
    }
    if (filter1_granular)
      addZonedBuffer(filter1_output, granular, granular_zone_gain, num_samples);
    if (filter2_granular)
      addZonedBuffer(filter2_output, granular, granular_zone_gain, num_samples);
    if (granular_bus1)
      addZonedBuffer(bus1_output, granular, granular_zone_gain, num_samples);
    if (granular_bus2)
      addZonedBuffer(bus2_output, granular, granular_zone_gain, num_samples);
    if (granular_bus3)
      addZonedBuffer(bus3_output, granular, granular_zone_gain, num_samples);
    if (granular_direct_out)
      addZonedBuffer(direct_output, granular, granular_zone_gain, num_samples);

  }
} // namespace vital
