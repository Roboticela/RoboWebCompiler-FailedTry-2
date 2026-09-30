// ---------------------------------------------------------------------------
// RoboWatch - the live preview.
//
//     HTML box  +  CSS box
//          |
//     RoboHTMLCompiler (in process: the same pipeline RoboViewer drives)
//          |
//     RoboRenderer + SoftwareBackend
//          |
//     the frame, painted into the window, and written as a PNG beside the
//     document on every successful build
//
// It is the loop a stylesheet wants: edit, press Rebuild, see the frame. The two
// boxes hold the document and its stylesheet and nothing compiles until it is
// asked for, so a keystroke, a save or a large paste never stalls the window.
// The last frame that compiled stays on screen while a broken one is being
// fixed, so a half-written rule never blanks the preview. A build runs on a
// worker thread, so the boxes keep taking input while it works.
//
//     RoboWatch <document.html> [options]
//
//     --css FILE       stylesheet to preview      (default: <document>.css)
//     --output FILE    PNG to write               (default: <source>preview.png)
//     --width N        logical width              (default 900)
//     --height N       logical height             (default 620)
//     --assets DIR     directory resources resolve against. Left empty by
//                      default: with a directory, every stylesheet in it is
//                      compiled as well, and the CSS box is what is being
//                      previewed.
//     --editor COMMAND code editor to open beside the preview (default
//                      notepad; `none` leaves it alone)
//     --no-tile        do not arrange the editor and the preview side by side
//     --auto           rebuild as the boxes and the two files change; off by
//                      default, because a build per keystroke is the wrong
//                      trade for a document large enough to notice
//     --once           build once, write the PNG, print the result and exit
//     --quiet          print only the output path
//     --help           this text
//
// F5 and the Rebuild button compile the boxes; Ctrl+S writes them back to disk.
// With --auto the two files are watched as well, so editing in the editor
// reloads the boxes and rebuilds: the editor and the boxes become one loop.
// ---------------------------------------------------------------------------
#include "RendererHarness.h"

#include "RoboRenderer/Renderer.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
// `min` and `max` from the Windows SDK are function-like macros, and this file
// uses the two standard algorithms of those names.
#define NOMINMAX
#include <windows.h>

namespace
{
    // The runtime's class carries the name of the system it implements, so the
    // tools alias it rather than pulling the whole namespace in.
    using Runtime = RoboRenderer::RoboRenderer;

    using RoboRenderer::Image;
    using RoboRenderer::RendererOptions;
    using RoboRenderer::SoftwareBackend;
    using RoboRenderer::WritePngFile;

    // -----------------------------------------------------------------------
    // Options
    // -----------------------------------------------------------------------

    struct WatchOptions
    {
        std::string input;
        std::string styleSheet;
        std::string output;
        std::string assetDirectory;
        std::string editorCommand;
        float width = 900.0f;
        float height = 620.0f;
        bool tile = true;
        bool autoRebuild = false;
        bool once = false;
        bool quiet = false;
    };

    void PrintUsage()
    {
        std::cout <<
            "RoboWatch - live preview of an HTML document and its stylesheet\n"
            "\n"
            "    RoboWatch <document.html> [options]\n"
            "\n"
            "    --css FILE       stylesheet to preview   (default: <document>.css)\n"
            "    --output FILE    PNG to write            (default: preview.png)\n"
            "    --width N        logical width           (default 900)\n"
            "    --height N       logical height          (default 620)\n"
            "    --assets DIR     directory resources resolve against\n"
            "    --editor COMMAND code editor to open     (default notepad, `none` for none)\n"
            "    --no-tile        do not arrange the windows side by side\n"
            "    --auto           rebuild as the boxes and files change (off by default)\n"
            "    --once           build once, write the PNG and exit\n"
            "    --quiet          print only the output path\n"
            "    --help           this text\n";
    }

    // -----------------------------------------------------------------------
    // Text and files
    // -----------------------------------------------------------------------

    bool ReadTextFile(const std::string& path, std::string& out)
    {
        std::ifstream stream(path, std::ios::binary);

        if (!stream)
        {
            return false;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        out = buffer.str();
        return true;
    }

    bool WriteTextFile(const std::string& path, const std::string& text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);

        if (!stream)
        {
            return false;
        }

        stream << text;
        return stream.good();
    }

