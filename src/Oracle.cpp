#include "plugin.hpp"

constexpr int NUM_SEED_PARAMS = 6;
constexpr int DOTTED_ONE_HALF_DIVISION = 72;


enum SeedState { A, B, C, D, E, F };

SeedState& operator++(SeedState& state) {
	switch (state) {
		case A: return state = B;
		case B: return state = C;
		case C: return state = D;
		case D: return state = E;
		case E: return state = F;
		case F: return state = A;
	}
	return state;
}


struct Oracle final : Module {
	enum ParamId {
		ALPHA_PARAM,
		BETA_PARAM,
		GAMMA_PARAM,
		DELTA_PARAM,
		EPSILON_PARAM,
		ZETA_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(ALPHA_LIGHT, 3),
		ENUMS(BETA_LIGHT, 3),
		ENUMS(GAMMA_LIGHT, 3),
		ENUMS(DELTA_LIGHT, 3),
		ENUMS(EPSILON_LIGHT, 3),
		ENUMS(ZETA_LIGHT, 3),
		LIGHTS_LEN
	};

	ParamId seedButtons[NUM_SEED_PARAMS] = {ALPHA_PARAM, BETA_PARAM, GAMMA_PARAM, DELTA_PARAM, EPSILON_PARAM, ZETA_PARAM};
	dsp::BooleanTrigger pushTriggers[NUM_SEED_PARAMS];
	LightId seedLights[NUM_SEED_PARAMS] = {ALPHA_LIGHT, BETA_LIGHT, GAMMA_LIGHT, DELTA_LIGHT, EPSILON_LIGHT, ZETA_LIGHT};
	SeedState seedConfiguration[NUM_SEED_PARAMS] = {A};
	int seed = 0;

	dsp::SchmittTrigger clockTrigger;
	dsp::ClockDivider clockDivider;

	dsp::SchmittTrigger resetTrigger;

	Oracle() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(ALPHA_PARAM, "Alpha");
		configButton(BETA_PARAM, "Beta");
		configButton(GAMMA_PARAM, "Gamma");
		configButton(DELTA_PARAM, "Delta");
		configButton(EPSILON_PARAM, "Epsilon");
		configButton(ZETA_PARAM, "Zeta");
		configInput(CLOCK_INPUT, "Clock (24ppqn)");
		configInput(RESET_INPUT, "Reset");

