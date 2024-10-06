# Tyche modules

Tyche (`/ˈtaɪki/`) is a set of modules for VCV Rack, designed to bring controlled, reproducible randomness to the modular patches.

<div align="center">
  <img src="/res/img/tyche-modules.png" alt="tyche-modules" width="400px">
</div>

At the moment, Tyche modules include:

- **Omen**: a central module holding the seed, which is shared with all connected modules.
- **Tale**: a random voltage generator that outputs both stepped and smooth random outputs. Capable of being both a **very** slow LFO, or an audio-rate noise oscillator.
- **Kron**: a probabilistic trigger generator, emitting triggers based on clock division and density.
- **Fate**: a probabilistic trigger routing module (Bernoulli gate) that routes incoming triggers to one of two outputs based on a user-defined probability.
- **Moira**: a probabilistic voltage selector that outputs one of three voltages based on defined probabilities.
- **Blank**: a simple utility module that can be used to visually group the modules without breaking the expander chain.

**Omen** is the only standalone module in the collection, all other modules are its expanders. Connect **Omen** with just one module for a simple setup, or chain dozens of them to create a central randomness hub controlling the entire patch.

## Omen

"Omen" is the central module of the Tyche collection, generating the "seed" that controls the randomness of all connected modules. The seed allows the creation of deterministic, reproducible random patterns across patch saves.

### Parameters, Inputs, and Outputs

#### Seed Buttons

- Six momentary buttons cycle through six states (Alpha, Beta, Gamma, Delta, Epsilon, Zeta), changing the seed value and corresponding LED color.
- The seed is saved with the patch, and 46,656 unique seeds are possible.

#### Clock Input

- Expects a 24ppqn clock signal to drive the connected modules.

#### Reset Input

- Triggers a global reset of Omen and all connected modules.

## Tale

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

## Kron

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

## Fate

**Fate** is a probabilistic trigger routing module (Bernoulli gate) in the Tyche collection. It routes incoming triggers to one of two outputs based on a user-defined probability, with its randomness influenced by the seed from the **Omen** module.

### Parameters, Inputs, and Outputs

#### Probability

- **Bias Knob**: Sets the likelihood of routing triggers to **Output A**. A probability of 50% means equal chances for **Output A** or **Output B**.
- **Bias Input**: CV input (-5V to +5V) to modulate the probability externally. When patched, the bias knob is deactivated.

#### Gate / Trigger Input

- Receives incoming triggers or gates to be routed.

#### Reset Input

- Resets the internal routing logic, resetting any probabilistic decisions. Useful for keeping probability routing synced with other elements in your patch.

#### Outputs

- **Output A**: Receives the trigger if the probabilistic decision favors this output.
- **Output B**: Receives the trigger if not routed to **Output A**.

### Usage Notes

- **Latch mode**: Use right-click context menu to enable latch mode, where the last trigger is held until the next trigger arrives.
- **Seed Influence**: The randomness is driven by the seed from **Omen**, making the probabilistic routing consistent and repeatable across patches.
- **External Modulation**: Use CV modulation to dynamically change the probability during a patch, allowing for more evolving and fluid routing behavior.
- **Generative Patching**: Combine **Fate** with sequencers, random generators, or rhythmic modules for creative, probabilistic control over trigger paths.
- **Dynamic Signal Routing**: Modulate probability with an LFO or envelope to create ever-changing trigger paths in generative patches.
- **Reset for Sync**: Use the **Reset Input** to create looping sequences.

## Moira

"Moira" is a probabilistic voltage selector in the Tyche collection. It outputs one of three user-defined voltages (**X**, **Y**, or **Z**) based on relative probabilities, with its randomness influenced by the seed from the **Omen** module.

### Parameters, Inputs, and Outputs

#### Probabilities

- **X Probability Slider**: Sets the relative probability for voltage **X** (0%-100%).
- **Y Probability Slider**: Sets the relative probability for voltage **Y** (0%-100%).
- **Z Probability Slider**: Sets the relative probability for voltage **Z** (0%-100%).
- **X Probability Input**: CV input (-5V to +5V) to modulate the probability of **X**. When patched, the slider acts as an attenuator for the patched signal.
- **Y Probability Input**: CV input to modulate the probability of **Y**.
- **Z Probability Input**: CV input to modulate the probability of **Z**.

#### Voltages

- **X Voltage Knob**: Sets the output voltage for **X** (-10V to +10V).
- **Y Voltage Knob**: Sets the output voltage for **Y** (-10V to +10V).
- **Z Voltage Knob**: Sets the output voltage for **Z** (-10V to +10V).
- **X Voltage Input**: CV input to override the **X** voltage. When patched, the knob acts as an attenuverter.
- **Y Voltage Input**: CV input to override the **Y** voltage.
- **Z Voltage Input**: CV input to override the **Z** voltage.

#### Variant

- **Variant Knob**: Changes the characteristics of the randomness (1-128).

#### Fade

- **Fade Knob**: Sets the duration of voltage crossfade when switching outputs (0s to 20s).

#### Trigger Input

- **Trigger Input**: Triggers the probabilistic selection of **X**, **Y**, or **Z**.

#### Reset Input

- **Reset Input**: Resets the internal random number generator.

#### Outputs

- **Main Output**: Outputs the selected voltage (**X**, **Y**, or **Z**).
- **AUX Output**: Outputs a secondary voltage, based on the probabilities of the other two voltages.

### Usage Notes

- **Probabilistic Selection**: Use the probability sliders and inputs to define the likelihood of each voltage being selected when a trigger is received.
- **Fade**: Use the fade parameter to smoothly transition between voltages when the selection changes.
- **Variant**: Adjust the variant knob to change the pattern of randomness without affecting other connected Tyche modules.
- **Seed Influence**: The randomness is driven by the seed from **Omen**, making the probabilistic selection consistent and repeatable across patches.
- **Dynamic Probabilities**: Modulate the probability inputs for evolving probabilistic behavior.
- **Multi-dimensional Control**: Use the **AUX Output** for additional modulation possibilities, as it provides a secondary selection based on the remaining voltages.
- **Reset for Sync**: Use the **Reset Input** to synchronize the random selection with other elements in your patch.

## Blank

**Blank** serves as a placeholder within the expander chain (being an expander itself), allowing you to create clean visual sections in your patches while maintaining connectivity between the Tyche modules.

### Parameters, Inputs, and Outputs

- **Blank** has no parameters, inputs, outputs, or lights. It’s purely for visual and organizational purposes.

## Issues and feedback

If you have any feedback, suggestions, or you've discovered a bug, please [open an issue](https://github.com/denolehov/VCVTyche/issues).
