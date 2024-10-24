#include "DaisyExpander.h"
#include "plugin.hpp"


struct CrossFadeFilter {
	float fadeTime = 0.f, fadeProgress = 0.f;
	bool isFading = false;

	void start(const float duration) {
		if (duration <= 0.f) {
			isFading = false;
		} else {
			fadeTime = duration;
			fadeProgress = 0.f;
			isFading = true;
		}
	}

	float process(const float prevVoltage, const float currVoltage, const float deltaTime) {
		if (!isFading)
			return currVoltage;

		fadeProgress += deltaTime;
		const float t = clamp(fadeProgress / fadeTime, 0.f, 1.f);

		const float value = crossfade(prevVoltage, currVoltage, easeInOut(t));

		if (t >= 1.f)
			isFading = false;

		return value;
	}

	static float easeInOut(const float t) {
		if (t < 0.5)
			return 4 * t * t * t;

		return (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
	}

	json_t* dataToJson() const {
		json_t* rootJ = json_object();

		json_t* fadeTimeJ = json_real(fadeTime);
		json_object_set_new(rootJ, "fadeTime", fadeTimeJ);

		json_t* fadeProgressJ = json_real(fadeProgress);
		json_object_set_new(rootJ, "fadeProgress", fadeProgressJ);

		json_t* isFadingJ = json_boolean(isFading);
		json_object_set_new(rootJ, "isFading", isFadingJ);

		return rootJ;
	}

	void dataFromJson(const json_t* rootJ) {
		const json_t* fadeTimeJ = json_object_get(rootJ, "fadeTime");
		if (fadeTimeJ)
			fadeTime = static_cast<float>(json_real_value(fadeTimeJ));

		const json_t* fadeProgressJ = json_object_get(rootJ, "fadeProgress");
		if (fadeProgressJ)
			fadeProgress = static_cast<float>(json_real_value(fadeProgressJ));

		const json_t* isFadingJ = json_object_get(rootJ, "isFading");
		if (isFadingJ)
			isFading = json_boolean_value(isFadingJ);
	}
};


struct OutputChangeTracker {
	enum Output { X, Y, Z, NONE };
	Output currentOutput = NONE;
	Output previousOutput = NONE;

	bool hasOutputChanged = false;

	bool process(const Output newState) {
		hasOutputChanged = false;

		if (newState != currentOutput) {
			previousOutput = currentOutput;
			currentOutput = newState;
			hasOutputChanged = true;
		}

		return hasOutputChanged;
	}

	Output getPreviousOutput() const {
		return previousOutput;
	}

	Output getCurrentOutput() const {
		return currentOutput;
	}

	bool hasChanged() const {
		return hasOutputChanged;
	}

	json_t* dataToJson() const {
		json_t* rootJ = json_object();

		json_t* currentOutputJ = json_integer(currentOutput);
		json_object_set_new(rootJ, "currentOutput", currentOutputJ);

		json_t* previousOutputJ = json_integer(previousOutput);
		json_object_set_new(rootJ, "previousOutput", previousOutputJ);

		return rootJ;
	}

	void dataFromJson(const json_t* rootJ) {
		const json_t* currentOutputJ = json_object_get(rootJ, "currentOutput");
		if (currentOutputJ)
			currentOutput = static_cast<Output>(json_integer_value(currentOutputJ));

		const json_t* previousOutputJ = json_object_get(rootJ, "previousOutput");
		if (previousOutputJ)
			previousOutput = static_cast<Output>(json_integer_value(previousOutputJ));

		if (currentOutput != previousOutput)
			hasOutputChanged = true;
	}
};


struct Moira final : DaisyExpander {
	enum ParamId {
		X_PROB_PARAM,
		Y_PROB_PARAM,
		Z_PROB_PARAM,
		X_VALUE_PARAM,
		Y_VALUE_PARAM,
		Z_VALUE_PARAM,
		VARIANT_PARAM,
		FADE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		X_PROB_INPUT,
		Y_PROB_INPUT,
		Z_PROB_INPUT,
		X_VALUE_INPUT,
		Y_VALUE_INPUT,
		Z_VALUE_INPUT,
		TRIGGER_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		X_CHOSEN_OUTPUT,
		Y_CHOSEN_OUTPUT,
		Z_CHOSEN_OUTPUT,
		AUX_OUTPUT,
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(X_PROB_LIGHT, 3),
		ENUMS(Y_PROB_LIGHT, 3),
		ENUMS(Z_PROB_LIGHT, 3),
		LIGHTS_LEN
	};

