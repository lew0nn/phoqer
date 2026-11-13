#pragma once
#include <JuceHeader.h>
#include "Core/CharacterPreset.h"
#include <cmath>
namespace phoqer {
struct VoiceData {
    CharacterPreset preset;
    juce::String id, name, description, hash, json;   // json: the exact text, saved with songs as their snapshot
    static bool read(const juce::File& file, VoiceData& result, juce::String& error) {
        if (!file.existsAsFile() || file.getSize() > 65536 || file.getSize() < 64) { error="Voice file missing or too large."; return false; }
        return parse(file.loadFileAsString(),result,error);
    }
    static bool parse(const juce::String& text, VoiceData& result, juce::String& error) {
        if (text.getNumBytesAsUTF8() > 65536 || text.length() < 64) { error="Voice data missing or too large."; return false; }
        const auto root=juce::JSON::parse(text);
        if (!root.isObject() || root["format"].toString()!="PHQVOICE" || static_cast<int>(root["version"])!=1) {error="Unsupported voice file.";return false;}
        VoiceData data; data.id=root["id"].toString();data.name=root["name"].toString();data.description=root["description"].toString();data.json=text;
        if(data.id.isEmpty() || data.id.length()>64 || data.id.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789.-_")!=data.id || data.name.isEmpty() || data.name.length()>24) {error="Invalid voice identity.";return false;}
        auto& p=data.preset; p.id="loaded"; p.displayName="LOADED VOICE";
        // Settings from older voice files that no current voice uses: still checked, so a damaged file is
        // still refused, then ignored. Every .phqvoice made so far loads unchanged.
        float ignored=0.0f;
        const auto values=root["parameters"]; if(!values.isObject()){error="Voice parameters missing.";return false;}
        auto number=[&](const juce::var& value,float& out,float low,float high){if(!(value.isDouble()||value.isInt()||value.isInt64()))return false;const auto n=static_cast<double>(value);if(!std::isfinite(n)||n<low||n>high)return false;out=static_cast<float>(n);return true;};
        if(!number(values["attackSkirtSeconds"],p.attackSkirtSeconds,0.0001f,0.02f)){error="Invalid attackSkirtSeconds.";return false;}
        if(!number(values["noiseMix"],p.noiseMix,0.0f,1.0f)){error="Invalid noiseMix.";return false;}
        if(!number(values["jitter"],p.jitter,0.0f,1.0f)){error="Invalid jitter.";return false;}
        if(!number(values["shimmer"],p.shimmer,0.0f,1.0f)){error="Invalid shimmer.";return false;}
        if(!number(values["subharmonicChance"],p.subharmonicChance,0.0f,1.0f)){error="Invalid subharmonicChance.";return false;}
        if(!number(values["subOctaveGain"],p.subOctaveGain,0.0f,1.0f)){error="Invalid subOctaveGain.";return false;}
        if(!number(values["attackSeconds"],p.attackSeconds,0.001f,5.0f)){error="Invalid attackSeconds.";return false;}
        if(!number(values["decaySeconds"],p.decaySeconds,0.001f,5.0f)){error="Invalid decaySeconds.";return false;}
        if(!number(values["sustainLevel"],p.sustainLevel,0.0f,1.0f)){error="Invalid sustainLevel.";return false;}
        if(!number(values["releaseSeconds"],p.releaseSeconds,0.005f,5.0f)){error="Invalid releaseSeconds.";return false;}
        if(!number(values["outputTrim"],p.outputTrim,0.01f,4.0f)){error="Invalid outputTrim.";return false;}
        if(!number(values["gestureSeconds"],p.gestureSeconds,0.001f,10.0f)){error="Invalid gestureSeconds.";return false;}
        if(!number(values["gestureDepth"],p.gestureDepth,0.0f,4.0f)){error="Invalid gestureDepth.";return false;}
        if(!number(values["wobbleSemitones"],p.wobbleSemitones,0.0f,4.0f)){error="Invalid wobbleSemitones.";return false;}
        if(!number(values["wobbleRateHz"],p.wobbleRateHz,0.1f,40.0f)){error="Invalid wobbleRateHz.";return false;}
        if(!number(values["flutterSemitones"],p.flutterSemitones,0.0f,4.0f)){error="Invalid flutterSemitones.";return false;}
        if(!number(values["flutterRateHz"],p.flutterRateHz,0.1f,40.0f)){error="Invalid flutterRateHz.";return false;}
        if(!number(values["squeakNoise"],p.squeakNoise,0.0f,1.0f)){error="Invalid squeakNoise.";return false;}
        if(!number(values["onsetPunch"],p.onsetPunch,0.0f,1.0f)){error="Invalid onsetPunch.";return false;}
        if(!number(values["thump"],p.thump,0.0f,1.0f)){error="Invalid thump.";return false;}
        if(!number(values["vowelOffset"],p.vowelOffset,-1.0f,1.0f)){error="Invalid vowelOffset.";return false;}
        if(!number(values["cycleRateHz"],ignored,0.0f,30.0f)){error="Invalid cycleRateHz.";return false;}
        if(!number(values["cyclicPitchSemitones"],ignored,0.0f,36.0f)){error="Invalid cyclicPitchSemitones.";return false;}
        if(!number(values["vowelCycleDepth"],ignored,0.0f,1.0f)){error="Invalid vowelCycleDepth.";return false;}
        if(!number(values["amplitudeCycleDepth"],ignored,0.0f,1.0f)){error="Invalid amplitudeCycleDepth.";return false;}
        if(!number(values["spectralTilt"],ignored,0.0f,4.0f)){error="Invalid spectralTilt.";return false;}
        if(!number(values["spectralRoughness"],ignored,0.0f,2.0f)){error="Invalid spectralRoughness.";return false;}
        if(!number(values["timbreSeconds"],ignored,0.0f,10.0f)){error="Invalid timbreSeconds.";return false;}
        if(!number(values["bassGain"],p.bassGain,0.0f,2.0f)){error="Invalid bassGain.";return false;}
        if(!number(values["bassDrive"],p.bassDrive,0.0f,4.0f)){error="Invalid bassDrive.";return false;}
        if(!number(values["bassDecaySeconds"],p.bassDecaySeconds,0.001f,5.0f)){error="Invalid bassDecaySeconds.";return false;}
        if(!number(values["bassSustain"],p.bassSustain,0.0f,1.0f)){error="Invalid bassSustain.";return false;}
        if(!number(values["spectralBodyGain"],p.spectralBodyGain,0.0f,4.0f)){error="Invalid spectralBodyGain.";return false;}
        if(!number(values["transientBite"],ignored,0.0f,4.0f)){error="Invalid transientBite.";return false;}
        if(!number(values["articulationSeconds"],ignored,0.0f,10.0f)){error="Invalid articulationSeconds.";return false;}
        if(!number(values["mouthMotionDepth"],ignored,0.0f,1.0f)){error="Invalid mouthMotionDepth.";return false;}
        if(!number(values["textureMotionDepth"],ignored,0.0f,1.0f)){error="Invalid textureMotionDepth.";return false;}
        if(!number(values["motionRateHz"],ignored,0.1f,20.0f)){error="Invalid motionRateHz.";return false;}
        if(!number(values["tractMotionSemitones"],ignored,0.0f,12.0f)){error="Invalid tractMotionSemitones.";return false;}
        if(!values["spectralSource"].isBool()){error="Invalid spectralSource.";return false;}
        if(!values["fullVowelMap"].isBool()){error="Invalid fullVowelMap.";return false;}p.fullVowelMap=static_cast<bool>(values["fullVowelMap"]);
        auto contour=[&](const char* key,PitchContour& dest,float low,float high){auto* a=values[key].getArray();if(a==nullptr||a->size()!=8)return false;for(int i=0;i<8;++i)if(!number(a->getReference(i),dest[static_cast<size_t>(i)],low,high))return false;return true;};
        auto band=[&](const juce::var& value,FormantSpec& b){auto* a=value.getArray();return a!=nullptr&&a->size()==4&&number((*a)[0],b.frequency,40,12000)&&number((*a)[1],b.bandwidth,10,2500)&&number((*a)[2],b.gain,0,4)&&number((*a)[3],b.harmonicLock,0,1);};
        PitchContour ignoredContour{};
        if(!contour("pitchContour",p.pitchContour,-48,48)||!contour("timbreContour",ignoredContour,-1,1)||!contour("amplitudeContour",ignoredContour,0,2)){error="Invalid contour.";return false;}
        auto* formants=values["formants"].getArray();if(formants==nullptr||formants->size()!=5){error="Expected five formants.";return false;}
        for(int b=0;b<5;++b)if(!band((*formants)[b],p.formants[static_cast<size_t>(b)])){error="Invalid formant.";return false;}
        auto* map=values["articulationMap"].getArray();if(map==nullptr||map->size()!=9){error="Expected nine vowels.";return false;}
        for(int v=0;v<9;++v){auto* row=(*map)[v].getArray();if(row==nullptr||row->size()!=5){error="Invalid vowel map.";return false;}for(int b=0;b<5;++b)if(!band((*row)[b],p.articulationMap[static_cast<size_t>(v)][static_cast<size_t>(b)])){error="Invalid vowel formant.";return false;}}
        data.hash=juce::SHA256(text.toRawUTF8(),static_cast<size_t>(text.getNumBytesAsUTF8())).toHexString();result=std::move(data);error.clear();return true;
    }
};
}
