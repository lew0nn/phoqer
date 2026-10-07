// Standalone app for PHOQER: a frameless window whose only chrome is the editor's own
// PHOQER.EXE title bar (no OS title bar, no JUCE "Options" bar). Audio and MIDI still run through
// JUCE's StandalonePluginHolder; its settings dialog lives under EDIT > AUDIO/MIDI SETUP. A short
// boot splash shows first; the window follows the editor's zoom and can start in full screen.
// Enabled by JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP on the PHOQER target.

#include <JuceHeader.h>

#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "Editor.h"
#include "Logo.h"
#include "Settings.h"
#include "Style.h"

#include <PhoqerUIAssets.h>

namespace phoqer::ui
{
namespace
{
class FramelessWindow final : public juce::Component, private juce::ComponentListener
{
public:
    explicit FramelessWindow(juce::StandalonePluginHolder& h) : holder(h)
    {
        setOpaque(true);
        setName(JucePlugin_Name);
        editor.reset(holder.processor->createEditorIfNeeded());
        content.addAndMakeVisible(*editor);
        content.setSize(editor->getWidth(), editor->getHeight());
        addAndMakeVisible(content);

        if (auto* testEditor = dynamic_cast<PhoqerEditor*>(editor.get()))
        {
            WindowControls controls;
            controls.minimise = [this] { if (auto* peer = getPeer()) peer->setMinimised(true); };
            controls.toggleFullscreen = [this] { setFullscreen(! fullscreen); };
            controls.close = [] { if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); };
            controls.audioSettings = [this] { holder.showAudioSettingsDialog(); };
            controls.startDrag = [this](const juce::MouseEvent& e) { if (! fullscreen) dragger.startDraggingComponent(this, e.getEventRelativeTo(this)); };
            controls.drag = [this](const juce::MouseEvent& e) { if (! fullscreen) dragger.dragComponent(this, e.getEventRelativeTo(this), nullptr); };
            testEditor->setWindowControls(std::move(controls));
        }

        setSize(editor->getWidth(), editor->getHeight());
        addToDesktop(juce::ComponentPeer::windowAppearsOnTaskbar | juce::ComponentPeer::windowHasDropShadow
                     | juce::ComponentPeer::windowHasMinimiseButton);
        centreWithSize(getWidth(), getHeight());
        editor->addComponentListener(this);
        setVisible(true);
        toFront(true);
        if (UiSettings::get().startFullscreen()) setFullscreen(true);
    }

    ~FramelessWindow() override
    {
        editor->removeComponentListener(this);
        if (fullscreen) juce::Desktop::getInstance().setKioskModeComponent(nullptr, false);
        holder.processor->editorBeingDeleted(editor.get());
        editor = nullptr;
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }    // letterbox when fullscreen

    // The editor has a fixed layout; fullscreen scales its holder (not the editor itself, whose
    // transform belongs to the host) to fit, centred.
    void resized() override
    {
        const float w = static_cast<float>(content.getWidth()), h = static_cast<float>(content.getHeight());
        if (w <= 0.0f || h <= 0.0f) return;
        const float s = juce::jmin(static_cast<float>(getWidth()) / w, static_cast<float>(getHeight()) / h);
        content.setTransform(juce::AffineTransform::scale(s).translated((static_cast<float>(getWidth()) - w * s) * 0.5f,
                                                                        (static_cast<float>(getHeight()) - h * s) * 0.5f));
    }

private:
    // A new zoom (VIEW menu or the size grip) resizes the window, kept on its screen. In full screen
    // the window stays put and the bigger or smaller editor is fitted again.
    void componentMovedOrResized(juce::Component&, bool, bool wasResized) override
    {
        if (! wasResized) return;
        content.setSize(editor->getWidth(), editor->getHeight());
        if (fullscreen) { resized(); return; }
        auto bounds = getBounds().withSize(editor->getWidth(), editor->getHeight());
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
            if (bounds.getWidth() <= display->userArea.getWidth() && bounds.getHeight() <= display->userArea.getHeight())
                bounds = bounds.constrainedWithin(display->userArea);
        setBounds(bounds);
    }

    void setFullscreen(bool shouldBeFullscreen)
    {
        if (shouldBeFullscreen == fullscreen) return;
        fullscreen = shouldBeFullscreen;
        if (fullscreen)
        {
            windowedBounds = getBounds();
            juce::Desktop::getInstance().setKioskModeComponent(this, false);
        }
        else
        {
            juce::Desktop::getInstance().setKioskModeComponent(nullptr, false);
            setBounds(windowedBounds.withSize(editor->getWidth(), editor->getHeight()));
        }
    }

    juce::StandalonePluginHolder& holder;
    juce::Component content;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::ComponentDragger dragger;
    juce::Rectangle<int> windowedBounds;
    bool fullscreen = false;
};

// Boot splash (an experiment): the big app icon, the wordmark and a Win98 loading bar for a moment
// while audio starts.
class BootSplash final : public juce::Component, private juce::Timer
{
public:
    static constexpr double seconds = 1.5;