	Moira() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(X_PROB_PARAM, 0.f, 100.f, 50.f, "X Relative probability", "%");
		configParam(Y_PROB_PARAM, 0.f, 100.f, 50.f, "Y Relative probability", "%");
		configParam(Z_PROB_PARAM, 0.f, 100.f, 50.f, "Z Relative probability", "%");
		configParam(X_VALUE_PARAM, -10.f, 10.f, 0.f, "X");
		configParam(Y_VALUE_PARAM, -10.f, 10.f, 0.f, "Y");
		configParam(Z_VALUE_PARAM, -10.f, 10.f, 0.f, "Z");
		configParam(VARIANT_PARAM, 1.f, 128.f, 1.f, "Variant");
		configParam(FADE_PARAM, 0.f, 20.f, 0.f, "Fade duration", " s");

		configInput(X_PROB_INPUT, "X Probability");
		configInput(Y_PROB_INPUT, "Y Probability");
		configInput(Z_PROB_INPUT, "Z Probability");
		configInput(X_VALUE_INPUT, "X");
		configInput(Y_VALUE_INPUT, "Y");
		configInput(Z_VALUE_INPUT, "Z");
		configInput(TRIGGER_INPUT, "Trigger");
		configInput(RESET_INPUT, "Reset");
		configOutput(AUX_OUTPUT, "AUX");
		configOutput(OUT_OUTPUT, "Main");

		configOutput(X_CHOSEN_OUTPUT, "X chosen trigger");
		configOutput(Y_CHOSEN_OUTPUT, "Y chosen trigger");
		configOutput(Z_CHOSEN_OUTPUT, "Z chosen trigger");

		variantChangeDivider.setDivision(16384);
		lightDivider.setDivision(512);
	}

	float PHASE_ADVANCE_SPEED = dsp::FREQ_A4;

	int seed = 0;
	float variant = 1.f;
	double phase = 0;

	CrossFadeFilter outCrossfadeFilters[PORT_MAX_CHANNELS];
	CrossFadeFilter auxCrossfadeFilters[PORT_MAX_CHANNELS];

	dsp::ClockDivider variantChangeDivider;
	dsp::ClockDivider lightDivider;

	dsp::SchmittTrigger triggerInput;
	dsp::SchmittTrigger resetTrigger;

	float AUX_OFFSET = 300.f;

	OutputChangeTracker mainOutputTracker;
	OutputChangeTracker auxOutputTracker;

	dsp::PulseGenerator xPulse, yPulse, zPulse;

	struct Probabilities {
		float x = 0.f;
		float y = 0.f;
		float z = 0.f;

		Probabilities(float x, float y, float z) {
			const float totalProb = x + y + z;
			if (totalProb > 0.f) {
				this->x = x / totalProb;
				this->y = y / totalProb;
				this->z = z / totalProb;
			}
		}

		Probabilities() = default;

		float total() const {
			return x + y + z;
		}
	};

	Probabilities p;

	void calculateProbabilities() {
		const float xProbParam = getProbabilityAt(X_PROB_PARAM, X_PROB_INPUT);
		const float yProbParam = getProbabilityAt(Y_PROB_PARAM, Y_PROB_INPUT);
		const float zProbParam = getProbabilityAt(Z_PROB_PARAM, Z_PROB_INPUT);

		p = Probabilities(xProbParam, yProbParam, zProbParam);
	}

