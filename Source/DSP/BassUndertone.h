#pragma once
#include "../Core/CharacterPreset.h"
#include "../Core/DspPrimitives.h"
#include <array>
#include <cmath>

namespace phoqer
{
// Optional phase-locked octave bass under the FOF source, built from a fixed
// harmonic recipe. No samples or waveshaper.
class BassUndertone
{
public:
    void prepare(double rate) noexcept { sampleRate=rate;reset(); }
    void reset() noexcept { phase=0;decay=1;cachedSeconds=-1;cachedDrive=-1; }
    float process(double hz,const CharacterPreset& p) noexcept
    {
        if(p.bassDecaySeconds!=cachedSeconds)
        {
            cachedSeconds=p.bassDecaySeconds;
            coefficient=std::exp(-1.0/(sampleRate*std::max(.005f,cachedSeconds)));
        }
        if(p.bassDrive!=cachedDrive)
        {
            cachedDrive=p.bassDrive;
            const auto d=clamp(0.0f,1.0f,cachedDrive);
            harmonics={{1.0f,-.10f+.44f*d,.04f+.18f*d,-.020f+.16f*d,.012f+.10f*d}};
        }
        phase+=hz*.5/sampleRate;phase-=std::floor(phase);
        decay*=coefficient;
        const auto s=fastSin2Pi(phase),c=2.0f*fastSin2Pi(phase+.25);
        float previous=0,current=s,value=0;
        for(size_t i=0;i<9;++i)
        {
            if((i&1)==0)
            {
                const auto f=hz*.5*static_cast<double>(i+1);
                const auto safe=clamp(0.0f,1.0f,static_cast<float>((sampleRate*.42-f)/(sampleRate*.08)));
                value+=current*harmonics[i/2]*safe;
            }
            const auto next=c*current-previous;previous=current;current=next;
        }
        return value*p.bassGain*(p.bassSustain+(1.0f-p.bassSustain)*static_cast<float>(decay));
    }
private:
    double sampleRate=44100,phase=0,decay=1,coefficient=1;
    float cachedSeconds=-1,cachedDrive=-1;
    std::array<float,5> harmonics{};
};
}
