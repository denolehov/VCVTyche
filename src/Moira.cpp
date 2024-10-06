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

	static float easeInOut(float t) {
		if (t < 0.5)
			return 4 * t * t * t;

		return (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
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

		variantChangeDivider.setDivision(16384);
		lightDivider.setDivision(512);
	}

	float PHASE_ADVANCE_SPEED = dsp::FREQ_A4;

	float variant = 1.f;
	double phase = 0;

	CrossFadeFilter outCrossfadeFilter;
	CrossFadeFilter auxCrossfadeFilter;

	dsp::ClockDivider variantChangeDivider;
	dsp::ClockDivider lightDivider;

	dsp::SchmittTrigger triggerInput;
	dsp::SchmittTrigger resetTrigger;

	float AUX_OFFSET = 300.f;

	OutputChangeTracker mainOutputTracker;
	OutputChangeTracker auxOutputTracker;

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

		calculateProbabilities();
		updatedTrackedOutputs();
		updateOutVoltagesWithFade(args.sampleTime);
		updateLights();
	}

	void updatedTrackedOutputs() {
		const bool triggered = triggerInput.process(inputs[TRIGGER_INPUT].getVoltage(), 0.1f, 1.f);
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

	void updateOutVoltagesWithFade(const float delta)
	{
		const float fadeDuration = getParam(FADE_PARAM).getValue();

		if (mainOutputTracker.hasChanged())
			outCrossfadeFilter.start(fadeDuration);

		if (auxOutputTracker.hasChanged())
			auxCrossfadeFilter.start(fadeDuration);

		const float prevOutVoltage = getActiveOutputVoltage(mainOutputTracker.getPreviousOutput());
		const float currOutVoltage = getActiveOutputVoltage(mainOutputTracker.getCurrentOutput());
		getOutput(OUT_OUTPUT).setVoltage(outCrossfadeFilter.process(prevOutVoltage, currOutVoltage, delta));

		const float prevAuxVoltage = getActiveOutputVoltage(auxOutputTracker.getPreviousOutput());
		const float currAuxVoltage = getActiveOutputVoltage(auxOutputTracker.getCurrentOutput());
		getOutput(AUX_OUTPUT).setVoltage(auxCrossfadeFilter.process(prevAuxVoltage, currAuxVoltage, delta));
	}

	float getActiveOutputVoltage(const OutputChangeTracker::Output output)
	{
		switch (output) {
		case OutputChangeTracker::X:
			return getVoltageAt(X_VALUE_PARAM, X_VALUE_INPUT);
		case OutputChangeTracker::Y:
			return getVoltageAt(Y_VALUE_PARAM, Y_VALUE_INPUT);
		case OutputChangeTracker::Z:
			return getVoltageAt(Z_VALUE_PARAM, Z_VALUE_INPUT);
		default:
			return 0.f;
		}
	}

	enum Color { WHITE, ORANGE, BLUE };

	void updateLights() {
		if (!lightDivider.process())
			return;

		setLight(X_PROB_LIGHT, p.x, WHITE);
		setLight(Y_PROB_LIGHT, p.y, WHITE);
		setLight(Z_PROB_LIGHT, p.z, WHITE);

		updateOutputLight(mainOutputTracker.getCurrentOutput(), ORANGE);
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
		case ORANGE:
			red = brightness;
			green = brightness * 0.5f;
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

	float getVoltageAt(const ParamId param, const InputId input)
	{
		float value = getParam(param).getValue();

		if (getInput(input).isConnected())
		{
			value = getInput(input).getVoltage();
			value *= rescale(getParam(param).getValue(), -10.f, 10.f, 0.f, 1.f);
		}

		return value;
	}

	void reset() override
	{
		phase = 0;
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
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.647, 60.0)), module, Moira::X_VALUE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22.86, 60.0)), module, Moira::Y_VALUE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(38.073, 60.0)), module, Moira::Z_VALUE_PARAM));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(7.647, 96.5)), module, Moira::VARIANT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22.695, 96.5)), module, Moira::FADE_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 43.5)), module, Moira::X_PROB_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(22.86, 43.5)), module, Moira::Y_PROB_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(38.073, 43.5)), module, Moira::Z_PROB_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 70.264)), module, Moira::X_VALUE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(22.86, 70.271)), module, Moira::Y_VALUE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(38.073, 70.271)), module, Moira::Z_VALUE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 114.233)), module, Moira::TRIGGER_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(22.695, 114.233)), module, Moira::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(37.62, 96.5)), module, Moira::AUX_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(37.62, 114.233)), module, Moira::OUT_OUTPUT));
	}
};


Model* modelMoira = createModel<Moira, MoiraWidget>("Moira");