	void process(const ProcessArgs& args) override {
		DaisyExpander::process(args);

		phase += args.sampleTime * PHASE_ADVANCE_SPEED;

		if (resetTrigger.process(getInput(RESET_INPUT).getVoltage()))
			reset();

		const float newVariant = getParam(VARIANT_PARAM).getValue();
		if (variant != newVariant && variantChangeDivider.process())
			variant = newVariant;

		const bool triggered = triggerInput.process(inputs[TRIGGER_INPUT].getVoltage(), 0.1f, 1.f);

		calculateProbabilities();
		updatedTrackedOutputs(triggered);

		int numChannels = 1;
		if (inputs[X_VALUE_INPUT].isConnected())
			numChannels = std::max(numChannels, inputs[X_VALUE_INPUT].getChannels());
		if (inputs[Y_VALUE_INPUT].isConnected())
			numChannels = std::max(numChannels, inputs[Y_VALUE_INPUT].getChannels());
		if (inputs[Z_VALUE_INPUT].isConnected())
			numChannels = std::max(numChannels, inputs[Z_VALUE_INPUT].getChannels());

		outputs[OUT_OUTPUT].setChannels(numChannels);
		outputs[AUX_OUTPUT].setChannels(numChannels);

		const float fadeDuration = getParam(FADE_PARAM).getValue();

		if (mainOutputTracker.hasChanged()) {
			for (int c = 0; c < numChannels; c++) {
				outCrossfadeFilters[c].start(fadeDuration);
			}
		}

		if (auxOutputTracker.hasChanged()) {
			for (int c = 0; c < numChannels; c++) {
				auxCrossfadeFilters[c].start(fadeDuration);
			}
		}

		for (int c = 0; c < numChannels; c++) {
			updateOutVoltagesWithFade(c, args.sampleTime);
		}

		updateChosenOutput(triggered, args.sampleTime);
		updateLights();
	}

	void updateChosenOutput(const bool triggered, const float delta) {
		const OutputChangeTracker::Output chosenOutput = mainOutputTracker.getCurrentOutput();
		if (!mainOutputTracker.hasChanged() && !triggered) {
			updatedChosenOutputTriggers(delta);
			return;
		}

		switch (chosenOutput) {
		case OutputChangeTracker::X:
			xPulse.trigger(1e-3);
			break;
		case OutputChangeTracker::Y:
			yPulse.trigger(1e-3);
			break;
		case OutputChangeTracker::Z:
			zPulse.trigger(1e-3);
			break;
		default:
			xPulse.reset(); yPulse.reset(); zPulse.reset();
		}

		updatedChosenOutputTriggers(delta);
	}

	void updatedChosenOutputTriggers(const float delta) {
		getOutput(X_CHOSEN_OUTPUT).setVoltage(xPulse.process(delta) ? 10.f : 0.f);
		getOutput(Y_CHOSEN_OUTPUT).setVoltage(yPulse.process(delta) ? 10.f : 0.f);
		getOutput(Z_CHOSEN_OUTPUT).setVoltage(zPulse.process(delta) ? 10.f : 0.f);
	}

	void updatedTrackedOutputs(const bool triggered) {
		if (!triggered) {
			mainOutputTracker.process(mainOutputTracker.getCurrentOutput());
			auxOutputTracker.process(auxOutputTracker.getCurrentOutput());
			return;
		}

		const float noiseVal = sampleNoise();

		if (p.x >= 0.f && noiseVal < p.x) {
			mainOutputTracker.process(OutputChangeTracker::X);

			if (p.y + p.z == 0.f) {
				auxOutputTracker.process(OutputChangeTracker::X);
				return;
			}

			const float auxYprob = p.y / (p.y + p.z);
			if (sampleNoise(AUX_OFFSET) < auxYprob) {
				auxOutputTracker.process(OutputChangeTracker::Y);
			} else {
				auxOutputTracker.process(OutputChangeTracker::Z);
			}

		} else if (p.y >= 0.f && noiseVal <= p.x + p.y) {
			mainOutputTracker.process(OutputChangeTracker::Y);

			if (p.x + p.z == 0.f) {
				auxOutputTracker.process(OutputChangeTracker::Y);
				return;
			}

			const float auxXprob = p.x / (p.x + p.z);
			if (sampleNoise(AUX_OFFSET) < auxXprob) {
				auxOutputTracker.process(OutputChangeTracker::X);
			} else {
				auxOutputTracker.process(OutputChangeTracker::Z);
			}

		} else if (p.z > 0.f) {
			mainOutputTracker.process(OutputChangeTracker::Z);

			if (p.x + p.y == 0.f) {
				auxOutputTracker.process(OutputChangeTracker::Z);
				return;
			}

			const float auxXprob = p.x / (p.x + p.y);
			if (sampleNoise(AUX_OFFSET) < auxXprob) {
				auxOutputTracker.process(OutputChangeTracker::X);
			} else {
				auxOutputTracker.process(OutputChangeTracker::Y);
			}
		}
	}

