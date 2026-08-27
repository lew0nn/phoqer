#include "CoreSanity.h"
#include "../PluginProcessor.h"
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    PhoqerAudioProcessor processor;
    auto result = phoqer::CoreSanity::run(processor.getEngineForDiagnostics(),
                                          processor.getValueTreeState());

    std::cout << "silence=" << result.silenceIsSilent
              << " polyphony=" << result.eightVoicesFinite
              << " extremes=" << result.extremesBounded
              << " formants=" << result.formantSweepFinite
              << " vowels=" << result.vowelAnchorsDistinct
              << " evolution=" << result.callEvolutionCoherent
              << " contrasts=" << result.extremeCombinationsDistinct
              << " repeats=" << result.repeatedNotesVary
              << " reprepare=" << result.reprepareFinite
              << " personalities=" << result.personalitiesVary
              << " telemetry=" << result.telemetryNormalized << std::endl;
    return result.passed() ? 0 : 1;
}
