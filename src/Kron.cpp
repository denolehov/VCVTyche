#include "plugin.hpp"


struct Kron : Module {
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
		DENSITY_LIGHT,
		LIGHTS_LEN
	};

	Kron() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.f, "");
		configParam(DIVISION_PARAM, 0.f, 1.f, 0.f, "");
		configParam(VARIANT_PARAM, 0.f, 1.f, 0.f, "");
		configInput(MUTE_INPUT, "");
		configInput(RESET_INPUT, "");
		configOutput(OUT_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct KronWidget : ModuleWidget {
	KronWidget(Kron* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Kron.svg")));

		addParam(createLightParamCentered<VCVLightSlider<RedGreenBlueLight>>(mm2px(Vec(7.647, 24.0)), module, Kron::DENSITY_PARAM, Kron::DENSITY_LIGHT));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.647, 43.5)), module, Kron::DIVISION_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.647, 60.0)), module, Kron::VARIANT_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 78)), module, Kron::MUTE_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 114.233)), module, Kron::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(7.647, 96.5)), module, Kron::OUT_OUTPUT));
	}
};


Model* modelKron = createModel<Kron, KronWidget>("Kron");