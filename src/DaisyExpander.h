#pragma once

#include "plugin.hpp"
#include "OpenSimplexNoise/OpenSimplexNoise.h"

struct Message {
    int seed = 0;
    bool seedChanged = false;

    bool globalReset = false;

    uint32_t clock = 0;
    bool clockReceived = false;

    bool processed = false;

    Message() = default;
};

struct DaisyExpander : Module
{
    std::unique_ptr<OpenSimplexNoise::Noise> noise;
    Message messages[2] = {};

    DaisyExpander();
    ~DaisyExpander() override = default;

    void process(const ProcessArgs& args) override;
    void reseedNoise(int seed);
    void processIncomingMessage();
    void propagateToDaisyChained(const Message& message);


    virtual void reset();
    virtual void onClock(uint32_t clock);
};

bool isExpanderCompatible(Module* module);