	float sampleNoise(const float offset = 0.f) const
	{
		return rescale(noise->eval(variant, phase + offset), -1.f, 1.f, 0.f, 1.f);
	}

	void updateOutVoltagesWithFade(const int channel, const float delta)
	{
		const float prevOutVoltage = getActiveOutputVoltage(mainOutputTracker.getPreviousOutput(), channel);
		const float currOutVoltage = getActiveOutputVoltage(mainOutputTracker.getCurrentOutput(), channel);
		outputs[OUT_OUTPUT].setVoltage(outCrossfadeFilters[channel].process(prevOutVoltage, currOutVoltage, delta), channel);

		const float prevAuxVoltage = getActiveOutputVoltage(auxOutputTracker.getPreviousOutput(), channel);
		const float currAuxVoltage = getActiveOutputVoltage(auxOutputTracker.getCurrentOutput(), channel);
		outputs[AUX_OUTPUT].setVoltage(auxCrossfadeFilters[channel].process(prevAuxVoltage, currAuxVoltage, delta), channel);
	}

	float getActiveOutputVoltage(const OutputChangeTracker::Output output, const int channel)
	{
		switch (output) {
		case OutputChangeTracker::X:
			return getVoltageAt(X_VALUE_PARAM, X_VALUE_INPUT, channel);
		case OutputChangeTracker::Y:
			return getVoltageAt(Y_VALUE_PARAM, Y_VALUE_INPUT, channel);
		case OutputChangeTracker::Z:
			return getVoltageAt(Z_VALUE_PARAM, Z_VALUE_INPUT, channel);
		default:
			return 0.f;
		}
	}

	enum Color { WHITE, GREEN, BLUE };

	void updateLights() {
		if (!lightDivider.process())
			return;

		setLight(X_PROB_LIGHT, p.x, WHITE);
		setLight(Y_PROB_LIGHT, p.y, WHITE);
		setLight(Z_PROB_LIGHT, p.z, WHITE);

		updateOutputLight(mainOutputTracker.getCurrentOutput(), GREEN);
		updateOutputLight(auxOutputTracker.getCurrentOutput(), BLUE);
	}

	void updateOutputLight(const OutputChangeTracker::Output output, Color color) {
		switch (output) {
			case OutputChangeTracker::X:
				setLight(X_PROB_LIGHT, p.x, color);
				break;
			case OutputChangeTracker::Y:
				setLight(Y_PROB_LIGHT, p.y, color);
				break;
			case OutputChangeTracker::Z:
				setLight(Z_PROB_LIGHT, p.z, color);
				break;
			default:
				break;
		}
	}

	void setLight(const LightId lightId, const float brightness, const Color color) {
		float red, green, blue;

		switch (color) {
		case GREEN:
			red = 0.f;
			green = brightness;
			blue = 0.f;
			break;
		case BLUE:
			red = brightness * 0.15f;
			green = brightness * 0.5f;
			blue = brightness;
			break;
		case WHITE:
		default:
				red = brightness;
				green = brightness;
				blue = brightness;
				break;
		}

		getLight(lightId + 0).setBrightness(red);
		getLight(lightId + 1).setBrightness(green);
		getLight(lightId + 2).setBrightness(blue);
	}

	float getProbabilityAt(const ParamId probParam, const InputId probInput)
	{
		float value = getParam(probParam).getValue();

		if (getInput(probInput).isConnected())
		{
			value *= rescale(getInput(probInput).getVoltage(), -5.f, 5.f, 0.f, 1.f);
		}

		return value;
	}

