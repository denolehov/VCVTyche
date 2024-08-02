#include "plugin.hpp"
#include "DaisyExpander.h"


enum class LightColor{ RED, YELLOW, OFF };


struct Kron final : DaisyExpander {
	enum ParamId {
		DENSITY_PARAM,
		DIVISION_PARAM,
		VARIANT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
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

	Kron() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(DENSITY_PARAM, 0.f, 100.f, 0.f, "Density", "%");
		configSwitch(DIVISION_PARAM, 0.f, 11.f, 0.f, "Division", {"1/2", "1/2t", "1/2.", "1/4", "1/4t", "1/4.", "1/8", "1/8t", "1/8.", "1/16", "1/16t", "1/16."});
		configParam(VARIANT_PARAM, 1.f, 128.f, 1.f, "Variant");
		configInput(MUTE_INPUT, "Mute");
		configInput(RESET_INPUT, "Reset");
		configOutput(OUT_OUTPUT, "Trigger");
	}

	uint32_t clock = 0;
	uint32_t division = 48;
	float variant = 1;

	uint32_t prevReceivedClock = 0;

	bool clockTriggered = false;

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

	void process(const ProcessArgs& args) override {
		handleReset();
		DaisyExpander::process(args);
		handleVariantChange();
		handleDivisionChange();

		const bool clockDivisionTriggered = clock % division == 0;
		const bool isBlocked = getInput(MUTE_INPUT).getVoltage() >= 0.1f;

		const float noiseVal = rescale(noise->eval(variant, clock), -1.f, 1.f, 0.f, 100.f);

		const float density = getParam(DENSITY_PARAM).getValue();
		bool noiseGate = false;
		if (density >= noiseVal)
			noiseGate = true;

		if (clockTriggered && clockDivisionTriggered && noiseGate && !isBlocked)
		{
			pulse.trigger(1e-3f);
			setLight(DENSITY_LIGHT, LightColor::YELLOW, args.sampleTime);
		} else if (clockTriggered && clockDivisionTriggered && noiseGate && isBlocked)
		{
			setLight(DENSITY_LIGHT, LightColor::RED, args.sampleTime);
		} else
		{
			setLight(DENSITY_LIGHT, LightColor::OFF, args.sampleTime);
		}

		clockTriggered = false;

		getOutput(OUT_OUTPUT).setVoltage(pulse.process(args.sampleTime) ? 10.f : 0.f);
	}

	void handleReset()
	{
		const float resetIn = getInput(RESET_INPUT).getVoltage();
		if (resetTrigger.process(resetIn, 0.1f, 2.f))
			reset();
	}

	void handleDivisionChange()
	{
		const float divisionParam = getParam(DIVISION_PARAM).getValue();
		const int index = static_cast<int>(divisionParam);
		const auto newDivision = divisionMapping[index];

		if (division != newDivision && clock == 0)
		{
			division = newDivision;
			DEBUG("DIVISION IS SET TO %d", newDivision);
		}
	}

	void handleVariantChange()
	{
		const float newVariant = getParam(VARIANT_PARAM).getValue();
		if (newVariant != variant && clock == 0)
		{
			variant = newVariant;
			DEBUG("VARIANT IS SET TO %f", variant);
		}
	}

	void onClock(const uint32_t clock) override
	{
		clockTriggered = true;

		if (clock == 1)
		{
			this->clock = clock;
			prevReceivedClock = clock;
			return;
		}

		const auto clockDelta = static_cast<int32_t>(clock - prevReceivedClock);
		if (clockDelta > 1)
			DEBUG("CLOCK DELTA IS %d; CLOCK: %d, PREV CLOCK: %d", clockDelta, clock, prevReceivedClock);

		this->clock += clockDelta;
		prevReceivedClock = clock;
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
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(7.647, 43.5)), module, Kron::DIVISION_PARAM));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(7.647, 60.0)), module, Kron::VARIANT_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 78)), module, Kron::MUTE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 114.233)), module, Kron::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 96.5)), module, Kron::OUT_OUTPUT));
	}
};


Model* modelKron = createModel<Kron, KronWidget>("Kron");
