#include "plugin.hpp"
#include "OpenSimplexNoise/OpenSimplexNoise.h"


// region: DaisyExpander
struct Message
{
	int seed = 0;
	bool seedChanged = false;

	bool globalReset = false;

	int clock = 0;
	bool clockReceived = false;

	bool processed = false;

	Message() = default;
};

struct DaisyExpander : Module
{
	std::unique_ptr<OpenSimplexNoise::Noise> noise;
	Message messages[2] = {};

	DaisyExpander() : noise(new OpenSimplexNoise::Noise()) // TODO: Use make_unique
	{
		getLeftExpander().producerMessage = &messages[0];
		getRightExpander().producerMessage = &messages[1];
	}

	~DaisyExpander() override = default;

	void process(const ProcessArgs& args) override
	{
		processIncomingMessage();
	}

	void reseedNoise(const int seed)
	{
		noise.reset(new OpenSimplexNoise::Noise(seed));
	}

	void processIncomingMessage()
	{
		// ReSharper disable once CppReinterpretCastFromVoidPtr
		auto* message = reinterpret_cast<Message*>(getLeftExpander().consumerMessage);
		if (!message || message->processed)
			return;

		if (message->seedChanged)
			reseedNoise(message->seed);

		if (message->globalReset)
			reset();

		if (message->clockReceived)
			onClock(message->clock);

		message->processed = true;

		propagateToDaisyChained(*message);
	}

	void propagateToDaisyChained(const Message message)
	{
		Module* rightModule = getRightExpander().module;
		if (!isExpanderCompatible(rightModule))
			return;

		// ReSharper disable once CppReinterpretCastFromVoidPtr
		auto* producerMessage = reinterpret_cast<Message*>(rightModule->getLeftExpander().producerMessage);
		if (!producerMessage)
		{
			DEBUG("Producer message is null. Was it set in the DaisyExpander constructor?");
			return;
		}

		*producerMessage = message;
		producerMessage->processed = false;

		rightModule->getLeftExpander().requestMessageFlip();
	}

	static bool isExpanderCompatible(Module* module)
	{
		return dynamic_cast<DaisyExpander*>(module) != nullptr;
	}

	virtual void reset() {}
	virtual void onClock(int clock) {}
};


// endregion: DaisyExpander


struct Pythia final : DaisyExpander {
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