    BootSplash()
    {
        setOpaque(true);
        for (int i = 0; i < PhoqerUIAssets::namedResourceListSize; ++i)
        {
            const auto* name = PhoqerUIAssets::namedResourceList[i];
            if (juce::String(PhoqerUIAssets::getNamedResourceOriginalFilename(name)) == "phoqer-app-icon-256.png")
            {
                int size = 0;
                const auto* data = PhoqerUIAssets::getNamedResource(name, size);
                icon = juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(size));
            }
        }
        word = renderOutrunWordmark(0, true);
        setSize(440, 320);
        addToDesktop(juce::ComponentPeer::windowHasDropShadow);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        started = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& pal = paletteFor(0);
        const auto r = getLocalBounds().toFloat();
        g.fillAll(win98::face);
        bevel(g, r, true);
        const auto art = r.reduced(4.0f).withTrimmedBottom(58.0f);
        g.setColour(pal.sky);
        g.fillRect(art);
        juce::Random stars(7);
        for (int k = 0; k < 40; ++k)
        {
            g.setColour(juce::Colours::white.withAlpha(0.2f + 0.5f * stars.nextFloat()));
            g.fillRect(art.getX() + std::round(stars.nextFloat() * art.getWidth() / 2.0f) * 2.0f,
                       art.getY() + std::round(stars.nextFloat() * art.getHeight() / 2.0f) * 2.0f, 2.0f, 2.0f);
        }
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.setOpacity(1.0f);
        const juce::Rectangle<float> iconArea { art.getX() + 24.0f, art.getCentreY() - 64.0f, 128.0f, 128.0f };
        if (icon.isValid()) g.drawImage(icon, iconArea);
        const float wordW = static_cast<float>(word.getWidth()) * 2.0f, wordH = static_cast<float>(word.getHeight()) * 2.0f;
        const float textX = iconArea.getRight() + 20.0f;
        g.drawImage(word, { textX, art.getCentreY() - wordH - 4.0f, wordW, wordH });
        drawText(g, "VERSION " JucePlugin_VersionString, { textX, art.getCentreY() + 8.0f, art.getRight() - textX, 16.0f },
                 pixelFont(12.0f), pal.secondary);

        // Loading bar: Win98 blocks in the accent colour.
        const float progress = static_cast<float>(juce::jlimit(0.0, 1.0, elapsed() / seconds));
        const juce::Rectangle<float> bar { r.getX() + 16.0f, art.getBottom() + 12.0f, r.getWidth() - 32.0f, 18.0f };
        sunken(g, bar);
        const auto inner = bar.reduced(3.0f);
        const int blocks = static_cast<int>(inner.getWidth() / 10.0f), lit = juce::roundToInt(progress * static_cast<float>(blocks));
        g.setColour(pal.accent.darker(0.35f));
        for (int b = 0; b < lit; ++b) g.fillRect(inner.getX() + static_cast<float>(b) * 10.0f, inner.getY(), 8.0f, inner.getHeight());
        drawText(g, "WAKING UP THE SEALS...", { bar.getX(), bar.getBottom() + 4.0f, bar.getWidth(), 16.0f }, pixelFont(12.0f), juce::Colours::black);
    }

private:
    double elapsed() const { return (juce::Time::getMillisecondCounterHiRes() - started) * 0.001; }
    void timerCallback() override { repaint(); }

    juce::Image icon, word;
    double started = 0.0;
};

class StandaloneApp final : public juce::JUCEApplication
{
public:
    StandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = JucePlugin_Name;
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #endif
        properties.setStorageParameters(options);
    }

    const juce::String getApplicationName() override { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String&) override {}

    void initialise(const juce::String&) override
    {
        // Connected MIDI keyboards open straight away; there is no Options bar to enable them from.
        holder = std::make_unique<juce::StandalonePluginHolder>(properties.getUserSettings(), false, juce::String(), nullptr,
                                                                juce::Array<juce::StandalonePluginHolder::PluginInOuts>(), true);
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty()) return;
        splash = std::make_unique<BootSplash>();
        juce::Timer::callAfterDelay(juce::roundToInt(BootSplash::seconds * 1000.0) + 100, [this]
        {
            if (holder == nullptr || window != nullptr) return;
            window = std::make_unique<FramelessWindow>(*holder);
            splash = nullptr;
        });
    }

    void shutdown() override
    {
        splash = nullptr;
        window = nullptr;
        holder = nullptr;
        properties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (holder != nullptr) holder->savePluginState();
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay(100, [] { if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
        else
            quit();
    }

private:
    juce::ApplicationProperties properties;
    std::unique_ptr<juce::StandalonePluginHolder> holder;
    std::unique_ptr<BootSplash> splash;
    std::unique_ptr<FramelessWindow> window;
};
}
}

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new phoqer::ui::StandaloneApp(); }

#endif
