#include "DaisyExpander.h"
#include "plugin.hpp"


struct Tale final : DaisyExpander {
	enum ParamId {
		PACE_PARAM,
		VARIANT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		PACE_INPUT,
		SAMPLE_AND_HOLD_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PACE_LIGHT, 3),
		LIGHTS_LEN
	};

	double phase = 0;
	float variant = 1.f;

	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger sampleAndHoldTrigger;

	const float minSpeed = 0.001f;
	const float maxSpeed = dsp::FREQ_A4;

	float heldNoiseValue = 0.f;

	dsp::ClockDivider variantChangeDivider;
	dsp::ClockDivider lightDivider;

	Tale() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PACE_PARAM, 0.f, 1.f, .5f, "Pace");
		configParam(VARIANT_PARAM, 1.f, 128.f, 1.f, "Variant");
		configInput(PACE_INPUT, "Pace");
		configInput(SAMPLE_AND_HOLD_INPUT, "S&H");
		configInput(RESET_INPUT, "Reset");
		configOutput(OUT_OUTPUT, "Main");

		variantChangeDivider.setDivision(16384);
	}

	void process(const ProcessArgs& args) override {
		DaisyExpander::process(args);

		if (resetTrigger.process(getInput(RESET_INPUT).getVoltage()))
			reset();

		const float newVariant = getParam(VARIANT_PARAM).getValue();
		if (variant != newVariant && variantChangeDivider.process())
			variant = newVariant;

		float outCV = heldNoiseValue;
		if (getInput(SAMPLE_AND_HOLD_INPUT).isConnected())
		{
			if (sampleAndHoldTrigger.process(getInput(SAMPLE_AND_HOLD_INPUT).getVoltage()))
			{
				heldNoiseValue = outCV = noise->eval(variant, phase);
			}
		} else
		{
			outCV = noise->eval(variant, phase);
		}

		outCV = rescale(outCV, -1.f, 1.f, -5.f, 5.f);
		getOutput(OUT_OUTPUT).setVoltage(outCV);

		float pace = getParam(PACE_PARAM).getValue();
		if (getInput(PACE_INPUT).isConnected())
			pace *= rescale(getInput(PACE_INPUT).getVoltage(), -5.f, 5.f, 0.f, 1.f);

		const float speed = minSpeed * std::pow(maxSpeed / minSpeed, pace);
		phase += speed * args.sampleTime;

		if (lightDivider.process())
			setLight(PACE_LIGHT, outCV, args.sampleTime);
}

	void setLight(LightId lightId, float val, float delta)
	{
		val = rescale(val, -5.f, 5.f, -1.f, 1.f);
		if (val >= 0)
		{
			getLight(lightId + 0).setBrightnessSmooth(0, delta);
			getLight(lightId + 1).setBrightnessSmooth(1.f * val, delta);
			getLight(lightId + 2).setBrightnessSmooth(0.f, delta);
		} else
		{
			getLight(lightId + 0).setBrightnessSmooth(1.f * std::fabs(val), delta);
			getLight(lightId + 1).setBrightnessSmooth(0.f, delta);
			getLight(lightId + 2).setBrightnessSmooth(0.f, delta);
		}
	}

	void reset() override
	{
		phase = 0;
	}

	json_t* dataToJson() override
	{
		json_t* rootJ = json_object();

		json_t* variantJ = json_real(variant);
		json_object_set_new(rootJ, "variant", variantJ);

		json_t* heldNoiseValueJ = json_real(heldNoiseValue);
		json_object_set_new(rootJ, "heldNoiseValue", heldNoiseValueJ);

		json_t* phaseJ = json_real(phase);
		json_object_set_new(rootJ, "phase", phaseJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override
	{
		const json_t* variantJ = json_object_get(rootJ, "variant");
		if (variantJ)
			variant = static_cast<float>(json_real_value(variantJ));

		const json_t* heldNoiseValueJ = json_object_get(rootJ, "heldNoiseValue");
		if (heldNoiseValueJ)
			heldNoiseValue = static_cast<float>(json_real_value(heldNoiseValueJ));

		const json_t* phaseJ = json_object_get(rootJ, "phase");
		if (phaseJ)
			phase = json_real_value(phaseJ);
	}
};


struct RoundSmallBlackSnapKnob final : RoundSmallBlackKnob {
	RoundSmallBlackSnapKnob() {
		snap = true;
	}
};


struct TaleWidget final : ModuleWidget {
	explicit TaleWidget(Tale* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Tale.svg")));

		addParam(createLightParamCentered<VCVLightSlider<RedGreenBlueLight>>(mm2px(Vec(7.647, 24.0)), module, Tale::PACE_PARAM, Tale::PACE_LIGHT));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(7.647, 60.0)), module, Tale::VARIANT_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 43.5)), module, Tale::PACE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 78.0)), module, Tale::SAMPLE_AND_HOLD_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 114.233)), module, Tale::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 96.5)), module, Tale::OUT_OUTPUT));
	}
};


Model* modelTale = createModel<Tale, TaleWidget>("Tale");
