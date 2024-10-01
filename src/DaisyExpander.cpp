#include "DaisyExpander.h"

template<typename T, typename ...Args>
std::unique_ptr<T> make_unique(Args&& ...args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

DaisyExpander::DaisyExpander() : noise(make_unique<OpenSimplexNoise::Noise>())
{
    getLeftExpander().producerMessage = &messages[0];
    getLeftExpander().consumerMessage = &messages[1];
}

void DaisyExpander::process(const ProcessArgs& args)
{
    processIncomingMessage();
}

void DaisyExpander::reseedNoise(const int seed)
{
    noise = make_unique<OpenSimplexNoise::Noise>(seed);
}

void DaisyExpander::processIncomingMessage()
{
    // ReSharper disable once CppReinterpretCastFromVoidPtr
    auto* message = reinterpret_cast<Message*>(getLeftExpander().consumerMessage);
    if (!message || message->processed)
        return;

    if (message->seed != seed || message->globalReset)
    {
        reseedNoise(message->seed);
        reset();
        seed = message->seed;
    }

    if (message->clockReceived)
        onClock(message->clock);

    message->processed = true;

    propagateToDaisyChained(*message);
}

void DaisyExpander::propagateToDaisyChained(const Message& message)
{
    Module* rightModule = getRightExpander().module;
    if (!isExpanderCompatible(rightModule))
        return;

    // ReSharper disable once CppReinterpretCastFromVoidPtr
    auto* producerMessage = reinterpret_cast<Message*>(rightModule->getLeftExpander().producerMessage);
    if (!producerMessage)
        return;

    *producerMessage = message;
    producerMessage->processed = false;

    rightModule->getLeftExpander().requestMessageFlip();
}

bool isExpanderCompatible(Module* module)
{
    return dynamic_cast<DaisyExpander*>(module) != nullptr;
}

void DaisyExpander::reset() {}

void DaisyExpander::onClock(uint32_t clock) {}
