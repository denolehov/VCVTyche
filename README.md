# Tyche modules

Tyche (`/ˈtaɪki/`) is a set of modules for VCV Rack, designed to bring controlled, reproducible randomness to the modular patches.

![Tyche modules](/res/img/tyche-modules.png)

At the moment, Tyche modules include:

- **Omen**: a central module holding the seed, which is shared with all connected modules
- **Tale**: a random voltage generator that outputs both stepped and smooth random outputs. Capable of being both a **very** slow LFO, or a audio-rate noise oscillator
- **Kron**: a probabilistic trigger generator, emits triggers as per configured clock division and density
- **Blank**: a simple utility module that can be used to visually group the modules without breaking the expander chain
- 
At the heart of the system is **Omen**, the central module that holds the "seed" - a value that governs the random behaviour of all connected modules. The seed allows you to create random, but deterministic patterns, which are reproducible across patch saves. By changing the seed, you can explore new variations while always having the ability to revert to previous states.

**Omen** is the only standalone module in the collection, all other modules are its expanders. Connect **Omen** with just one module for a simple setup, or chain dozens of them to create a central randomness hub controlling the entire patch.

## Issues and feedback

If you have any feedback, suggestions, or you've discovered a bug, please [open an issue](https://github.com/denolehov/VCVTyche/issues).