	float getVoltageAt(const ParamId param, const InputId input, const int channel)
	{
		float value = getParam(param).getValue();

		if (getInput(input).isConnected())
		{
			value = getInput(input).getVoltage(channel);
			value *= rescale(getParam(param).getValue(), -10.f, 10.f, 0.f, 1.f);
		}

		return value;
	}

	void reset() override
	{
		phase = 0;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_t* variantJ = json_real(variant);
		json_object_set_new(rootJ, "variant", variantJ);

		json_t* phaseJ = json_real(phase);
		json_object_set_new(rootJ, "phase", phaseJ);

		json_t* mainOutputTrackerJ = mainOutputTracker.dataToJson();
		json_object_set_new(rootJ, "mainOutputTracker", mainOutputTrackerJ);

		json_t* auxOutputTrackerJ = auxOutputTracker.dataToJson();
		json_object_set_new(rootJ, "auxOutputTracker", auxOutputTrackerJ);

		json_t* seedJ = json_integer(seed);
		json_object_set_new(rootJ, "seed", seedJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		const json_t* variantJ = json_object_get(rootJ, "variant");
		if (variantJ)
			variant = static_cast<float>(json_real_value(variantJ));

		const json_t* phaseJ = json_object_get(rootJ, "phase");
		if (phaseJ)
			phase = json_real_value(phaseJ);

		const json_t* mainOutputTrackerJ = json_object_get(rootJ, "mainOutputTracker");
		if (mainOutputTrackerJ)
			mainOutputTracker.dataFromJson(mainOutputTrackerJ);

		const json_t* auxOutputTrackerJ = json_object_get(rootJ, "auxOutputTracker");
		if (auxOutputTrackerJ)
			auxOutputTracker.dataFromJson(auxOutputTrackerJ);

		const json_t* seedJ = json_object_get(rootJ, "seed");
		if (seedJ)
			seed = static_cast<int>(json_integer_value(seedJ));
	}

	void processSeed(int newSeed) override {
		if (seed != newSeed) {
			seed = newSeed;
			reseedNoise(seed);
		}
	}
};

struct RoundSmallBlackSnapKnob final : RoundSmallBlackKnob {
	RoundSmallBlackSnapKnob() {
		snap = true;
	}
};

struct MoiraWidget final : ModuleWidget {
	explicit MoiraWidget(Moira* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Moira.svg")));

		addParam(createLightParamCentered<VCVLightSlider<RedGreenBlueLight>>(mm2px(Vec(7.647, 24.0)), module, Moira::X_PROB_PARAM, Moira::X_PROB_LIGHT));
		addParam(createLightParamCentered<VCVLightSlider<RedGreenBlueLight>>(mm2px(Vec(22.86, 24.0)), module, Moira::Y_PROB_PARAM, Moira::Y_PROB_LIGHT));
		addParam(createLightParamCentered<VCVLightSlider<RedGreenBlueLight>>(mm2px(Vec(38.073, 24.0)), module, Moira::Z_PROB_PARAM, Moira::Z_PROB_LIGHT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.647, 53.75)), module, Moira::X_VALUE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22.86, 53.75)), module, Moira::Y_VALUE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(38.073, 53.75)), module, Moira::Z_VALUE_PARAM));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(7.647, 96.5)), module, Moira::VARIANT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22.695, 96.5)), module, Moira::FADE_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 43.5)), module, Moira::X_PROB_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(22.86, 43.5)), module, Moira::Y_PROB_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(38.073, 43.5)), module, Moira::Z_PROB_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 64.014)), module, Moira::X_VALUE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(22.86, 64.021)), module, Moira::Y_VALUE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(38.073, 64.021)), module, Moira::Z_VALUE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 114.233)), module, Moira::TRIGGER_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(22.695, 114.233)), module, Moira::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 74.791)), module, Moira::X_CHOSEN_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(22.86, 74.791)), module, Moira::Y_CHOSEN_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(38.073, 74.791)), module, Moira::Z_CHOSEN_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(37.62, 96.5)), module, Moira::AUX_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(37.62, 114.233)), module, Moira::OUT_OUTPUT));
	}
};


Model* modelMoira = createModel<Moira, MoiraWidget>("Moira");
