#include "CoreSanity.h"
#include <iostream>

int main()
{
    phoqer::PhoqerEngine engine;
    auto result = phoqer::CoreSanity::run(engine);

    std::cout << "silence=" << result.silenceIsSilent
              << " characters=" << result.characterRoutingValid
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
