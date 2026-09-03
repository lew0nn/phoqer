#include "Phoqer.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

#include <algorithm>

namespace
{
constexpr int presetCount = 1;

const IColor background { 255, 8, 9, 12 };
const IColor panel { 255, 15, 16, 21 };
const IColor panelRaised { 255, 21, 21, 28 };
const IColor knobFace { 255, 30, 29, 38 };
const IColor border { 255, 48, 43, 58 };
const IColor borderBright { 255, 76, 57, 94 };
const IColor foreground { 255, 226, 223, 232 };
const IColor muted { 255, 132, 124, 145 };
const IColor accent { 255, 139, 43, 232 };
const IColor accentBright { 255, 178, 86, 255 };
const IColor accentDark { 255, 63, 28, 92 };
}

Phoqer::Phoqer(const InstanceInfo& info)
    : iplug::Plugin(info, MakeConfig(phoqer::parameters::count, presetCount))
{
    for (int index = 0; index < phoqer::parameters::character; ++index)
    {
        const auto& parameter = phoqer::parameters::descriptors[static_cast<size_t>(index)];
        GetParam(index)->InitDouble(parameter.name, parameter.defaultValue, parameter.minimum,
                                    parameter.maximum, parameter.step, parameter.unit);
    }
    GetParam(phoqer::parameters::character)->InitEnum(
        "CHARACTER", static_cast<int>(phoqer::defaultSealCharacter),
        { "LOW / BURP", "MAIN / BARK-GROAN", "PAD / MOAN-SHOUT" });

#if IPLUG_EDITOR
    mMakeGraphicsFunc = [&]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                            GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    };

    mLayoutFunc = [&](IGraphics* graphics) {
        graphics->AttachCornerResizer(EUIResizerMode::Scale, false);
        graphics->AttachPanelBackground(background);
        graphics->EnableMouseOver(true);
        constexpr const char* uiFont = "PHOQER-UI";
        if (! graphics->LoadFont(uiFont, ROBOTO_FN))
            graphics->LoadFont(uiFont, "Segoe UI", ETextStyle::Normal);

        const auto bounds = graphics->GetBounds().GetPadded(-20.0f);
        const auto header = bounds.GetFromTop(70.0f);
        const auto content = bounds.GetReducedFromTop(84.0f);
        const IRECT sourcePanel { content.L, content.T, content.L + 574.0f, content.B };
        const IRECT masterPanel { sourcePanel.R + 16.0f, content.T, content.R, content.B };
        const IRECT sourceArea { sourcePanel.L + 18.0f, sourcePanel.T + 46.0f,
                                 sourcePanel.R - 18.0f, sourcePanel.T + 230.0f };
        const IRECT motionArea { sourcePanel.L + 18.0f, sourcePanel.T + 274.0f,
                                 sourcePanel.R - 18.0f, sourcePanel.B - 18.0f };
        const IRECT profileArea { masterPanel.L + 20.0f, masterPanel.T + 244.0f,
                                  masterPanel.R - 20.0f, masterPanel.B - 22.0f };

        graphics->AttachControl(new ILambdaControl(bounds,
            [=](ILambdaControl*, IGraphics& g, IRECT&) {
                g.FillRoundRect(panel, sourcePanel, 7.0f);
                g.DrawRoundRect(border, sourcePanel, 7.0f, nullptr, 1.0f);
                g.FillRoundRect(panel, masterPanel, 7.0f);
                g.DrawRoundRect(border, masterPanel, 7.0f, nullptr, 1.0f);
                g.DrawLine(accent, sourcePanel.L + 18.0f, sourcePanel.T + 36.0f,
                           sourcePanel.R - 18.0f, sourcePanel.T + 36.0f, nullptr, 1.5f);
                g.DrawLine(border, sourcePanel.L + 18.0f, sourcePanel.T + 264.0f,
                           sourcePanel.R - 18.0f, sourcePanel.T + 264.0f, nullptr, 1.0f);
                g.DrawLine(accent, masterPanel.L + 20.0f, masterPanel.T + 36.0f,
                           masterPanel.R - 20.0f, masterPanel.T + 36.0f, nullptr, 1.5f);
                g.FillRoundRect(panelRaised, profileArea, 5.0f);
                g.DrawRoundRect(border, profileArea, 5.0f, nullptr, 1.0f);

                const auto centreY = profileArea.MH() + 8.0f;
                const auto left = profileArea.L + 17.0f;
                const auto right = profileArea.R - 17.0f;
                g.DrawLine(accentDark, left, centreY - 34.0f, right, centreY - 34.0f, nullptr, 2.0f);
                g.DrawLine(accent, left, centreY, right, centreY, nullptr, 3.0f);
                g.DrawLine(accentDark, left, centreY + 34.0f, right, centreY + 34.0f, nullptr, 2.0f);
                for (int index = 0; index < 7; ++index)
                {
                    const auto x = left + (right - left) * static_cast<float>(index) / 6.0f;
                    const auto height = index % 2 == 0 ? 16.0f : 28.0f;
                    g.DrawLine(accentBright, x, centreY - height, x, centreY + height,
                               nullptr, 1.5f);
                }
            }, 0, false, false, kNoParameter, true));

        graphics->AttachControl(new ITextControl(
            header.GetFromTop(44.0f), "PHOQER",
            IText(32.0f, foreground, uiFont, EAlign::Near, EVAlign::Middle)));
        graphics->AttachControl(new ITextControl(
            header.GetFromBottom(24.0f), "REFERENCE CALL SYNTHESIS",
            IText(12.0f, muted, uiFont, EAlign::Near, EVAlign::Middle)));
        graphics->AttachControl(new ITextControl(
            IRECT(sourcePanel.L + 18.0f, sourcePanel.T + 8.0f,
                  sourcePanel.R - 18.0f, sourcePanel.T + 32.0f), "SOURCE",
            IText(12.0f, foreground, uiFont, EAlign::Near, EVAlign::Middle)));
        graphics->AttachControl(new ITextControl(
            IRECT(sourcePanel.L + 18.0f, sourcePanel.T + 236.0f,
                  sourcePanel.R - 18.0f, sourcePanel.T + 258.0f), "MOTION",
            IText(12.0f, foreground, uiFont, EAlign::Near, EVAlign::Middle)));
        graphics->AttachControl(new ITextControl(
            IRECT(masterPanel.L + 20.0f, masterPanel.T + 8.0f,
                  masterPanel.R - 20.0f, masterPanel.T + 32.0f), "MASTER",
            IText(12.0f, foreground, uiFont, EAlign::Near, EVAlign::Middle)));
        graphics->AttachControl(new ITextControl(
            IRECT(profileArea.L + 12.0f, profileArea.T + 8.0f,
                  profileArea.R - 12.0f, profileArea.T + 28.0f), "CALL PROFILE",
            IText(10.0f, muted, uiFont, EAlign::Near, EVAlign::Middle)));

        auto style = DEFAULT_STYLE
            .WithColor(kBG, panelRaised)
            .WithColor(kFG, knobFace)
            .WithColor(kPR, accent)
            .WithColor(kFR, borderBright)
            .WithColor(kHL, accentBright)
            .WithColor(kSH, background)
            .WithDrawShadows(false)
            .WithRoundness(1.0f)
            .WithFrameThickness(1.5f)
            .WithWidgetFrac(0.70f)
            .WithLabelText(IText(12.0f, foreground, uiFont, EAlign::Center, EVAlign::Top))
            .WithValueText(IText(11.0f, muted, uiFont, EAlign::Center, EVAlign::Bottom));

        const IRECT characterLabelArea { bounds.R - 570.0f, header.T + 2.0f,
                                         bounds.R, header.T + 18.0f };
        const IRECT characterSelectorArea { bounds.R - 570.0f, header.T + 23.0f,
                                            bounds.R, header.B - 5.0f };
        graphics->AttachControl(new ITextControl(
            characterLabelArea, "CHARACTER",
            IText(9.0f, muted, uiFont, EAlign::Near, EVAlign::Middle)));
        const auto characterStyle = style
            .WithColor(kBG, background)
            .WithColor(kFG, panelRaised)
            .WithColor(kPR, accent)
            .WithColor(kFR, borderBright)
            .WithColor(kHL, accentBright)
            .WithFrameThickness(1.0f)
            .WithRoundness(0.18f)
            .WithShowLabel(false)
            .WithValueText(IText(9.0f, foreground, uiFont, EAlign::Center, EVAlign::Middle));
        graphics->AttachControl(new IVTabSwitchControl(
            characterSelectorArea, phoqer::parameters::character,
            { "LOW / BURP", "MAIN / BARK-GROAN", "PAD / MOAN-SHOUT" },
            "", characterStyle, EVShape::EndsRounded));

        for (int index = 0; index < 4; ++index)
        {
            const auto cell = sourceArea.GetGridCell(index, 1, 4).GetPadded(-8.0f);
            graphics->AttachControl(new IVKnobControl(
                cell.GetCentredInside(118.0f), index,
                phoqer::parameters::descriptors[static_cast<size_t>(index)].name, style));
        }

        for (int index = 0; index < 3; ++index)
        {
            const auto parameter = phoqer::parameters::space + index;
            const auto cell = motionArea.GetGridCell(index, 1, 3).GetPadded(-10.0f);
            graphics->AttachControl(new IVKnobControl(
                cell.GetCentredInside(102.0f), parameter,
                phoqer::parameters::descriptors[static_cast<size_t>(parameter)].name, style));
        }

        const IRECT outputArea { masterPanel.L + 34.0f, masterPanel.T + 58.0f,
                                 masterPanel.R - 34.0f, masterPanel.T + 222.0f };
        graphics->AttachControl(new IVKnobControl(
            outputArea.GetCentredInside(136.0f), phoqer::parameters::output,
            phoqer::parameters::descriptors[static_cast<size_t>(phoqer::parameters::output)].name,
            style.WithWidgetFrac(0.72f)));
    };