		clockDivider.setDivision(DOTTED_ONE_HALF_DIVISION);
	}

	void process(const ProcessArgs& args) override {
		bool seedChanged = false;
		for (int i = 0; i < NUM_SEED_PARAMS; i++)
		{
			if (isSeedButtonPushed(i))
			{
				seedConfiguration[i] = ++seedConfiguration[i];
				seedChanged = true;
			}
		}

		if (seedChanged)
			updateSeed();

		const bool clockHigh = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage());
		if (clockHigh)
			clockDivider.process();

		const bool resetHigh = resetTrigger.process(inputs[RESET_INPUT].getVoltage());
		if (resetHigh)
			reset();

		// if (seedChanged || clockHigh || resetHigh)
			// TODO: Propagate this to the expanders

		updateSeedButtonColors(args.sampleTime);
	}

	void onReset(const ResetEvent& e) override
	{
		Module::onReset(e);
		reset();
	}

	void onRandomize(const RandomizeEvent& e) override
	{
		for (auto & seedState : seedConfiguration)
			seedState = static_cast<SeedState>(random::u32() % 6);

		updateSeed();

		// TODO: Propagate this to the expanders.
	}

	bool isSeedButtonPushed(const int btnIndex)
	{
		return pushTriggers[btnIndex].process(params[seedButtons[btnIndex]].getValue());
	}

	void updateSeed()
	{
		for (int i = 0; i < NUM_SEED_PARAMS; ++i)
		{
			const int stateValue = static_cast<int>(seedConfiguration[i]) + 1;
			seed ^= stateValue << i * 3;
		}
	}

	void reset()
	{
		clockDivider.reset();
		clockTrigger.reset();
		resetTrigger.reset();
	}

	void updateSeedButtonColors(float delta)
	{
		for (int i = 0; i < NUM_SEED_PARAMS; i++)
		{
			const SeedState seedState = seedConfiguration[i];
			const LightId lightIndex = seedLights[i];

			switch (seedState)
			{
			case A:
				updateLightRGB(lightIndex, 1.f, 0.f, 0.f, delta);
				break;
			case B:
				updateLightRGB(lightIndex, 0.f, 1.f, 0.f, delta);
				break;
			case C:
				updateLightRGB(lightIndex, 0.f, 0.f, 1.f, delta);
				break;
			case D:
				updateLightRGB(lightIndex, 1.f, 1.f, 0.f, delta);
				break;
			case E:
				updateLightRGB(lightIndex, 0.f, 1.f, 1.f, delta);
				break;
			case F:
				updateLightRGB(lightIndex, 1.f, 0.f, 1.f, delta);
				break;
			}
		}
	}

	void updateLightRGB(const LightId lightIndex, const float r, const float g, const float b, const float delta)
	{
		getLight(lightIndex + 0).setBrightnessSmooth(r, delta);
		getLight(lightIndex + 1).setBrightnessSmooth(g, delta);
		getLight(lightIndex + 2).setBrightnessSmooth(b, delta);
	}

	json_t* dataToJson() override
	{
		json_t* rootJ = json_object();

		json_t* seedJ = json_integer(seed);
		json_object_set_new(rootJ, "seed", seedJ);

		json_t* seedConfigurationJ = json_array();
		for (auto & i : seedConfiguration)
		{
			json_t* seedStateJ = json_integer(i);
			json_array_append_new(seedConfigurationJ, seedStateJ);
		}
		json_object_set_new(rootJ, "seedConfiguration", seedConfigurationJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override
	{
		// Seed
		const json_t* seedJ = json_object_get(rootJ, "seed");
		if (seedJ)
			seed = json_integer_value(seedJ);

		// Seed Configuration
		const json_t* seedConfigurationJ = json_object_get(rootJ, "seedConfiguration");
		if (seedConfigurationJ)
		{
			for (int i = 0; i < NUM_SEED_PARAMS; ++i)
			{
				json_t* seedStateJ = json_array_get(seedConfigurationJ, i);
				if (seedStateJ)
					seedConfiguration[i] = static_cast<SeedState>(json_integer_value(seedStateJ));
			}
		}

		// TODO: Propagate the seed to the expanders.
	}
};


struct OracleWidget final : ModuleWidget {
	explicit OracleWidget(Oracle* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Oracle.svg")));

		addParam(createLightParamCentered<LightButton<VCVBezel, VCVBezelLight<RedGreenBlueLight>>>(mm2px(Vec(7.62, 18.0)), module, Oracle::ALPHA_PARAM, Oracle::ALPHA_LIGHT));
		addParam(createLightParamCentered<LightButton<VCVBezel, VCVBezelLight<RedGreenBlueLight>>>(mm2px(Vec(7.62, 30.0)), module, Oracle::BETA_PARAM, Oracle::BETA_LIGHT));
		addParam(createLightParamCentered<LightButton<VCVBezel, VCVBezelLight<RedGreenBlueLight>>>(mm2px(Vec(7.62, 42.0)), module, Oracle::GAMMA_PARAM, Oracle::GAMMA_LIGHT));
		addParam(createLightParamCentered<LightButton<VCVBezel, VCVBezelLight<RedGreenBlueLight>>>(mm2px(Vec(7.62, 54.0)), module, Oracle::DELTA_PARAM, Oracle::DELTA_LIGHT));
		addParam(createLightParamCentered<LightButton<VCVBezel, VCVBezelLight<RedGreenBlueLight>>>(mm2px(Vec(7.62, 66.0)), module, Oracle::EPSILON_PARAM, Oracle::EPSILON_LIGHT));
		addParam(createLightParamCentered<LightButton<VCVBezel, VCVBezelLight<RedGreenBlueLight>>>(mm2px(Vec(7.62, 78.0)), module, Oracle::ZETA_PARAM, Oracle::ZETA_LIGHT));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 92.5)), module, Oracle::CLOCK_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 110.233)), module, Oracle::RESET_INPUT));
	}
};


Model* modelOracle = createModel<Oracle, OracleWidget>("Oracle");
