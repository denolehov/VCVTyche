#include "DaisyExpander.h"
#include "plugin.hpp"


struct SlewFilter
{
	float value = 0.f;

	float process(const float in, const float slew)
	{
		value += math::clamp(in - value, -slew, slew);
		return value;
	}

	float getValue() const
	{
		return value;
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
		SLEW_PARAM,
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

		struct SlewQuantity : ParamQuantity
		{
			float getDisplayValue() override
			{
				if (getValue() <= getMinValue())
					return 0.f;
				return ParamQuantity::getDisplayValue();
			}
		};
		configParam<SlewQuantity>(SLEW_PARAM, std::log2(1e-3f), std::log2(10.f), std::log2(1e-3f), "Slew", " ms/V", 2, 1000);

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
	}

	float PHASE_ADVANCE_SPEED = dsp::FREQ_A4;

	float variant = 1.f;
	double phase = 0;

	SlewFilter outSlewFilter;

	dsp::ClockDivider variantChangeDivider;
	dsp::SchmittTrigger triggerInput;
	dsp::SchmittTrigger resetTrigger;

	float targetOutputVoltage = 0.f;

	void process(const ProcessArgs& args) override {
		DaisyExpander::process(args);

		if (resetTrigger.process(getInput(RESET_INPUT).getVoltage()))
			reset();

		phase += args.sampleTime * PHASE_ADVANCE_SPEED;

		const float newVariant = getParam(VARIANT_PARAM).getValue();
		if (variant != newVariant && variantChangeDivider.process())
			variant = newVariant;

		const float xProbParam = getProbabilityAt(X_PROB_PARAM, X_PROB_INPUT);
		const float yProbParam = getProbabilityAt(Y_PROB_PARAM, Y_PROB_INPUT);
		const float zProbParam = getProbabilityAt(Z_PROB_PARAM, Z_PROB_INPUT);

		const float totalProb = xProbParam + yProbParam + zProbParam;
		if (totalProb <= 0.f)
		{
			setLight(X_PROB_LIGHT, 0.f, args.sampleTime);
			setLight(Y_PROB_LIGHT, 0.f, args.sampleTime);
			setLight(Z_PROB_LIGHT, 0.f, args.sampleTime);
			return;
		}

		const float xProb = xProbParam / totalProb;
		const float yProb = yProbParam / totalProb;
		const float zProb = zProbParam / totalProb;

		setLight(X_PROB_LIGHT, xProb, args.sampleTime);
		setLight(Y_PROB_LIGHT, yProb, args.sampleTime);
		setLight(Z_PROB_LIGHT, zProb, args.sampleTime);

		const bool triggered = triggerInput.process(inputs[TRIGGER_INPUT].getVoltage(), 0.1f, 1.f);
		if (!triggered)
		{
			updateOutVoltage(args.sampleTime);
			return;
		}

		const float noiseVal = rescale(noise->eval(variant, phase), -1.f, 1.f, 0.f, 1.f);

		DEBUG("TOTAL: %f, X: %f, Y: %f, Z: %f, NOISE: %f", totalProb, xProb, yProb, zProb, noiseVal);

		if (xProb >= 0.f && noiseVal < xProb)
		{
			targetOutputVoltage = getVoltageAt(X_VALUE_PARAM, X_VALUE_INPUT);
		} else if (yProb >= 0.f && noiseVal <= xProb + yProb)
		{
			targetOutputVoltage = getVoltageAt(Y_VALUE_PARAM, Y_VALUE_INPUT);
		} else if (zProb >= 0.f)
		{
			targetOutputVoltage = getVoltageAt(Z_VALUE_PARAM, Z_VALUE_INPUT);
		}
	}

	void updateOutVoltage(float delta)
	{
		float slewAmount = getParam(SLEW_PARAM).getValue();
		if (slewAmount <= std::log2(1e-3f))
			slewAmount = -INFINITY;

		float slew = INFINITY;
		if (std::isfinite(slewAmount))
			slew = dsp::exp2_taylor5(-slewAmount + 30.f) / std::exp2(30.f);

		const float slewDelta = slew * delta;

		const float outputVoltage = outSlewFilter.process(targetOutputVoltage, slewDelta);
		getOutput(OUT_OUTPUT).setVoltage(outputVoltage);
	}

	void setLight(const LightId lightId, const float val, const float delta)
	{
		getLight(lightId + 0).setBrightnessSmooth(1.f * val, delta);
		getLight(lightId + 1).setBrightnessSmooth(1.f * val, delta);
		getLight(lightId + 2).setBrightnessSmooth(1.f * val, delta);
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
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22.695, 96.5)), module, Moira::SLEW_PARAM));

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