    long long FileStamp(const std::string& path)
    {
        WIN32_FILE_ATTRIBUTE_DATA attributes{};

        if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attributes))
        {
            return -1;
        }

        return (static_cast<long long>(attributes.ftLastWriteTime.dwHighDateTime) << 32) |
               static_cast<long long>(attributes.ftLastWriteTime.dwLowDateTime);
    }

    std::string DirectoryOf(const std::string& path)
    {
        const std::size_t slash = path.find_last_of("\\/");

        if (slash == std::string::npos)
        {
            return std::string();
        }

        return path.substr(0, slash);
    }

    std::string FileNameOf(const std::string& path)
    {
        const std::size_t slash = path.find_last_of("\\/");
        return slash == std::string::npos ? path : path.substr(slash + 1);
    }

    std::string ReplaceExtension(const std::string& path, const std::string& extension)
    {
        const std::size_t dot = path.find_last_of('.');

        if (dot == std::string::npos || path.find_last_of("\\/") > dot)
        {
            return path + extension;
        }

        return path.substr(0, dot) + extension;
    }

    bool FileExists(const std::string& path)
    {
        return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    std::string Join(const std::string& directory, const std::string& name)
    {
        return directory.empty() ? name : directory + "\\" + name;
    }

    /// Only the tag name and the attribute names are compared case insensitively
    /// here; the paths themselves keep the case the document wrote, because a
    /// file name is case sensitive on disk.
    std::string AsciiLower(const std::string& text)
    {
        std::string lowered = text;

        for (char& character : lowered)
        {
            if (character >= 'A' && character <= 'Z')
            {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }

        return lowered;
    }

    /// The stylesheet a document is watching, in the order a reader would expect:
    /// the file the document links, the `.css` beside it, and failing both the
    /// name derived from the document itself.
    ///
    /// A document that links its stylesheet is the common case, and following
    /// the link is what makes `RoboWatch page.html` preview the page the browser
    /// would show rather than an empty box beside a file nobody asked for.
    std::string FindStyleSheet(const std::string& document, const std::string& documentText)
    {
        const std::string directory = DirectoryOf(document);
        const std::string lowered = AsciiLower(documentText);

        const auto linkedStyleSheet = [&]() -> std::string {
            std::size_t search = 0;

            while (true)
            {
                const std::size_t link = lowered.find("<link", search);

                if (link == std::string::npos)
                {
                    return std::string();
                }

                const std::size_t end = lowered.find('>', link);
                search = end == std::string::npos ? lowered.size() : end + 1;

                if (end == std::string::npos)
                {
                    return std::string();
                }

                // Only a stylesheet: a `rel` of "icon" or "preload" naming a
                // .css file is not the stylesheet being previewed.
                const std::string tag = lowered.substr(link, end - link);

                if (tag.find("stylesheet") == std::string::npos)
                {
                    continue;
                }

                const std::size_t href = tag.find("href");

                if (href == std::string::npos)
                {
                    continue;
                }

                const std::size_t quote = tag.find_first_of("\"'", href);

                if (quote == std::string::npos)
                {
                    continue;
                }

                const std::size_t close = tag.find(tag[quote], quote + 1);

                if (close == std::string::npos)
                {
                    continue;
                }

                // The tag is lower cased but the path is not: a file name is
                // case sensitive on disk, so the name comes from the document as
                // written.
                const std::string name = documentText.substr(quote + 1, close - quote - 1);

                if (name.size() > 4 && name.compare(name.size() - 4, 4, ".css") == 0 &&
                    FileExists(Join(directory, name)))
                {
                    return Join(directory, name);
                }
            }
        };

        if (const std::string linked = linkedStyleSheet(); !linked.empty())
        {
            return linked;
        }

        const std::string beside = Join(directory, "app.css");

        if (FileExists(beside))
        {
            return beside;
        }

        // One stylesheet in the directory is the one being previewed, whatever
        // it is called.
        std::string firstInDirectory;

        if (!directory.empty())
        {
            WIN32_FIND_DATAA entry{};
            HANDLE search = FindFirstFileA(Join(directory, "*.css").c_str(), &entry);

            if (search != INVALID_HANDLE_VALUE)
            {
                firstInDirectory = Join(directory, entry.cFileName);
                FindClose(search);
            }
        }

        if (!firstInDirectory.empty())
        {
            return firstInDirectory;
        }

        return ReplaceExtension(document, ".css");
    }

    std::wstring Widen(const std::string& text)
    {
        if (text.empty())
        {
            return std::wstring();
        }

        const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                               static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), length);
        return wide;
    }

    std::string Narrow(const std::wstring& text)
    {
        if (text.empty())
        {
            return std::string();
        }

        const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                               nullptr, 0, nullptr, nullptr);
        std::string narrow(static_cast<std::size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), narrow.data(), length,
                            nullptr, nullptr);
        return narrow;
    }

    /// The edit controls hand back CRLF and the compiler reads the file the same
    /// way, so line endings are normalised once here rather than in every stage
    /// that has to look at a line.
    std::string NormaliseLineEndings(const std::string& text)
    {
        std::string normalised;
        normalised.reserve(text.size());

        for (std::size_t index = 0; index < text.size(); ++index)
        {
            if (text[index] == '\r')
            {
                if (index + 1 < text.size() && text[index + 1] == '\n')
                {
                    continue;
                }

                normalised += '\n';
                continue;
            }

            normalised += text[index];
        }

        return normalised;
    }

    /// The first line of a diagnostic block: the status strip has one line, and
    /// the first line is the one that says what went wrong.
    std::string FirstLine(const std::string& text, std::size_t limit = 160)
    {
        std::size_t end = text.find('\n');
        std::string line = end == std::string::npos ? text : text.substr(0, end);

        if (line.size() > limit)
        {
            line = line.substr(0, limit);
            line += "...";
        }

        return line;
    }

    // -----------------------------------------------------------------------
    // The frame, as a Windows surface
    // -----------------------------------------------------------------------

    /// A device independent bitmap the frame is copied into, so the view can
    /// blit it with GDI rather than walking pixels on every paint.
    struct FrameSurface
    {
        HDC device = nullptr;
        HBITMAP bitmap = nullptr;
        void* pixels = nullptr;
        int width = 0;
        int height = 0;

        ~FrameSurface() { Release(); }

        void Release()
        {
            if (bitmap != nullptr)
            {
                DeleteObject(bitmap);
                bitmap = nullptr;
            }

            if (device != nullptr)
            {
                DeleteDC(device);
                device = nullptr;
            }

            pixels = nullptr;
            width = 0;
            height = 0;
        }

        bool Upload(const Image& image)
        {
            if (image.width == 0 || image.height == 0 || image.format != RoboRenderer::PixelFormat::Rgba8)
            {
                return false;
            }

            if (device == nullptr)
            {
                device = CreateCompatibleDC(nullptr);
            }

            if (bitmap == nullptr || width != static_cast<int>(image.width) ||
                height != static_cast<int>(image.height))
            {
                if (bitmap != nullptr)
                {
                    DeleteObject(bitmap);
                    bitmap = nullptr;
                }

                BITMAPINFO info{};
                info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                info.bmiHeader.biWidth = static_cast<LONG>(image.width);
                // A negative height asks for a top down bitmap, which is the
                // order the framebuffer is already in.
                info.bmiHeader.biHeight = -static_cast<LONG>(image.height);
                info.bmiHeader.biPlanes = 1;
                info.bmiHeader.biBitCount = 32;
                info.bmiHeader.biCompression = BI_RGB;

                bitmap = CreateDIBSection(device, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);

                if (bitmap == nullptr)
                {
                    return false;
                }

                SelectObject(device, bitmap);
                width = static_cast<int>(image.width);
                height = static_cast<int>(image.height);
            }

            // A 32 bpp DIB is BGRA in memory and the framebuffer is RGBA, so the
            // red and blue channels change places. The frame is opaque: forcing
            // the alpha byte keeps a blit from treating it as premultiplied.
            const std::uint8_t* source = image.pixels.data();
            std::uint8_t* target = static_cast<std::uint8_t*>(pixels);
            const std::size_t bytes = static_cast<std::size_t>(image.width) * image.height * 4;

            for (std::size_t offset = 0; offset + 3 < bytes; offset += 4)
            {
                target[offset + 0] = source[offset + 2];
                target[offset + 1] = source[offset + 1];
                target[offset + 2] = source[offset + 0];
                target[offset + 3] = 255;
            }

            return true;
        }
    };

    // -----------------------------------------------------------------------
    // The watch window
    // -----------------------------------------------------------------------

    const wchar_t* const MainClassName = L"RoboWatchWindow";
    const wchar_t* const FrameClassName = L"RoboWatchFrame";

    const int TimerRebuild = 1;
    const int TimerWatch = 2;
    const int TimerLoader = 3;

    /// Posted by the build thread when it is finished; the payload is a
    /// `BuildOutcome` the window thread takes ownership of.
    const UINT MessageBuildDone = WM_APP + 1;

    /// Milliseconds a keystroke waits before the document is compiled, in the
    /// `--auto` mode: long enough that a word typed quickly is one build, long
    /// enough that the window is not built under while typing.
    const int RebuildDelayMs = 250;

    /// How often the two files are checked for a change made outside the
    /// window, in the `--auto` mode.
    const int WatchIntervalMs = 750;

    /// How often the loader dots advance while a build runs.
    const int LoaderIntervalMs = 120;

    /// The two accelerators, as command identifiers: a saved pair of files and a
    /// build asked for now rather than by a keystroke in a box.
    const WORD CommandSave = 1001;
    const WORD CommandRebuild = 1002;
    const WORD CommandAuto = 1003;

    const int StatusHeight = 22;
    const int ToolbarHeight = 30;
    const int BoxHeight = 300;
    const int Margin = 6;

    struct WatchWindow
    {
        HWND window = nullptr;
        HWND htmlBox = nullptr;
        HWND cssBox = nullptr;
        HWND frameView = nullptr;
        HWND status = nullptr;

        /// A read-only view laid over the frame when a build fails, so the whole
        /// diagnostic can be read and scrolled the way the console prints it.
        HWND errorView = nullptr;
        HWND autoBox = nullptr;
        HWND rebuildButton = nullptr;
        HWND saveButton = nullptr;
        HFONT font = nullptr;

        WatchOptions options;

        /// The two files the boxes mirror, and the names the compiler sees.
        std::string documentName;
        std::string styleName;
        long long documentStamp = -1;
        long long styleStamp = -1;

        Image frame;
        FrameSurface surface;
        bool hasFrame = false;

        std::string statusText = "ready";
        bool statusIsError = false;

        /// True when the last build failed after an earlier one had drawn a
        /// frame: the pixels on screen are then the previous build's, and the
        /// panel says so rather than pretending the picture is current.
        bool frameIsStale = false;

        /// The full text of the last failed build, shown in `errorView` while it
        /// is up. Empty as soon as a build draws a frame.
        std::string errorDetails;

        /// True while a build runs on its own thread. The window keeps taking
        /// keystrokes while it does, and a request made during a build is
        /// remembered and run after it rather than alongside it.
        bool building = false;
        bool rebuildPending = false;

        /// The build in flight. It is joined after the message loop, so closing
        /// the window never leaves a build writing to a frame nobody owns.
        std::thread build;

        /// Off by default: the two boxes compile when they are told to, not
        /// whenever a character arrives.
        bool autoRebuild = false;
        int loaderPhase = 0;

        /// Set by a keystroke and cleared when a build starts, so the strip can
        /// say that what is on screen is older than what is in the boxes.
        bool dirty = false;

        /// True while the boxes are being filled programmatically, so the
        /// resulting notifications are not mistaken for typing.
        bool loading = false;
    };

    WatchWindow& State()
    {
        static WatchWindow state;
        return state;
    }

    /// `building` with one dot more each time the loader timer fires: the state
    /// is spelled out where the eye already is, and the dots are the movement
    /// that says the window is working rather than stuck.
    std::wstring LoaderLabel()
    {
        const std::size_t phase = static_cast<std::size_t>(State().loaderPhase);
        return L"building" + std::wstring(phase, L'.');
    }

    std::string GetBoxText(HWND box)
    {
        const int length = GetWindowTextLengthW(box);

        if (length <= 0)
        {
            return std::string();
        }

        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
        GetWindowTextW(box, text.data(), length + 1);
        text.resize(static_cast<std::size_t>(length));
        return Narrow(text);
    }

    /// A multi-line edit control breaks a line on CRLF and on nothing else:
    /// text read from a file written with bare LF arrives as one long line.
    /// Every ending is expanded before the text is handed to a box.
    std::string ExpandLineEndings(const std::string& text)
    {
        std::string expanded;
        expanded.reserve(text.size() + text.size() / 16);

        for (std::size_t index = 0; index < text.size(); ++index)
        {
            if (text[index] == '\n' && (index == 0 || text[index - 1] != '\r'))
            {
                expanded += "\r\n";
                continue;
            }

            expanded += text[index];
        }

        return expanded;
    }

    void SetBoxText(HWND box, const std::string& text)
    {
        SetWindowTextW(box, Widen(ExpandLineEndings(text)).c_str());
    }

    /// What a build thread hands back to the window: the frame already drawn and
    /// the one line the status strip shows. Nothing in it points at the window,
    /// which is what lets the whole build run off the window thread.
    struct BuildOutcome
    {
        Image frame;
        bool hasFrame = false;

        /// One line for the status strip; `details` is the whole thing, the
        /// same text the console prints, for the panel to show in full.
        std::string status;
        std::string details;

        bool isError = false;
        long long elapsedMs = 0;
    };

    /// The compile, the render and the PNG, with no window call in it at all.
    /// A failed build says why and leaves the caller's last good frame alone:
    /// the preview is a working surface, and blanking it on every half-typed
    /// rule would make it useless while typing.
    BuildOutcome RunBuild(const WatchOptions& options, const std::string& styleName,
                          const std::string& html, const std::string& css)
    {
        BuildOutcome outcome;

        const ULONGLONG started = GetTickCount64();

        RoboTools::HarnessOptions harness;
        harness.applicationName = "Watch";
        harness.documentPath = options.input;
        harness.viewportWidth = options.width;
        harness.viewportHeight = options.height;

        // The stylesheet the boxes hold, under the name the document links: the
        // file on disk is what the editor is editing, and this is what is being
        // previewed, so the two are deliberately not the same bytes.
        harness.styleSheets.emplace_back(styleName, css);
        harness.sourceDirectory = options.assetDirectory;

        RoboTools::CompiledApplication application;
        std::string error;

        if (!application.Build(html, harness, error))
        {
            outcome.status = "error: " + FirstLine(error);
            outcome.details = error;
            outcome.isError = true;
            return outcome;
        }

        Runtime renderer(application.Document());

        SoftwareBackend backend;
        RendererOptions rendererOptions;
        rendererOptions.target.logicalWidth = options.width;
        rendererOptions.target.logicalHeight = options.height;
        rendererOptions.assetDirectory = options.assetDirectory;

        if (!renderer.Initialize(backend, rendererOptions) || !renderer.RenderFrame())
        {
            const std::string failure = renderer.DiagnosticLog().Format();
            outcome.status = "error: " + FirstLine(failure);
            outcome.details = failure;
            outcome.isError = true;
            return outcome;
        }

        outcome.elapsedMs = static_cast<long long>(GetTickCount64() - started);
        outcome.frame = backend.Framebuffer();
        outcome.hasFrame = true;

        const bool wrote = WritePngFile(options.output.c_str(), outcome.frame);

        std::ostringstream status;
        status << "built in " << outcome.elapsedMs << " ms  |  " << application.ElementCount()
               << " elements  |  " << static_cast<int>(options.width) << "x"
               << static_cast<int>(options.height);

        const std::string diagnostics = application.Diagnostics();

        if (!diagnostics.empty())
        {
            status << "  |  " << FirstLine(diagnostics);
        }

        if (!wrote)
        {
            status << "  |  cannot write " << options.output;
            outcome.details = "the frame was drawn but could not be written\r\n\r\n  " +
                              options.output +
                              "\r\n\r\nthe folder may be read only, or the file is open for writing\r\n"
                              "somewhere else.";
        }

        outcome.status = status.str();
        outcome.isError = !wrote;
        return outcome;
    }

    /// Writes the status line into the strip. The strip is a static control,
    /// so setting the string and invalidating it is not enough: without writing
    /// it back, the first word put there, "ready", stays on screen for the life
    /// of the window and every later status is silently dropped.
    void RefreshStatus()
    {
        WatchWindow& state = State();
        SetWindowTextW(state.status, Widen(state.statusText).c_str());
        InvalidateRect(state.status, nullptr, TRUE);
    }

    /// Shows or hides the read-only error view over the frame. A failed build
    /// replaces the picture with the whole diagnostic — the text the console
    /// prints — so the reason is never reduced to one ellipsised line.
    void ShowBuildError(bool visible, const std::string& details)
    {
        WatchWindow& state = State();

        if (visible)
        {
            // An edit wraps on CRLF, which is what `SetBoxText` expands to. The
            // change notification it sends is swallowed so it is not read as an
            // edit to the document.
            state.loading = true;
            SetBoxText(state.errorView, details);
            state.loading = false;
            // A diagnostic is read from its first line down, so the view opens
            // at the top however far the last one was scrolled.
            SendMessageW(state.errorView, EM_SETSEL, 0, 0);
            SendMessageW(state.errorView, EM_SCROLLCARET, 0, 0);
            SetWindowPos(state.errorView, HWND_TOP, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        else
        {
            ShowWindow(state.errorView, SW_HIDE);
        }
    }

    /// Starts a build on a thread of its own, or remembers that one is wanted
    /// when a build is already running: two at once would race for the frame,
    /// and the text that was asked for second is the one worth compiling.
    void StartBuild()
    {
        WatchWindow& state = State();

        if (state.building)
        {
            state.rebuildPending = true;
            return;
        }

        // Both boxes are read here, on the window thread, because reading a
        // control is the one thing a worker cannot do.
        const std::string html = NormaliseLineEndings(GetBoxText(state.htmlBox));
        const std::string css = NormaliseLineEndings(GetBoxText(state.cssBox));

        state.building = true;
        state.rebuildPending = false;
        state.loaderPhase = 0;
        state.dirty = false;
        state.statusText = "building";
        state.statusIsError = false;

        // The frame (or the loader badge) is watched while the next build runs,
        // so a diagnostic from the build before is put away first.
        ShowBuildError(false, std::string());

        SetTimer(state.window, TimerLoader, LoaderIntervalMs, nullptr);
        RefreshStatus();
        InvalidateRect(state.frameView, nullptr, TRUE);

        const WatchOptions options = state.options;
        const std::string styleName = state.styleName;
        const HWND window = state.window;

        state.build = std::thread([window, options, styleName, html, css]() {
            // The outcome is handed over as a pointer the window thread takes
            // ownership of and deletes once the message arrives.
            auto outcome = std::make_unique<BuildOutcome>(RunBuild(options, styleName, html, css));
            PostMessageW(window, MessageBuildDone, 0, reinterpret_cast<LPARAM>(outcome.release()));
        });
    }

    /// Takes what the build thread left behind. Only the window thread touches
    /// the surface and the controls, so the frame is uploaded and the status is
    /// written here rather than in the thread that drew the frame.
    void FinishBuild(std::unique_ptr<BuildOutcome> outcome)
    {
        WatchWindow& state = State();

        // The thread posted this outcome as its last act, so the join is a
        // formality: it is what keeps a handle from piling up per build.
        if (state.build.joinable())
        {
            state.build.join();
        }

        state.building = false;
        KillTimer(state.window, TimerLoader);
        state.loaderPhase = 0;

        if (outcome->hasFrame)
        {
            state.frame = outcome->frame;
            state.hasFrame = state.surface.Upload(state.frame);
        }

        state.statusText = outcome->status;
        state.statusIsError = outcome->isError;

        // A failed build keeps the last good frame, but that frame is now older
        // than the boxes: the panel catches the eye with it so a mistyped tag
        // does not look like the edit had no effect at all.
        state.frameIsStale = outcome->isError && state.hasFrame;

        // A failure is spelled out in full over the panel, exactly as the
        // console prints it; a build that drew a frame takes the view down.
        state.errorDetails = outcome->isError ? outcome->details : std::string();
        ShowBuildError(state.statusIsError, state.errorDetails);

        // Typing while this build ran means the frame is already behind the
        // boxes again, and the strip is the only place that can say so.
        if (state.dirty)
        {
            state.statusText += "  |  edited since this build";
        }

        RefreshStatus();
        InvalidateRect(state.frameView, nullptr, TRUE);

        // A request made while this build ran is honoured now, so a keystroke
        // late in a build is never dropped.
        if (state.rebuildPending)
        {
            state.rebuildPending = false;
            StartBuild();
        }
    }

    /// A build asked for by a keystroke or a change on disk. The timer collapses
    /// a burst of typing into one compile. It is only reached when the watch was
    /// asked to keep up — `--auto` or the Auto rebuild box — because a build per
    /// keystroke is the wrong trade for a document large enough to notice.
    void ScheduleRebuild()
    {
        SetTimer(State().window, TimerRebuild, RebuildDelayMs, nullptr);
    }

    void SaveBoxes()
    {
        WatchWindow& state = State();

        const std::string html = GetBoxText(state.htmlBox);
        const std::string css = GetBoxText(state.cssBox);

        const bool wrote = WriteTextFile(state.options.input, html) &&
                           WriteTextFile(state.options.styleSheet, css);

        // The stamps are refreshed so the watcher does not read this very save
        // back as a change made somewhere else.
        state.documentStamp = FileStamp(state.options.input);
        state.styleStamp = FileStamp(state.options.styleSheet);

        state.statusText = wrote ? "saved " + state.options.input + " and " + state.options.styleSheet
                                 : "cannot save the two files";
        state.statusIsError = !wrote;
        RefreshStatus();
    }

    /// Reloads a box whose file changed on disk, unless the box is what wrote it.
    void ReloadIfChanged()
    {
        WatchWindow& state = State();

        const long long documentStamp = FileStamp(state.options.input);

        if (documentStamp != state.documentStamp)
        {
            std::string text;

            if (ReadTextFile(state.options.input, text))
            {
                state.documentStamp = documentStamp;
                state.loading = true;
                SetBoxText(state.htmlBox, text);
                state.loading = false;

                // The file changed under the window, so the box follows it; a
                // build still waits to be asked for unless `--auto` said
                // otherwise.
                if (state.autoRebuild)
                {
                    ScheduleRebuild();
                }
            }
        }

        const long long styleStamp = FileStamp(state.options.styleSheet);

        if (styleStamp != state.styleStamp)
        {
            std::string text;

            if (ReadTextFile(state.options.styleSheet, text))
            {
                state.styleStamp = styleStamp;
                state.loading = true;
                SetBoxText(state.cssBox, text);
                state.loading = false;

                if (state.autoRebuild)
                {
                    ScheduleRebuild();
                }
            }
        }
    }

    /// A label drawn in a corner of the preview: how the panel says its state
    /// where the eye already is, rather than only in the status strip. `row`
    /// stacks labels when more than one wants the corner.
    void DrawFrameBadge(HDC target, const RECT& client, const std::wstring& label, int row,
                        COLORREF fill, COLORREF edge, COLORREF text)
    {
        SIZE extent{};
        GetTextExtentPoint32W(target, label.c_str(), static_cast<int>(label.size()), &extent);

        const int top = client.top + Margin + row * (extent.cy + 8 + Margin);
        RECT badge{ client.left + Margin, top, client.left + Margin + extent.cx + 16,
                    top + extent.cy + 8 };

        HBRUSH fillBrush = CreateSolidBrush(fill);
        FillRect(target, &badge, fillBrush);
        DeleteObject(fillBrush);

        HBRUSH edgeBrush = CreateSolidBrush(edge);
        FrameRect(target, &badge, edgeBrush);
        DeleteObject(edgeBrush);

        const int savedMode = SetBkMode(target, TRANSPARENT);
        const COLORREF savedColour = SetTextColor(target, text);
        DrawTextW(target, label.c_str(), -1, &badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(target, savedColour);
        SetBkMode(target, savedMode);
    }

    void PaintFrameView(HWND window)
    {
        WatchWindow& state = State();

        PAINTSTRUCT paint{};
        HDC target = BeginPaint(window, &paint);

        RECT client{};
        GetClientRect(window, &client);

        HBRUSH background = CreateSolidBrush(RGB(238, 238, 240));
        FillRect(target, &client, background);
        DeleteObject(background);

        // The last frame is copied through, so the panel's own edge shows even
        // when nothing has compiled yet.
        HBRUSH border = CreateSolidBrush(RGB(120, 120, 128));
        FrameRect(target, &client, border);
        DeleteObject(border);

        if (state.surface.bitmap != nullptr && state.surface.width > 0 && state.surface.height > 0)
        {
            const double availableWidth = static_cast<double>(client.right - client.left - 2 * Margin);
            const double availableHeight = static_cast<double>(client.bottom - client.top - 2 * Margin);
            const double scale = std::min(availableWidth / state.surface.width,
                                          availableHeight / state.surface.height);

            const int width = static_cast<int>(state.surface.width * scale + 0.5);
            const int height = static_cast<int>(state.surface.height * scale + 0.5);
            const int x = (client.right - client.left - width) / 2;
            const int y = (client.bottom - client.top - height) / 2;

            SetStretchBltMode(target, HALFTONE);
            SetBrushOrgEx(target, 0, 0, nullptr);
            StretchBlt(target, x, y, width, height, state.surface.device, 0, 0, state.surface.width,
                       state.surface.height, SRCCOPY);
        }
        else
        {
            // Nothing has been drawn yet, so the panel itself carries the last
            // word: the build running, or why the first build never produced a
            // frame. Without this an error on the very first build would look
            // like an empty window rather than a document that will not compile.
            std::wstring message;

            if (state.building)
            {
                message = L"Building...";
            }
            else if (state.statusIsError)
            {
                message = Widen(state.statusText);
            }
            else
            {
                message = L"Nothing has compiled yet.";
            }

            SetTextColor(target, state.statusIsError ? RGB(180, 30, 30) : RGB(90, 90, 96));
            SetBkMode(target, TRANSPARENT);
            RECT text = client;
            text.left += Margin * 2;
            text.right -= Margin * 2;
            DrawTextW(target, message.c_str(), -1, &text, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        }

        // The badges sit over the frame rather than instead of it: the last good
        // preview stays readable while the next one is compiled, and a failed
        // build says the picture is the previous one instead of leaving that to
        // be guessed from a strip at the bottom.
        if (state.building)
        {
            DrawFrameBadge(target, client, LoaderLabel(), 0, RGB(255, 247, 214), RGB(196, 168, 84),
                           RGB(88, 68, 12));
        }
        else if (state.frameIsStale)
        {
            DrawFrameBadge(target, client, L"build failed - showing the last good preview", 0,
                           RGB(255, 228, 228), RGB(196, 80, 80), RGB(140, 24, 24));
        }

        EndPaint(window, &paint);
    }

    void Layout(HWND window)
    {
        WatchWindow& state = State();

        RECT client{};
        GetClientRect(window, &client);

        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        // The toolbar row of buttons across the top, the two boxes under it and
        // the frame taking whatever is left.
        const int buttonHeight = ToolbarHeight - 10;
        const int buttonTop = (ToolbarHeight - buttonHeight) / 2;
        const int buttonWidth = 96;
        const int checkboxWidth = 110;

        MoveWindow(state.rebuildButton, Margin, buttonTop, buttonWidth, buttonHeight, TRUE);
        MoveWindow(state.saveButton, Margin * 2 + buttonWidth, buttonTop, buttonWidth, buttonHeight, TRUE);
        MoveWindow(state.autoBox, Margin * 3 + buttonWidth * 2, buttonTop, checkboxWidth, buttonHeight, TRUE);

        const int boxesTop = ToolbarHeight + Margin;
        int boxesHeight = std::min(BoxHeight, (height - ToolbarHeight - StatusHeight) / 2);

        if (boxesHeight < 60)
        {
            boxesHeight = 60;
        }

        const int boxWidth = (width - 3 * Margin) / 2;
        const int frameTop = boxesTop + boxesHeight + Margin;
        int frameHeight = height - frameTop - StatusHeight;

        if (frameHeight < 0)
        {
            frameHeight = 0;
        }

        MoveWindow(state.htmlBox, Margin, boxesTop, boxWidth, boxesHeight, TRUE);
        MoveWindow(state.cssBox, Margin * 2 + boxWidth, boxesTop, width - boxWidth - 3 * Margin, boxesHeight,
                   TRUE);
        MoveWindow(state.frameView, 0, frameTop, width, frameHeight, TRUE);
        // The error view sits exactly over the frame, so a failure replaces the
        // picture with its reason rather than only annotating the corner.
        MoveWindow(state.errorView, 0, frameTop, width, frameHeight, TRUE);
        MoveWindow(state.status, 0, height - StatusHeight, width, StatusHeight, TRUE);
    }

    LRESULT CALLBACK FrameViewProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_PAINT:
            PaintFrameView(window);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            InvalidateRect(window, nullptr, TRUE);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    /// Draws the class of an editor and the preview's own status strip.
    LRESULT OnColourControl(HDC device, HWND control)
    {
        WatchWindow& state = State();

        if (control == state.status)
        {
            SetTextColor(device, state.statusIsError ? RGB(180, 30, 30) : RGB(40, 40, 44));
            SetBkColor(device, RGB(232, 232, 236));
            static HBRUSH brush = CreateSolidBrush(RGB(232, 232, 236));
            return reinterpret_cast<LRESULT>(brush);
        }

        // A read-only edit is coloured as a static control, so the diagnostic
        // view is a light red page rather than another white editor: it reads
        // as an error at a glance, the way the console's lines do.
        if (control == state.errorView)
        {
            SetTextColor(device, RGB(140, 24, 24));
            SetBkColor(device, RGB(255, 240, 240));
            static HBRUSH errorBrush = CreateSolidBrush(RGB(255, 240, 240));
            return reinterpret_cast<LRESULT>(errorBrush);
        }

        static HBRUSH editorBrush = CreateSolidBrush(RGB(255, 255, 255));
        SetBkColor(device, RGB(255, 255, 255));
        return reinterpret_cast<LRESULT>(editorBrush);
    }

    LRESULT CALLBACK MainProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        WatchWindow& state = State();

        switch (message)
        {
        case WM_SIZE:
            Layout(window);
            return 0;

        case WM_TIMER:
            if (wParam == TimerRebuild)
            {
                KillTimer(window, TimerRebuild);
                StartBuild();
                return 0;
            }

            if (wParam == TimerWatch)
            {
                ReloadIfChanged();
                return 0;
            }

            if (wParam == TimerLoader)
            {
                state.loaderPhase = (state.loaderPhase + 1) % 4;
                state.statusText =
                    "building" + std::string(static_cast<std::size_t>(state.loaderPhase), '.');
                RefreshStatus();
                InvalidateRect(state.frameView, nullptr, TRUE);
                return 0;
            }

            break;

        case MessageBuildDone:
            FinishBuild(std::unique_ptr<BuildOutcome>(reinterpret_cast<BuildOutcome*>(lParam)));
            return 0;

        case WM_COMMAND:
            // The toolbar buttons and the two accelerators share their ids, so
            // Rebuild and Save arrive here however they were asked for.
            switch (LOWORD(wParam))
            {
            case CommandRebuild:
                StartBuild();
                return 0;

            case CommandSave:
                SaveBoxes();
                StartBuild();
                return 0;

            case CommandAuto:
                if (HIWORD(wParam) == BN_CLICKED)
                {
                    state.autoRebuild = SendMessageW(state.autoBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    state.statusText = state.autoRebuild
                                           ? "auto rebuild on: a change in a box or either file builds"
                                           : "auto rebuild off: F5 or Rebuild builds";
                    state.statusIsError = false;
                    RefreshStatus();

                    if (state.autoRebuild)
                    {
                        ScheduleRebuild();
                    }
                }

                return 0;

            default:
                break;
            }

            // A keystroke only schedules a build when the watch was asked to
            // keep up: by default a box compiles when F5, Ctrl+S or the Rebuild
            // button says so, so a large paste never starts one by itself. What
            // a keystroke does do is mark the frame on screen as old.
            if (HIWORD(wParam) == EN_CHANGE && !state.loading)
            {
                state.dirty = true;

                if (state.autoRebuild)
                {
                    ScheduleRebuild();
                }
                else if (!state.building)
                {
                    state.statusText = "edited: F5, Ctrl+S or Rebuild builds it";
                    state.statusIsError = false;
                    RefreshStatus();
                }
            }

            return 0;

        case WM_CTLCOLORSTATIC:
            return OnColourControl(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));

        case WM_CTLCOLOREDIT:
            return OnColourControl(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));

        case WM_SETFOCUS:
            SetFocus(state.htmlBox);
            return 0;

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            KillTimer(window, TimerRebuild);
            KillTimer(window, TimerWatch);
            KillTimer(window, TimerLoader);
            state.surface.Release();
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool RegisterWindowClasses(HINSTANCE instance)
    {
        WNDCLASSEXW main{};
        main.cbSize = sizeof(main);
        main.style = CS_HREDRAW | CS_VREDRAW;
        main.lpfnWndProc = MainProc;
        main.hInstance = instance;
        main.hCursor = LoadCursor(nullptr, IDC_ARROW);
        main.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        main.lpszClassName = MainClassName;

        if (RegisterClassExW(&main) == 0)
        {
            return false;
        }

        WNDCLASSEXW frame{};
        frame.cbSize = sizeof(frame);
        frame.style = CS_HREDRAW | CS_VREDRAW;
        frame.lpfnWndProc = FrameViewProc;
        frame.hInstance = instance;
        frame.hCursor = LoadCursor(nullptr, IDC_ARROW);
        frame.lpszClassName = FrameClassName;

        return RegisterClassExW(&frame) != 0;
    }

    /// Opens the editor on the document and hands the window half the screen.
    /// A code editor beside the preview is the point of `Watch.bat`: the boxes
    /// are for a quick experiment, the editor is for the file.
    struct WindowSearch
    {
        DWORD process = 0;
        HWND window = nullptr;
    };

    BOOL CALLBACK FindEditorWindow(HWND window, LPARAM parameter)
    {
        WindowSearch& search = *reinterpret_cast<WindowSearch*>(parameter);

        DWORD process = 0;
        GetWindowThreadProcessId(window, &process);

        if (process != search.process || !IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr)
        {
            return TRUE;
        }

        // The first visible top level window of the process with a title of its
        // own: an editor opens a frame before it opens any helper window.
        if (GetWindowTextLengthW(window) == 0)
        {
            return TRUE;
        }

        search.window = window;
        return FALSE;
    }

    HWND StartEditor(const std::string& command, const std::string& file)
    {
        if (command.empty() || command == "none")
        {
            return nullptr;
        }

        std::wstring line = Widen(command) + L" \"" + Widen(file) + L"\"";

        // An editor started from a shortcut works in its own directory; a path
        // of "" is not "the current directory" to CreateProcess, so the empty
        // case is spelled as a null pointer.
        const std::wstring directory = Widen(DirectoryOf(file));
        const wchar_t* const workingDirectory = directory.empty() ? nullptr : directory.c_str();

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};

        if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                            workingDirectory, &startup, &process))
        {
            return nullptr;
        }

        CloseHandle(process.hThread);

        WindowSearch search;
        search.process = process.dwProcessId;

        // The editor takes a moment to put its window up; three seconds is long
        // enough for one to start and short enough not to hold up the preview.
        for (int attempt = 0; attempt < 30 && search.window == nullptr; ++attempt)
        {
            Sleep(100);
            EnumWindows(&FindEditorWindow, reinterpret_cast<LPARAM>(&search));
        }

        CloseHandle(process.hProcess);
        return search.window;
    }

    void TileWindows(HWND preview, HWND editor)
    {
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

        const int width = work.right - work.left;
        const int height = work.bottom - work.top;
        const int half = width / 2;

        if (editor != nullptr)
        {
            // Restored first: a maximized editor would sit on top of the preview
            // however it is moved.
            ShowWindow(editor, SW_RESTORE);
            MoveWindow(editor, work.left, work.top, half, height, TRUE);
        }

        MoveWindow(preview, work.left + half, work.top, width - half, height, TRUE);
    }

    // -----------------------------------------------------------------------
    // Command line
    // -----------------------------------------------------------------------

    /// A default editor that is always present, with the two that people
    /// actually install preferred when they are on the path.
    std::string DefaultEditor()
    {
        const char* const candidates[] = { "code", "notepad++", "notepad" };

        for (const char* candidate : candidates)
        {
            std::string found;

            if (candidate == std::string("notepad"))
            {
                return candidate;
            }

            // `where` is how the shell itself would decide whether a name is on
            // the path, so the answer matches what the editor would do.
            std::string command = std::string("where ") + candidate + " >nul 2>nul";

            if (std::system(command.c_str()) == 0)
            {
                return candidate;
            }
        }

        return "notepad";
    }

    bool ParseOptions(int argc, char** argv, WatchOptions& options)
    {
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];

            const auto value = [&](const char* name, std::string& target) {
                if (index + 1 >= argc)
                {
                    std::cerr << "[RoboWatch] " << name << " needs a value\n";
                    return false;
                }

                target = argv[++index];
                return true;
            };

            if (argument == "--css")
            {
                if (!value("--css", options.styleSheet))
                {
                    return false;
                }

                continue;
            }

            if (argument == "--output" || argument == "-o")
            {
                if (!value("--output", options.output))
                {
                    return false;
                }

                continue;
            }

            if (argument == "--assets")
            {
                if (!value("--assets", options.assetDirectory))
                {
                    return false;
                }

                continue;
            }

            if (argument == "--editor")
            {
                if (!value("--editor", options.editorCommand))
                {
                    return false;
                }

                continue;
            }

            if (argument == "--width" || argument == "--height")
            {
                std::string text;

                if (!value(argument.c_str(), text))
                {
                    return false;
                }

                const double number = std::atof(text.c_str());

                if (number < 1.0)
                {
                    std::cerr << "[RoboWatch] " << argument << " needs a positive number\n";
                    return false;
                }

                if (argument == "--width")
                {
                    options.width = static_cast<float>(number);
                }
                else
                {
                    options.height = static_cast<float>(number);
                }

                continue;
            }

            if (argument == "--no-tile")
            {
                options.tile = false;
                continue;
            }

            if (argument == "--auto")
            {
                options.autoRebuild = true;
                continue;
            }

            if (argument == "--once")
            {
                options.once = true;
                continue;
            }

            if (argument == "--quiet")
            {
                options.quiet = true;
                continue;
            }

            if (!argument.empty() && argument[0] == '-')
            {
                std::cerr << "[RoboWatch] unknown option: " << argument << "\n";
                PrintUsage();
                return false;
            }

            if (!options.input.empty())
            {
                std::cerr << "[RoboWatch] the document was given twice: " << argument << "\n";
                return false;
            }

            options.input = argument;
        }

        if (options.input.empty())
        {
            std::cerr << "[RoboWatch] no document to preview\n\n";
            PrintUsage();
            return false;
        }


        if (options.styleSheet.empty())
        {
            std::string documentText;
            ReadTextFile(options.input, documentText);
            options.styleSheet = FindStyleSheet(options.input, documentText);
        }

        if (options.output.empty())
        {
            const std::string directory = DirectoryOf(options.input);
            options.output = (directory.empty() ? std::string() : directory + "\\") + "preview.png";
        }

        return true;
    }

    /// The once-through build: the same compile and render the window uses, with
    /// the result reported on the console instead of painted. It is what keeps
    /// the preview honest - a change to the compiler shows up here too.
    int RunOnce(const WatchOptions& options)
    {
        std::string html;
        std::string css;

        if (!ReadTextFile(options.input, html))
        {
            std::cerr << "[RoboWatch] cannot read " << options.input << "\n";
            return 1;
        }

        if (!ReadTextFile(options.styleSheet, css))
        {
            std::cerr << "[RoboWatch] cannot read " << options.styleSheet << "\n";
            return 1;
        }

        RoboTools::HarnessOptions harness;
        harness.applicationName = "Watch";
        harness.documentPath = options.input;
        harness.viewportWidth = options.width;
        harness.viewportHeight = options.height;
        harness.styleSheets.emplace_back(FileNameOf(options.styleSheet), NormaliseLineEndings(css));
        harness.sourceDirectory = options.assetDirectory;

        RoboTools::CompiledApplication application;
        std::string error;

        const ULONGLONG started = GetTickCount64();

        if (!application.Build(NormaliseLineEndings(html), harness, error))
        {
            std::cerr << "[RoboWatch] compilation failed\n" << error << "\n";
            return 1;
        }

        Runtime renderer(application.Document());

        SoftwareBackend backend;
        RendererOptions rendererOptions;
        rendererOptions.target.logicalWidth = options.width;
        rendererOptions.target.logicalHeight = options.height;
        rendererOptions.assetDirectory = options.assetDirectory;

        if (!renderer.Initialize(backend, rendererOptions) || !renderer.RenderFrame())
        {
            std::cerr << "[RoboWatch] the frame could not be drawn\n"
                      << renderer.DiagnosticLog().Format() << "\n";
            return 1;
        }

        if (!WritePngFile(options.output.c_str(), backend.Framebuffer()))
        {
            std::cerr << "[RoboWatch] cannot write " << options.output << "\n";
            return 1;
        }

        if (options.quiet)
        {
            std::cout << options.output << "\n";
            return 0;
        }

        std::cout << "RoboWatch\r\n"
                  << "=========\r\n"
                  << "  document ....... " << options.input << "\r\n"
                  << "  stylesheet ..... " << options.styleSheet << "\r\n"
                  << "  elements ....... " << application.ElementCount() << " compiled\r\n"
                  << "  window ......... " << static_cast<int>(options.width) << "x"
                  << static_cast<int>(options.height) << "\r\n"
                  << "  output ......... " << options.output << "\r\n"
                  << "  build .......... " << (GetTickCount64() - started) << " ms\r\n";

        const std::string diagnostics = application.Diagnostics();

        if (!diagnostics.empty())
        {
            std::cout << "\ndiagnostics\r\n-----------\r\n" << diagnostics << "\r\n";
        }

        const std::string rendererDiagnostics = renderer.DiagnosticLog().Format();

        if (!rendererDiagnostics.empty())
        {
            std::cout << "\nrenderer diagnostics\r\n--------------------\r\n" << rendererDiagnostics << "\r\n";
        }

        return 0;
    }

    /// Fills the boxes from the two files, creating them empty when they are not
    /// there yet: the preview of a file that does not exist is how a new one
    /// starts.
    void LoadBoxes(WatchWindow& state)
    {
        std::string html;
        std::string css;

        ReadTextFile(state.options.input, html);
        ReadTextFile(state.options.styleSheet, css);

        state.loading = true;
        SetBoxText(state.htmlBox, html);
        SetBoxText(state.cssBox, css);
        state.loading = false;

        state.documentStamp = FileStamp(state.options.input);
        state.styleStamp = FileStamp(state.options.styleSheet);
    }

    int RunWindow(const WatchOptions& options, HINSTANCE instance)
    {
        WatchWindow& state = State();
        state.options = options;
        state.documentName = FileNameOf(options.input);
        state.styleName = FileNameOf(options.styleSheet);

        if (!RegisterWindowClasses(instance))
        {
            std::cerr << "[RoboWatch] the window classes could not be registered\n";
            return 1;
        }

        const std::wstring title = L"RoboWatch - " + Widen(state.documentName) + L" + " +
                                   Widen(state.styleName);

        state.window = CreateWindowExW(0, MainClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
                                       CW_USEDEFAULT, CW_USEDEFAULT, 980, 900, nullptr, nullptr,
                                       instance, nullptr);

        if (state.window == nullptr)
        {
            std::cerr << "[RoboWatch] the window could not be created\n";
            return 1;
        }

        state.font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 FIXED_PITCH | FF_MODERN, L"Consolas");

        const DWORD boxStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_BORDER | WS_VSCROLL |
                               WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                               ES_WANTRETURN;

        state.htmlBox = CreateWindowExW(0, L"EDIT", L"", boxStyle, 0, 0, 10, 10, state.window,
                                        nullptr, instance, nullptr);
        state.cssBox = CreateWindowExW(0, L"EDIT", L"", boxStyle, 0, 0, 10, 10, state.window,
                                       nullptr, instance, nullptr);
        // WS_CLIPSIBLINGS matters here: the error view is laid over this view,
        // and without it a repaint of the frame would draw straight over the
        // diagnostic, leaving the old picture visible and the error nowhere.
        state.frameView = CreateWindowExW(0, FrameClassName, L"",
                                          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 10, 10,
                                          state.window, nullptr, instance, nullptr);
        state.status = CreateWindowExW(0, L"STATIC", L"ready", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       0, 0, 10, 10, state.window, nullptr, instance, nullptr);

        // The error view is created hidden and shown only over a failed build:
        // a read-only edit, so a diagnostic can be read, scrolled and copied
        // exactly as the console prints it rather than squeezed into one line.
        state.errorView = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_CLIPSIBLINGS | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY |
                ES_AUTOVSCROLL | ES_WANTRETURN,
            0, 0, 10, 10, state.window, nullptr, instance, nullptr);

        // The toolbar: the two ways to compile and the switch that lets the
        // window keep up on its own. A button is the discoverable version of the
        // accelerator, and the switch is what makes the default visible.
        state.rebuildButton = CreateWindowExW(0, L"BUTTON", L"Rebuild  (F5)",
                                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10,
                                              state.window,
                                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(CommandRebuild)),
                                              instance, nullptr);
        state.saveButton = CreateWindowExW(0, L"BUTTON", L"Save  (Ctrl+S)",
                                           WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 10, 10,
                                           state.window,
                                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(CommandSave)),
                                           instance, nullptr);
        state.autoBox = CreateWindowExW(0, L"BUTTON", L"Auto rebuild",
                                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 10, 10,
                                        state.window,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(CommandAuto)),
                                        instance, nullptr);

        for (HWND control : { state.htmlBox, state.cssBox, state.status, state.errorView,
                              state.rebuildButton, state.saveButton, state.autoBox })
        {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(state.font), TRUE);
        }

        LoadBoxes(state);

        state.autoRebuild = options.autoRebuild;
        SendMessageW(state.autoBox, BM_SETCHECK, options.autoRebuild ? BST_CHECKED : BST_UNCHECKED, 0);

        ShowWindow(state.window, SW_SHOW);
        UpdateWindow(state.window);

        const std::string editorCommand =
            options.editorCommand.empty() ? DefaultEditor() : options.editorCommand;
        HWND editor = StartEditor(editorCommand, options.input);

        if (options.tile)
        {
            TileWindows(state.window, editor);
        }

        // The first build is started before the message loop so the window opens
        // on the preview — or on the loader, which is the window saying it is
        // working rather than a placeholder that never changes.
        StartBuild();

        SetTimer(state.window, TimerWatch, WatchIntervalMs, nullptr);

        // Ctrl+S saves, F5 rebuilds now: the two things a preview is asked for
        // that typing cannot express.
        ACCEL accelerators[] = {
            { FVIRTKEY | FCONTROL, 'S', CommandSave },
            { FVIRTKEY, VK_F5, CommandRebuild },
        };

        HACCEL acceleratorTable = CreateAcceleratorTableW(accelerators, 2);

        MSG message{};

        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            // A translated accelerator arrives as a WM_COMMAND carrying the id
            // it was created with, which is why the two commands are named by
            // number rather than by menu item.
            if (acceleratorTable != nullptr &&
                TranslateAcceleratorW(state.window, acceleratorTable, &message))
            {
                continue;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        // A build still in flight is waited for here rather than in the window
        // procedure, so closing stays instant and the build's last write is not
        // cut off halfway.
        if (state.build.joinable())
        {
            state.build.join();
        }

        if (acceleratorTable != nullptr)
        {
            DestroyAcceleratorTable(acceleratorTable);
        }

        if (state.font != nullptr)
        {
            DeleteObject(state.font);
        }

        return 0;
    }
}

int main(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];

        if (argument == "--help" || argument == "-h" || argument == "/?")
        {
            PrintUsage();
            return 0;
        }
    }

    WatchOptions options;

    if (!ParseOptions(argc, argv, options))
    {
        return 1;
    }

    // A console that is only ever asked for one answer does not need to keep a
    // window open for it.
    if (!options.once && !options.quiet)
    {
        std::cout << "RoboWatch: " << options.input << " + " << options.styleSheet << "\n"
                  << "  Ctrl+S saves both files, F5 rebuilds now, closing the window stops the watch.\n"
                  << (options.autoRebuild
                          ? "  Auto rebuild is on: a change in a box or either file builds.\n"
                          : "  Auto rebuild is off: the boxes compile when F5, Ctrl+S or Rebuild asks.\n");
    }

    if (options.once)
    {
        return RunOnce(options);
    }

    if (!FileExists(options.input) && !FileExists(options.styleSheet))
    {
        std::cerr << "[RoboWatch] neither " << options.input << " nor " << options.styleSheet
                  << " exists; the boxes start empty and Ctrl+S creates them\n";
    }

    return RunWindow(options, GetModuleHandleW(nullptr));
}
