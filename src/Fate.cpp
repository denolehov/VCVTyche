#include "DaisyExpander.h"
#include "plugin.hpp"


struct Fate final : DaisyExpander {
	enum ParamId {
		BIAS_PARAM,
		VARIANT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		BIAS_INPUT,
		IN_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_A_OUTPUT,
		OUT_B_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	enum HoldState { A, B, NONE };

	HoldState holdState = NONE;
	bool canProcessNewGate = true;

	bool latchMode = false;

	int seed = 0;
	double phase = 0;
	float variant = 1.f;

	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger inSchmitt;

	dsp::ClockDivider variantChangeDivider;

	Fate() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(BIAS_PARAM, 0.f, 100.f, 50.f, "Bias", "%");
		configParam(VARIANT_PARAM, 1.f, 128.f, 1.f, "Variant");
		configInput(BIAS_INPUT, "Bias");
		configInput(IN_INPUT, "In");
		configInput(RESET_INPUT, "Reset");
		configOutput(OUT_A_OUTPUT, "A");
		configOutput(OUT_B_OUTPUT, "B");

		variantChangeDivider.setDivision(16384);
	}

	void process(const ProcessArgs& args) override {
		DaisyExpander::process(args);

		phase += args.sampleTime;

		if (resetTrigger.process(getInput(RESET_INPUT).getVoltage()))
			reset();

		const float newVariant = getParam(VARIANT_PARAM).getValue();
		if (variant != newVariant && variantChangeDivider.process())
			variant = newVariant;

		inSchmitt.process(getInput(IN_INPUT).getVoltage(), 0.1f, 1.f);

		const bool gate = inSchmitt.isHigh();
		if (gate && canProcessNewGate)
		{
			const float bias = getBias();
			const double noiseVal = noise->eval(variant, phase);
			holdState = noiseVal >= bias ? A : B;
			canProcessNewGate = false;
		} else if (!gate)
		{
			if (!latchMode)
				holdState = NONE;
			canProcessNewGate = true;
		}

		switch (holdState)
		{
			case A:
				getOutput(OUT_A_OUTPUT).setVoltage(10.f);
				getOutput(OUT_B_OUTPUT).setVoltage(0.f);
				break;
			case B:
				getOutput(OUT_A_OUTPUT).setVoltage(0.f);
				getOutput(OUT_B_OUTPUT).setVoltage(10.f);
				break;
			case NONE:
				getOutput(OUT_A_OUTPUT).setVoltage(0.f);
				getOutput(OUT_B_OUTPUT).setVoltage(0.f);
				break;
		}
	}

	float getBias()
	{
		if (getInput(BIAS_INPUT).isConnected())
			return rescale(getInput(BIAS_INPUT).getVoltage(), -5.f, 5.f, -1.f, 1.f);

		return rescale(getParam(BIAS_PARAM).getValue(), 0.f, 100.f, -1.f, 1.f);
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

		json_t* phaseJ = json_real(phase);
		json_object_set_new(rootJ, "phase", phaseJ);

		json_t* holdStateJ = json_integer(holdState);
		json_object_set_new(rootJ, "holdState", holdStateJ);

		json_t* latchJ = json_boolean(latchMode);
		json_object_set_new(rootJ, "latch", latchJ);

		json_t* canProcessNewGateJ = json_boolean(canProcessNewGate);
		json_object_set_new(rootJ, "canProcessNewGate", canProcessNewGateJ);

		json_t* seedJ = json_integer(seed);
		json_object_set_new(rootJ, "seed", seedJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override
	{
		const json_t* variantJ = json_object_get(rootJ, "variant");
		if (variantJ)
			variant = static_cast<float>(json_real_value(variantJ));

		const json_t* phaseJ = json_object_get(rootJ, "phase");
		if (phaseJ)
			phase = json_real_value(phaseJ);

		const json_t* holdStateJ = json_object_get(rootJ, "holdState");
		if (holdStateJ)
			holdState = static_cast<HoldState>(json_integer_value(holdStateJ));

		const json_t* latchJ = json_object_get(rootJ, "latch");
		if (latchJ)
			latchMode = json_boolean_value(latchJ);

		const json_t* canProcessNewGateJ = json_object_get(rootJ, "canProcessNewGate");
		if (canProcessNewGateJ)
			canProcessNewGate = json_boolean_value(canProcessNewGateJ);

		const json_t* seedJ = json_object_get(rootJ, "seed");
		if (seedJ) {
			seed = static_cast<int>(json_integer_value(seedJ));
			reseedNoise(seed);
		}
	}

	void onSeedChanged(int newSeed) override {
		seed = newSeed;
		reseedNoise(seed);
	}
};


struct RoundSmallBlackSnapKnob final : RoundSmallBlackKnob {
	RoundSmallBlackSnapKnob() {
		snap = true;
	}
};


struct FateWidget final : ModuleWidget {
	explicit FateWidget(Fate* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Fate.svg")));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.468, 15.75)), module, Fate::BIAS_PARAM));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(7.468, 60.0)), module, Fate::VARIANT_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.468, 27.75)), module, Fate::BIAS_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.568, 43.5)), module, Fate::IN_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 114.233)), module, Fate::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(7.698, 78.0)), module, Fate::OUT_A_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(7.698, 96.5)), module, Fate::OUT_B_OUTPUT));
	}

	void appendContextMenu(ui::Menu* menu) override {
		Fate* module = getModule<Fate>();

		menu->addChild(new MenuSeparator());
		menu->addChild(createBoolPtrMenuItem("Latch mode", "", &module->latchMode));
	}
};


Model* modelFate = createModel<Fate, FateWidget>("Fate");
