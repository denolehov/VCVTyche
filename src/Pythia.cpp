#include "plugin.hpp"


struct Pythia : Module {
	enum ParamId {
		SPEED_PARAM,
		VARIANT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		HOLD_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Pythia() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SPEED_PARAM, 0.01f, 1.f, 0.01f, "Speed");
		configParam(VARIANT_PARAM, 1.f, 128.f, 1.f, "Variant");
		configInput(HOLD_INPUT, "Sample and hold");
		configInput(RESET_INPUT, "Reset");
		configOutput(OUT_OUTPUT, "Main");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct RoundLargeBlackSnapKnob final : RoundLargeBlackKnob {
	RoundLargeBlackSnapKnob() {
		snap = true;
	}
};


struct PythiaWidget final : ModuleWidget {
	explicit PythiaWidget(Pythia* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Pythia.svg")));

		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(12.7, 25.5)), module, Pythia::SPEED_PARAM));
		addParam(createParamCentered<RoundLargeBlackSnapKnob>(mm2px(Vec(12.7, 49.5)), module, Pythia::VARIANT_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(12.7, 73.0)), module, Pythia::HOLD_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(12.7, 110.0)), module, Pythia::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(12.7, 92.5)), module, Pythia::OUT_OUTPUT));
	}
};


Model* modelPythia = createModel<Pythia, PythiaWidget>("Pythia");
