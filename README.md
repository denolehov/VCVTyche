# Tyche modules

Tyche (`/ˈtaɪki/`) is a set of modules for VCV Rack, designed to bring controlled, reproducible randomness to the modular patches.

![Tyche modules](/res/img/tyche-modules.png)

At the moment, Tyche modules include:

- **Omen**: a central module holding the seed, which is shared with all connected modules.
- **Tale**: a random voltage generator that outputs both stepped and smooth random outputs. Capable of being both a **very** slow LFO, or an audio-rate noise oscillator.
- **Kron**: a probabilistic trigger generator, emitting triggers based on clock division and density.
- **Blank**: a simple utility module that can be used to visually group the modules without breaking the expander chain.

**Omen** is the only standalone module in the collection, all other modules are its expanders. Connect **Omen** with just one module for a simple setup, or chain dozens of them to create a central randomness hub controlling the entire patch.

## Tyche Omen

"Omen" is the central module of the Tyche collection, generating the "seed" that controls the randomness of all connected modules. The seed allows the creation of deterministic, reproducible random patterns across patch saves.

### Parameters, Inputs, and Outputs

#### Seed Buttons
- Six momentary buttons cycle through six states (Alpha, Beta, Gamma, Delta, Epsilon, Zeta), changing the seed value and corresponding LED color.
- The seed is saved with the patch, and 46,656 unique seeds are possible.

#### Clock Input
- Expects a 24ppqn clock signal to drive the connected modules.

#### Reset Input
- Triggers a global reset of Omen and all connected modules.

## Tyche Tale

"Tale" is a random voltage generator, producing control voltages influenced by the seed from Omen.

### Parameters, Inputs, and Outputs

#### Pace
- **Pace Slider**: Controls noise evolution speed (0.001 Hz to 440 Hz).
- **Pace Input**: CV input to modulate pace. When connected, the slider acts as an attenuverter.

#### Variant
- **Variant Knob**: Changes the characteristics of the noise (1-128).

#### Sample and Hold Input
- **S&H Input**: Samples the noise at incoming triggers for stepped outputs. If unused, outputs continuously evolving noise.

#### Reset Input
- Resets the noise generator to its initial state.

#### Output
- Outputs the generated noise (-5V to +5V).

### Usage Notes
- **Sample & Hold**: Create stepped random voltages or rhythmic patterns.
- **Reset Sync**: Use reset to create repeating noise patterns aligned with other modules.

## Tyche Kron

"Kron" is a probabilistic trigger generator, outputting triggers at clock divisions based on the density parameter, influenced by Omen's seed.

This module is heavily inspired by the [Rhythm Explorer](https://github.com/DaveBenham/VenomModules?tab=readme-ov-file#rhythm-explorer) from VenomModules.

### Parameters, Inputs, and Outputs

#### Density
- **Density Slider**: Controls trigger probability at each clock division (0%-100%).
- **Density Input**: CV input to modulate density (-5V to +5V). When patched, the density slider becomes an attenuverter.
- **Density Light**: Yellow for triggers, Red for muted triggers, Off for no trigger.

#### Variant
- **Variant Knob**: Alters randomness characteristics (1-128).

#### Mute Input
- Suppresses trigger output when gate voltage ≥ 0.1V.
- **NOTE**: When Kron is muted by another Kron that is _ahead_ of it in the expander chain, the mute will not work due to a 1-sample delay in the signal path. Please let me know if you know how to fix this!

#### Reset Input
- Resets the internal clock counter.

#### Trigger Output
- Outputs +10V triggers at 1ms length.

### Usage Notes
- **Clock Division**: Select clock divisions from 1/2 to 1/16, with triplet and dotted options available via right-click menu.
- **Synchronization**: Use reset to synchronize Kron with other modules.
- **Mute**: Useful for performance control or chaining Krons together for a linear rhythm.
- **Variants**: Experiment with different variant settings to alter randomness patterns without affecting the rest of the connected Tyche modules.
- **Dynamic Rhythms**: Modulate density for evolving patterns.
- **Gates**: Patch Kron with VCV Gates (or any other gate source) to convert triggers to gates.
  - **Tip**: Modulate gate length with Tale!

## Tyche Blank

**Blank** serves as a placeholder within the expander chain (being an expander itself), allowing you to create clean visual sections in your patches while maintaining connectivity between the Tyche modules.

### Parameters, Inputs, and Outputs

- **Blank** has no parameters, inputs, outputs, or lights. It’s purely for visual and organizational purposes.

## Issues and feedback

If you have any feedback, suggestions, or you've discovered a bug, please [open an issue](https://github.com/denolehov/VCVTyche/issues).
