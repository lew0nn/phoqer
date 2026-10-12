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

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace phoqer::ui
{
namespace
{
#if JUCE_WINDOWS
// Windows 11 rounds frameless top-level windows and supplies its own dark perimeter unless asked
// otherwise.  Keep the native peer square and neutral so PHOQER's drawn Win98 bevel is the frame.
void applyNativeWin98Frame(juce::Component& component)
{
    auto* peer = component.getPeer();
    if (peer == nullptr) return;
    auto* window = static_cast<HWND>(peer->getNativeHandle());
    if (window == nullptr) return;

    using SetAttribute = HRESULT (WINAPI*) (HWND, DWORD, LPCVOID, DWORD);
    if (auto* library = ::LoadLibraryW(L"dwmapi.dll"))
    {
        if (auto* setAttribute = reinterpret_cast<SetAttribute>(::GetProcAddress(library, "DwmSetWindowAttribute")))
        {
            constexpr DWORD cornerPreferenceAttribute = 33;
            constexpr DWORD borderColourAttribute = 34;
            constexpr int doNotRound = 1;
            const COLORREF borderColour = RGB(192, 192, 192);
            setAttribute(window, cornerPreferenceAttribute, &doNotRound, sizeof(doNotRound));
            setAttribute(window, borderColourAttribute, &borderColour, sizeof(borderColour));
        }
        ::FreeLibrary(library);
    }
}
#else
void applyNativeWin98Frame(juce::Component&) {}
#endif

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
        applyNativeWin98Frame(*this);
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

// Standalone-only startup: a compact Win98 installer wakes each voice while audio starts.
class BootSplash final : public juce::Component, private juce::Timer
{
public:
    static constexpr double seconds = 1.5;

    BootSplash()
    {
        setOpaque(true);
        setSize(bootSplashWidth, bootSplashHeight);
        addToDesktop(juce::ComponentPeer::windowHasDropShadow);
        applyNativeWin98Frame(*this);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        started = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        paintStandaloneBootSplash(g, getLocalBounds().toFloat(),
                                  static_cast<float>(juce::jlimit(0.0, 1.0, elapsed() / seconds)));
    }

private:
    double elapsed() const { return (juce::Time::getMillisecondCounterHiRes() - started) * 0.001; }
    void timerCallback() override { repaint(); }

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
