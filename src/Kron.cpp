#include "plugin.hpp"
#include "DaisyExpander.h"


enum class LightColor{ RED, YELLOW, OFF };


struct Kron final : DaisyExpander {
	enum ParamId {
		DENSITY_PARAM,
		VARIANT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		DENSITY_INPUT,
		MUTE_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(DENSITY_LIGHT, 3),
		LIGHTS_LEN
	};

	uint32_t clock = 0;
	uint32_t division = 48;
	int divisionIdx = 0;

	float variant = 1;
	dsp::ClockDivider variantChangeDivider;

	uint32_t globalClock = 0;

	bool clockProcessed = true;

	const std::array<uint32_t, 12> divisionMapping = {
		48,	// 1/2
		32,	// 1/2t
		72, // 1/2.
		24, // 1/4
		16, // 1/4t
		36, // 1/4.
		12, // 1/8
		8,	// 1/8t
		18, // 1/8.
		6,	// 1/16
		4,	// 1/16t
		9	// 1/16.
	};

	dsp::SchmittTrigger resetTrigger;
	dsp::PulseGenerator pulse;

	Kron() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(DENSITY_PARAM, 0.f, 100.f, 0.f, "Density", "%");
		configParam(VARIANT_PARAM, 1.f, 128.f, 1.f, "Variant");
		configInput(DENSITY_INPUT, "Density");
		configInput(MUTE_INPUT, "Mute");
		configInput(RESET_INPUT, "Reset");
		configOutput(OUT_OUTPUT, "Trigger");

		variantChangeDivider.setDivision(16384);
	}


	void process(const ProcessArgs& args) override {
		handleReset();
		DaisyExpander::process(args);
		handleVariantChange();
		division = divisionMapping[divisionIdx];

		const bool clockDivisionTriggered = clock % division == 0;
		const bool isBlocked = getInput(MUTE_INPUT).getVoltage() >= 0.1f;

		const float noiseVal = rescale(noise->eval(variant, clock), -1.f, 1.f, 0.f, 100.f);

		const float density = getDensity();
		bool noiseGate = false;
		if (density >= noiseVal)
			noiseGate = true;

		if (!clockProcessed && clockDivisionTriggered && noiseGate && !isBlocked)
		{
			pulse.trigger(1e-3f);
			setLight(DENSITY_LIGHT, LightColor::YELLOW, args.sampleTime);
		} else if (!clockProcessed && clockDivisionTriggered && noiseGate && isBlocked)
		{
			setLight(DENSITY_LIGHT, LightColor::RED, args.sampleTime);
		} else
		{
			setLight(DENSITY_LIGHT, LightColor::OFF, args.sampleTime);
		}

		clockProcessed = true;

		getOutput(OUT_OUTPUT).setVoltage(pulse.process(args.sampleTime) ? 10.f : 0.f);
	}

	float getDensity()
	{
		float densityFactor = getParam(DENSITY_PARAM).getValue();
		densityFactor = rescale(densityFactor, 0.f, 100.f, 0.f, 1.f);

		float maxDensity = 100.f;
		if (getInput(DENSITY_INPUT).isConnected())
		{
			const float d = getInput(DENSITY_INPUT).getVoltage();
			maxDensity = rescale(d, -5.f, 5.f, 0.f, 100.f);
		}

		return maxDensity * densityFactor;
	}

	void handleReset()
	{
		const float resetIn = getInput(RESET_INPUT).getVoltage();
		if (resetTrigger.process(resetIn, 0.1f, 2.f))
			reset();
	}

	void handleVariantChange()
	{
		const float newVariant = getParam(VARIANT_PARAM).getValue();
		if (newVariant != variant && variantChangeDivider.process())
			variant = newVariant;
	}

	void onClock(const uint32_t clock) override
	{
		clockProcessed = false;

		if (clock == 0 || globalClock > clock)
		{
			this->clock = clock;
			globalClock = clock;
			return;
		}

		const auto clockDelta = static_cast<int32_t>(clock - globalClock);
		this->clock += clockDelta;
		globalClock = clock;
	}

	void reset() override
	{
		clock = 0;
		pulse.reset();
	}

	void setLight(const LightId lightIndex, const LightColor color, const float delta)
	{
		switch (color)
		{
		case LightColor::RED:
			getLight(lightIndex + 0).setBrightnessSmooth(1.f, delta);
			getLight(lightIndex + 1).setBrightnessSmooth(0.f, delta);
			getLight(lightIndex + 2).setBrightnessSmooth(0.f, delta);
			break;
		case LightColor::YELLOW:
			getLight(lightIndex + 0).setBrightnessSmooth(1.f, delta);
			getLight(lightIndex + 1).setBrightnessSmooth(1.f, delta);
			getLight(lightIndex + 2).setBrightnessSmooth(0.f, delta);
			break;
		case LightColor::OFF:
			getLight(lightIndex + 0).setBrightnessSmooth(0.f, delta);
			getLight(lightIndex + 1).setBrightnessSmooth(0.f, delta);
			getLight(lightIndex + 2).setBrightnessSmooth(0.f, delta);
			break;
		}
	}

	json_t* dataToJson() override
	{
		json_t* rootJ = json_object();
		json_t* divisionIdxJ = json_integer(divisionIdx);
		json_object_set_new(rootJ, "divisionIdx", divisionIdxJ);

		json_t* variantJ = json_real(variant);
		json_object_set_new(rootJ, "variant", variantJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override
	{
		const json_t* divisionIdxJ = json_object_get(rootJ, "divisionIdx");
		if (divisionIdxJ)
			divisionIdx = static_cast<int>(json_integer_value(divisionIdxJ));

		const json_t* variantJ = json_object_get(rootJ, "variant");
		if (variantJ)
			variant = static_cast<float>(json_integer_value(variantJ));
	}
};


struct RoundSmallBlackSnapKnob final : RoundSmallBlackKnob {
	RoundSmallBlackSnapKnob() {
		snap = true;
	}
};


struct KronWidget final : ModuleWidget {
	explicit KronWidget(Kron* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Kron.svg")));

		addParam(createLightParamCentered<VCVLightSlider<RedGreenBlueLight>>(mm2px(Vec(7.647, 24.0)), module, Kron::DENSITY_PARAM, Kron::DENSITY_LIGHT));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(7.647, 60.0)), module, Kron::VARIANT_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 43.5)), module, Kron::DENSITY_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 78)), module, Kron::MUTE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 114.233)), module, Kron::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 96.5)), module, Kron::OUT_OUTPUT));
	}

	void appendContextMenu(ui::Menu* menu) override
	{
		Kron* module = getModule<Kron>();

		menu->addChild(new MenuSeparator());
		menu->addChild(createIndexPtrSubmenuItem("Division", {"1/2", "1/2t", "1/2.", "1/4", "1/4t", "1/4.", "1/8", "1/8t", "1/8.", "1/16", "1/16t", "1/16."}, &module->divisionIdx));
	}
};


Model* modelKron = createModel<Kron, KronWidget>("Kron");
