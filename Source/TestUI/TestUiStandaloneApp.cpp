// Standalone app for the PHOQER Test UI: a frameless window whose only chrome is the editor's own
// PHOQER.EXE title bar (no OS title bar, no JUCE "Options" bar). Audio and MIDI still run through
// JUCE's StandalonePluginHolder; its settings dialog lives under File > Audio/MIDI Settings.
// Enabled by JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP on the PHOQER_TestUI target.

#include <JuceHeader.h>

#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "TestUiEditor.h"

namespace phoqer::testui
{
namespace
{
class FramelessWindow final : public juce::Component
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

        if (auto* testEditor = dynamic_cast<TestUiEditor*>(editor.get()))
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
        setVisible(true);
        toFront(true);
    }

    ~FramelessWindow() override
    {
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
            setBounds(windowedBounds);
        }
    }

    juce::StandalonePluginHolder& holder;
    juce::Component content;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::ComponentDragger dragger;
    juce::Rectangle<int> windowedBounds;
    bool fullscreen = false;
};

class TestUiStandaloneApp final : public juce::JUCEApplication
{
public:
    TestUiStandaloneApp()
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
        if (! juce::Desktop::getInstance().getDisplays().displays.isEmpty())
            window = std::make_unique<FramelessWindow>(*holder);
    }

    void shutdown() override
    {
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
    std::unique_ptr<FramelessWindow> window;
};
}
}

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new phoqer::testui::TestUiStandaloneApp(); }

#endif