#endif
}

#if IPLUG_DSP
void Phoqer::queueMidiEvent(const phoqer::MidiEvent& event) noexcept
{
    if (midiEventCount < static_cast<int>(midiEvents.size()))
        midiEvents[static_cast<size_t>(midiEventCount++)] = event;
}

void Phoqer::ProcessMidiMsg(const IMidiMsg& msg)
{
    const auto channel = msg.Channel();
    const auto offset = std::max(0, msg.mOffset);
    switch (msg.StatusMsg())
    {
        case IMidiMsg::kNoteOn:
            queueMidiEvent({ msg.Velocity() > 0 ? phoqer::MidiEventType::noteOn
                                                : phoqer::MidiEventType::noteOff,
                             offset, channel, msg.NoteNumber(), msg.Velocity() / 127.0f });
            break;
        case IMidiMsg::kNoteOff:
            queueMidiEvent({ phoqer::MidiEventType::noteOff, offset, channel,
                             msg.NoteNumber(), msg.Velocity() / 127.0f });
            break;
        case IMidiMsg::kPitchWheel:
            queueMidiEvent({ phoqer::MidiEventType::pitchWheel, offset, channel, 0,
                             static_cast<float>(msg.PitchWheel()) });
            break;
        case IMidiMsg::kControlChange:
            if (msg.ControlChangeIdx() == IMidiMsg::kAllNotesOff)
                queueMidiEvent({ phoqer::MidiEventType::allNotesOff, offset, channel, 0, 0.0f });
            break;
        default:
            break;
    }
}

