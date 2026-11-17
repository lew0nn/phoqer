// UPDATE.EXE: the notice in SCOPE.EXE's status bar, the prompt, the download (pages flying from lewonn.com into
// the PHOQER disk), the done box and the error boxes. Approved mockups: scratchpad feat/out18 (U4 COPY, the
// voice-gradient disk, the W3+W4 done text) and feat/out18/update-rest.png.
#include "Editor.h"
#include "EditorShared.h"
#include "Settings.h"
#include "../Licence.h"

namespace phoqer::ui
{
namespace
{
updates::Found lastFound;     // the newest check this run, so every open PHOQER window shows the notice
bool installScheduled = false;

constexpr float updateW = 274.0f, updateH = 170.0f, edge = 20.0f;
constexpr double minDownloadMs = 2000.0;
const char* const globe[] { "....####....", "..##.##.##..", ".#..#..#..#.", ".##########.", "#..#....#..#", "############", "#..#....#..#",
                            ".##########.", ".#..#..#..#.", "..##.##.##..", "....####...." };
const char* const page[] { "#####   ", "#...##  ", "#...#.# ", "#...####", "#......#", "#.####.#", "#......#", "#.###..#", "#......#", "########" };
const char* const disk[] { "kkkkkkkkkkk.", "kkmmmmmmmkkk", "kkmmmmmsmkkk", "kkmmmmmsmkkk", "kkmmmmmmmkkk", "kkkkkkkkkkkk", "kwwwwwwwwwwk",
                           "kwaaaaaaaawk", "kwwwwwwwwwwk", "kwbbbbbbwwwk", "kwwwwwwwwwwk", "kkkkkkkkkkkk" };

template <typename ColourFor>
void sprite(juce::Graphics& g, const char* const* rows, int count, float x, float y, float cell, ColourFor colourFor)
{
    for (int j = 0; j < count; ++j)
        for (int i = 0; rows[j][i] != 0; ++i)
        {
            const auto c = colourFor(rows[j][i], j);
            if (c.isTransparent()) continue;
            g.setColour(c);
            g.fillRect(x + static_cast<float>(i) * cell, y + static_cast<float>(j) * cell, cell, cell);
        }
}

float textWidth(const juce::String& s, const juce::Font& f) { return juce::GlyphArrangement::getStringWidth(f, s); }
}

void PhoqerView::setupUpdates()
{
    updateWindow.setComponentID("updateWindow");
    updateLeft.setComponentID("updateLeft");
    updateRight.setComponentID("updateRight");
    updateRight.primary = true;
    updateLeft.onClick = [this] { updatePressed(false); };
    updateRight.onClick = [this] { updatePressed(true); };
    updateWindow.addAndMakeVisible(updateLeft);
    updateWindow.addAndMakeVisible(updateRight);
    updateWindow.paintContent = [this](juce::Graphics& g, juce::Rectangle<float>) { paintUpdate(g); };
    updateWindow.onClose = [this] { if (updateStage != UpdateStage::downloading) updateStage = UpdateStage::none; };
    addChildComponent(updateWindow);
    // Remember where this copy lives (the app's folder, or the folder holding PHOQER.vst3), for updates.
    const auto self = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        UiSettings::get().setAppFolder(self.getParentDirectory().getFullPathName());
    else if (const auto bundle = self.getParentDirectory().getParentDirectory().getParentDirectory(); bundle.hasFileExtension("vst3") && bundle.isDirectory())
        UiSettings::get().setVst3Folder(bundle.getParentDirectory().getFullPathName());
    // Once a day (or every start with a test server), quietly; the notice appears if there's something new.
    const bool testServer = juce::SystemStats::getEnvironmentVariable("PHOQER_UPDATE_URL", {}).isNotEmpty();
    const auto now = juce::Time::currentTimeMillis() / 1000;
    if (lastFound.reached) updateInfo = lastFound;
    else if (UiSettings::get().checkUpdatesDaily() && (testServer || now - UiSettings::get().lastUpdateCheck() >= 86400))
        checkForUpdates(false);
}

void PhoqerView::checkForUpdates(bool manual)
{
    if (checkingUpdates) return;
    checkingUpdates = true;
    const bool unlocked = licence::status().unlocked;
    processor.getVoices().online().run([safe = juce::Component::SafePointer<PhoqerView>(this), manual, unlocked]
    {
        const auto found = updates::check(unlocked);
        juce::MessageManager::callAsync([safe, found, manual] { if (safe != nullptr) safe->updateFound(found, manual); });
    });
}

void PhoqerView::updateFound(const updates::Found& found, bool manual)
{
    checkingUpdates = false;
    if (found.reached)
    {
        lastFound = found;
        updateInfo = found;
        UiSettings::get().setLastUpdateCheck(juce::Time::currentTimeMillis() / 1000);
    }
    repaint();
    if (! manual) return;
    showUpdate(! found.reached ? UpdateStage::offline : found.phoqerNewer ? UpdateStage::prompt
               : found.kyraxNewer ? UpdateStage::kyrax : UpdateStage::upToDate);
}

juce::String PhoqerView::updateNotice() const
{
    const auto& f = updateInfo;
    if (installScheduled) return f.phoqer.version + " ON DAW EXIT";
    if (f.phoqerNewer && f.kyraxNewer) return "2 UPDATES READY";
    if (f.phoqerNewer) return "UPDATE " + f.phoqer.version + " READY";
    if (f.kyraxNewer) return "KYRAX UPDATE READY";
    return {};
}

juce::Rectangle<float> PhoqerView::noticeArea() const
{
    const auto client = windowClient(scopeWindow);
    const auto bar = client.withTrimmedTop(client.getHeight() - 15.0f);
    const float each = bar.getWidth() / 3.0f;
    return { bar.getX() + 2.0f * each, bar.getY(), each - 3.0f, bar.getHeight() };
}

void PhoqerView::openUpdateFromNotice()
{
    if (installScheduled) showUpdate(UpdateStage::ready);
    else if (updateInfo.phoqerNewer) showUpdate(updateFile.existsAsFile() ? UpdateStage::ready : UpdateStage::prompt);
    else if (updateInfo.kyraxNewer) showUpdate(UpdateStage::kyrax);
}

void PhoqerView::showUpdate(UpdateStage stage, const updates::Found& found, float progressForChecks)
{
    if (found.reached) updateInfo = found;
    updateTestProgress = progressForChecks;
    updateStage = stage;
    layoutUpdate();
    updateWindow.setVisible(stage != UpdateStage::none);
    updateWindow.toFront(false);
    updateWindow.repaint();
}

void PhoqerView::layoutUpdate()
{
    if (updateWindow.getWidth() == 0)
        updateWindow.setBounds(juce::Rectangle<float>((static_cast<float>(width) - updateW) * 0.5f, 230.0f, updateW, updateH).toNearestInt());
    updateWindow.setCharacter(character);
    struct Pair { const char* left; const char* right; float lw, rw; };
    const bool standalone = isStandalone();
    const Pair p = updateStage == UpdateStage::prompt ? Pair { "LATER", "UPDATE", 56, 56 }
                 : updateStage == UpdateStage::downloading ? Pair { nullptr, "CANCEL", 0, 64 }
                 : updateStage == UpdateStage::ready ? (standalone ? Pair { "LATER", "INSTALL NOW", 56, 90 } : Pair { "NOT NOW", "GOT IT", 64, 64 })
                 : updateStage == UpdateStage::failed || updateStage == UpdateStage::mismatch ? Pair { "CLOSE", "TRY AGAIN", 56, 76 }
                 : updateStage == UpdateStage::kyrax ? Pair { "LATER", "OPEN ITCH", 56, 76 }
                                                    : Pair { nullptr, "OK", 0, 56 };
    const float right = updateW - edge, y = 140.0f;
    updateRight.setButtonText(p.right);
    updateRight.setBounds(juce::Rectangle<float>(right - p.rw, y, p.rw, 18.0f).toNearestInt());
    updateLeft.setVisible(p.left != nullptr);
    if (p.left != nullptr)
    {
        updateLeft.setButtonText(p.left);
        updateLeft.setBounds(juce::Rectangle<float>(right - p.rw - 6.0f - p.lw, y, p.lw, 18.0f).toNearestInt());
    }
}

void PhoqerView::startUpdateDownload()
{
    const auto& r = updateInfo.phoqer;
   #if ! JUCE_WINDOWS
    juce::URL(r.page).launchInDefaultBrowser();     // a Mac gets the download page
    showUpdate(UpdateStage::none);
    return;
   #endif
    updates::downloadFolder().createDirectory();
    updateFile = updates::downloadFolder().getChildFile("PHOQER-" + r.version + "-Setup.exe");
    if (updates::verify(updateFile, r)) { showUpdate(UpdateStage::ready); return; }     // already here and checked
    downloadWatch.bytes = 0;
    downloadWatch.result = 0;
    downloadWatch.release = r;
    downloadWatch.file = updateFile;
    downloadStartedMs = juce::Time::getMillisecondCounterHiRes();
    updateDownload = juce::URL(r.url).downloadToFile(updateFile, juce::URL::DownloadTaskOptions().withListener(&downloadWatch));
    showUpdate(updateDownload != nullptr ? UpdateStage::downloading : UpdateStage::failed);
}

void PhoqerView::updateTick()
{
    if (updateStage != UpdateStage::downloading || updateTestProgress >= 0.0f) return;
    updateWindow.repaint();
    const int result = downloadWatch.result.load();
    if (result == 0 || updateDownload == nullptr) return;
    if (result == 1 && juce::Time::getMillisecondCounterHiRes() - downloadStartedMs < minDownloadMs) return;     // let it be seen
    updateDownload.reset();
    showUpdate(result == 1 ? UpdateStage::ready : result == 3 ? UpdateStage::mismatch : UpdateStage::failed);
}

void PhoqerView::updatePressed(bool right)
{
    switch (updateStage)
    {
        case UpdateStage::prompt:
            if (right) startUpdateDownload(); else showUpdate(UpdateStage::none);
            return;
        case UpdateStage::downloading:
            updateDownload.reset();
            updateFile.deleteFile();
            showUpdate(UpdateStage::none);
            return;
        case UpdateStage::ready:
        {
            if (! right) { showUpdate(UpdateStage::none); return; }
            // each copy is updated where it was last run from (both remembered in setupUpdates)
            const auto saved = [](const juce::String& path) { return juce::File::isAbsolutePath(path) ? juce::File(path) : juce::File(); };
            if (updates::installWhenClosed(updateFile, updateInfo.phoqer.sha256, isStandalone(), saved(UiSettings::get().appFolder()), saved(UiSettings::get().vst3Folder())))
            {
                installScheduled = ! isStandalone();
                showUpdate(UpdateStage::none);
                repaint();
                if (isStandalone()) juce::JUCEApplicationBase::quit();     // closes; the installer runs, then PHOQER opens again
            }
            return;
        }
        case UpdateStage::failed:
        case UpdateStage::mismatch:
            if (right) startUpdateDownload(); else showUpdate(UpdateStage::none);
            return;
        case UpdateStage::kyrax:
            if (right) juce::URL(updateInfo.kyrax.page).launchInDefaultBrowser();
            showUpdate(UpdateStage::none);
            return;
        default:
            showUpdate(UpdateStage::none);
            return;
    }
}

void PhoqerView::paintUpdate(juce::Graphics& g)
{
    const float L = edge, R = updateW - edge, w = R - L;
    const auto bold = pixelFont(10.0f, true), small = pixelFont(crispPixelPoints);
    const auto ink = valueInkFor(character), dim = win98::shadow;
    const auto& pal = paletteFor(character);
    const auto head = [&](const juce::String& s) { drawText(g, s, { L, 33.0f, w, 11.0f }, bold, juce::Colours::black); };
    const auto line = [&](const juce::String& s, float y, juce::Colour c) { drawText(g, s, { L, y, w, 8.0f }, small, c, juce::Justification::topLeft); };
    const auto lines = [&](std::initializer_list<const char*> list, float y)
    {
        for (const auto* s : list) { line(juce::String(s).replace("{V}", updateInfo.phoqer.version), y, juce::Colours::black); y += 14.0f; }
    };
    const auto& v = updateInfo.phoqer.version;
    switch (updateStage)
    {
        case UpdateStage::prompt:
        {
            head("PHOQER " + v + " IS OUT");
            line("YOU HAVE " + updates::currentVersion(), 47.0f, dim);
            const juce::Rectangle<float> box { L, 62.0f, w, 61.0f };
            sunken(g, box);
            float y = 71.0f;
            for (const auto& n : updateInfo.phoqer.notes) { drawText(g, "+ " + n, { L + 7.0f, y, w - 10.0f, 8.0f }, small, juce::Colours::black, juce::Justification::topLeft); y += 16.0f; }
            break;
        }
        case UpdateStage::downloading:
        {
            head("DOWNLOADING PHOQER " + v);
            const auto size = juce::jmax<juce::int64>(1, updateInfo.phoqer.size);
            // The bar follows the download, but never faster than the 2-second minimum, so a quick one still fills smoothly.
            const float shown = static_cast<float>((juce::Time::getMillisecondCounterHiRes() - downloadStartedMs) / minDownloadMs);
            const float progress = updateTestProgress >= 0.0f ? updateTestProgress
                                 : juce::jlimit(0.0f, 1.0f, juce::jmin(shown, static_cast<float>(downloadWatch.bytes.load()) / static_cast<float>(size)));
            // Labels 2 px in from the edges; each icon centred over its label; pages leave and land at the same gap.
            const float lewonnW = textWidth("LEWONN.COM", small), phoqerW = textWidth("PHOQER", small);
            line("LEWONN.COM", 84.0f, juce::Colours::black);
            drawText(g, "PHOQER", { R - phoqerW, 84.0f, phoqerW, 8.0f }, small, juce::Colours::black, juce::Justification::topLeft);
            const float gx = std::round(L + lewonnW * 0.5f - 12.0f), dx = std::round(R - phoqerW * 0.5f - 12.0f);
            sprite(g, globe, 11, gx, 52.0f, 2.0f, [](char c, int) { return c == '#' ? juce::Colour(40, 90, 200) : juce::Colours::transparentBlack; });
            sprite(g, disk, 12, dx, 51.0f, 2.0f, [&](char c, int row) {
                switch (c)
                {
                    case 'k': return titleBarColour(character, static_cast<float>(row) / 11.0f);
                    case 'm': return juce::Colour(200, 200, 210);
                    case 's': return juce::Colour(90, 90, 100);
                    case 'w': return juce::Colours::white;
                    case 'a': return pal.accent;
                    case 'b': return pal.secondary;
                    default: return juce::Colours::transparentBlack;
                }
            });
            const float x0 = gx + 30.0f, x1 = dx - 14.0f;
            const auto step = static_cast<int>(juce::Time::getMillisecondCounterHiRes() / 110.0);
            for (int k = 0; k < 2; ++k)
            {
                const float t = static_cast<float>((step + k * 6) % 12) / 12.0f;
                const float px = std::round(x0 + (x1 - x0) * t), py = std::round(58.0f - std::sin(t * juce::MathConstants<float>::pi) * 8.0f);
                sprite(g, page, 10, px, py, 1.0f, [](char c, int) { return c == '#' ? juce::Colours::black : c == '.' ? juce::Colours::white : juce::Colours::transparentBlack; });
            }
            const juce::Rectangle<float> bar { L, 98.0f, w, 17.0f };
            sunken(g, bar, win98::face);
            const int total = static_cast<int>((w - 4.0f) / 8.0f), lit = static_cast<int>(static_cast<float>(total) * progress);
            for (int i = 0; i < lit; ++i)
            {
                g.setColour(pal.accent.interpolatedWith(pal.secondary, static_cast<float>(i) / static_cast<float>(juce::jmax(1, total))));
                g.fillRect(L + 3.0f + static_cast<float>(i) * 8.0f, 101.0f, 6.0f, 11.0f);
            }
            const auto mb = [](double bytes) { return juce::String(bytes / 1048576.0, 1); };
            line(mb(progress * static_cast<double>(size)) + " OF " + mb(static_cast<double>(size)) + " MB", 126.0f, ink);
            drawText(g, juce::String(juce::roundToInt(progress * 100.0f)) + "%", { L, 126.0f, w, 8.0f }, small, ink, juce::Justification::topRight);
            break;
        }
        case UpdateStage::ready:
            head("PHOQER " + v + " IS READY");
            line("DOWNLOADED AND CHECKED", 47.0f, ink);
            if (isStandalone()) lines({ "PHOQER WILL CLOSE, INSTALL {V}", "AND OPEN AGAIN." }, 72.0f);
            else lines({ "PHOQER CAN'T UPDATE WHILE YOUR", "DAW IS OPEN. FINISH YOUR SONG.", "{V} GOES IN WHEN YOU CLOSE IT." }, 72.0f);
            break;
        case UpdateStage::failed:
            head("THE DOWNLOAD STOPPED");
            line("THE CONNECTION DROPPED AT " + juce::String(static_cast<double>(downloadWatch.bytes.load()) / 1048576.0, 1) + " MB.", 58.0f, juce::Colours::black);
            line("NOTHING WAS INSTALLED.", 72.0f, juce::Colours::black);
            break;
        case UpdateStage::mismatch:
            head("THE DOWNLOAD DIDN'T MATCH");
            lines({ "IT WASN'T THE FILE IT SHOULD BE,", "SO PHOQER THREW IT AWAY.", "NOTHING WAS INSTALLED." }, 58.0f);
            break;
        case UpdateStage::upToDate:
            head("PHOQER IS UP TO DATE");
            line("CHECKED JUST NOW", 47.0f, ink);
            line("YOU HAVE " + updates::currentVersion() + ", THE NEWEST.", 72.0f, juce::Colours::black);
            break;
        case UpdateStage::offline:
            head("COULDN'T CHECK FOR UPDATES");
            lines({ "LEWONN.COM DIDN'T ANSWER.", "ARE YOU ONLINE?" }, 58.0f);
            break;
        case UpdateStage::kyrax:
            head("KYRAX " + updateInfo.kyrax.version + " IS OUT");
            line("YOU HAVE 1.0", 47.0f, ink);
            lines({ "YOUR NEW KYRAX FILE IS IN YOUR", "ITCH LIBRARY. DOWNLOAD IT, THEN", "USE LOAD VOICE." }, 72.0f);
            break;
        default: break;
    }
}
}
