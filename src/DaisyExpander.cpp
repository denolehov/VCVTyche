#include "DaisyExpander.h"

DaisyExpander::DaisyExpander() : noise(new OpenSimplexNoise::Noise())
{
    getLeftExpander().producerMessage = &messages[0];
    getRightExpander().producerMessage = &messages[1];
}

void DaisyExpander::process(const ProcessArgs& args)
{
    processIncomingMessage();
}

void DaisyExpander::reseedNoise(const int seed)
{
    noise.reset(new OpenSimplexNoise::Noise(seed));
}

void DaisyExpander::processIncomingMessage()
{
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

void DaisyExpander::propagateToDaisyChained(const Message& message)
{
    Module* rightModule = getRightExpander().module;
    if (!isExpanderCompatible(rightModule))
        return;

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

bool DaisyExpander::isExpanderCompatible(Module* module)
{
    return dynamic_cast<DaisyExpander*>(module) != nullptr;
}

void DaisyExpander::reset() {}

void DaisyExpander::onClock(int clock) {}