void Phoqer::OnReset()
{
    midiEventCount = 0;
    engine.prepare(GetSampleRate(), GetBlockSize(), std::max(1, NOutChansConnected()));
}

void Phoqer::ProcessBlock(sample**, sample** outputs, int nFrames)
{
    const phoqer::MacroState macros {
        static_cast<float>(GetParam(phoqer::parameters::boom)->Value()),
        static_cast<float>(GetParam(phoqer::parameters::air)->Value()),
        static_cast<float>(GetParam(phoqer::parameters::bark)->Value()),
        static_cast<float>(GetParam(phoqer::parameters::vowel)->Value()),
        static_cast<float>(GetParam(phoqer::parameters::space)->Value()),
        static_cast<float>(GetParam(phoqer::parameters::tide)->Value()),
        static_cast<float>(GetParam(phoqer::parameters::detune)->Value()),
        static_cast<phoqer::SealCharacter>(GetParam(phoqer::parameters::character)->Int())
    };
    phoqer::AudioBuffer buffer(reinterpret_cast<float**>(outputs),
                               std::max(1, NOutChansConnected()), nFrames);
    engine.process(buffer, midiEvents.data(), midiEventCount, macros,
                   static_cast<float>(GetParam(phoqer::parameters::output)->Value()));
    midiEventCount = 0;
}
#endif
