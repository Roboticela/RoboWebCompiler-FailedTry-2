// ---------------------------------------------------------------------------
// RoboPlay - play with a document the way a browser would show it.
//
//     document.html + its stylesheet
//          |
//     RoboHTMLCompiler (in process: the same pipeline RoboViewer drives)
//          |
//     RoboRenderer + SoftwareBackend
//          |
//     one window whose pixels are the frame, with the mouse, the wheel, the
//     keyboard and the window resize forwarded to the runtime
//
// RoboViewer writes a picture of a document; RoboWatch edits one beside its
// picture. RoboPlay does neither: it opens the document in a window and hands
// the window's real input to the runtime, so a hover, a click, a typed field, a
// scroll and a resize all behave the way they do in a browser. It is the tool
// for feeling what a document actually does rather than reading a PNG of it.
//
//     RoboPlay <document.html> [options]
//
//     --css FILE       stylesheet to use        (default: the one the document
//                                               links, `app.css` beside it, or
//                                               the only .css in the folder)
//     --width N        initial logical width    (default 900)
//     --height N       initial logical height   (default 620)
//     --assets DIR     directory resources resolve against (default the
//                      document's own directory)
//     --watch          reload when either file changes on disk
//     --debug          draw the debug overlay
//     --title TEXT     window title             (default the document name)
//
// Keys: F5 reloads from disk, Escape closes the window. Everything else is the
// document's: Tab moves focus, the arrows and Backspace edit a focused field,
// Enter submits a form, the wheel scrolls.
//
// The window is Win32, which keeps the tool as dependency free as the runtime it
// shows; the target is therefore Windows only for now, and the rest of the build
// does not notice it.
// ---------------------------------------------------------------------------
#include "RendererHarness.h"

#include "RoboRenderer/Renderer.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
// `min` and `max` from the Windows SDK are function-like macros, and this file
// uses the two standard algorithms of those names.
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

namespace
{
    // The runtime's class carries the name of the system it implements, so the
    // tool aliases it rather than pulling the whole namespace in and colliding
    // with it.
    using Runtime = RoboRenderer::RoboRenderer;

    using RoboRenderer::EventResult;
    using RoboRenderer::EventType;
    using RoboRenderer::Image;
    using RoboRenderer::InputEvent;
    using RoboRenderer::Key;
    using RoboRenderer::ModifierAlt;
    using RoboRenderer::ModifierControl;
    using RoboRenderer::ModifierShift;
    using RoboRenderer::RendererOptions;
    using RoboRenderer::SoftwareBackend;

    // -----------------------------------------------------------------------
    // Options
    // -----------------------------------------------------------------------

    struct PlayOptions
    {
        std::string input;
        std::string styleSheet;
        std::string assetDirectory;
        std::string title;
        float width = 900.0f;
        float height = 620.0f;
        bool watch = false;
        bool debug = false;
    };

    void PrintUsage(const std::string& error)
    {
        std::string text;

        if (!error.empty())
        {
            text += error + "\n\n";
        }

        text +=
            "RoboPlay - open a document in a window and play with it\n\n"
            "    RoboPlay <document.html> [--css FILE] [--width N] [--height N]\n"
            "             [--assets DIR] [--watch] [--debug] [--title TEXT]\n\n"
            "Keys: F5 reloads from disk, Escape closes the window.\n";

        MessageBoxA(nullptr, text.c_str(), "RoboPlay", MB_OK | MB_ICONINFORMATION);
    }

    // -----------------------------------------------------------------------
    // Small file and string helpers
    // -----------------------------------------------------------------------

    bool ReadTextFile(const std::string& path, std::string& contents)
    {
        std::ifstream stream(path, std::ios::binary);

        if (!stream)
        {
            return false;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        contents = buffer.str();
        return true;
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

    std::string Join(const std::string& directory, const std::string& name)
    {
        return directory.empty() ? name : directory + "\\" + name;
    }

    bool FileExists(const std::string& path)
    {
        return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

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

    /// The stylesheet a document is played with, in the order a reader would
    /// expect: the file the document links, `app.css` beside it, the only .css in
    /// the folder, and failing all three the name derived from the document.
    std::string FindStyleSheet(const std::string& document, const std::string& documentText)
    {
        const std::string directory = DirectoryOf(document);
        const std::string lowered = AsciiLower(documentText);

        std::size_t search = 0;

        while (true)
        {
            const std::size_t link = lowered.find("<link", search);

            if (link == std::string::npos)
            {
                break;
            }

            const std::size_t end = lowered.find('>', link);
            search = end == std::string::npos ? lowered.size() : end + 1;

            if (end == std::string::npos)
            {
                break;
            }

            // Only a stylesheet: a `rel` of "icon" naming a .css file is not the
            // stylesheet being played.
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

            // The tag is lower cased but the path is not: the name comes from the
            // document as written, rebased from the lowered copy onto the text.
            const std::string name = documentText.substr(link + quote + 1, close - quote - 1);

            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".css") == 0 &&
                FileExists(Join(directory, name)))
            {
                return Join(directory, name);
            }
        }

        const std::string beside = Join(directory, "app.css");

        if (FileExists(beside))
        {
            return beside;
        }

        std::string firstInDirectory;

        if (!directory.empty())
        {
            WIN32_FIND_DATAA entry{};
            HANDLE handle = FindFirstFileA(Join(directory, "*.css").c_str(), &entry);

            if (handle != INVALID_HANDLE_VALUE)
            {
                firstInDirectory = Join(directory, entry.cFileName);
                FindClose(handle);
            }
        }

        if (!firstInDirectory.empty())
        {
            return firstInDirectory;
        }

        const std::size_t dot = document.find_last_of('.');

        if (dot == std::string::npos)
        {
            return document + ".css";
        }

        return document.substr(0, dot) + ".css";
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

    /// Encodes one code point as UTF-8, which is the form the runtime's text
    /// input and the compiler both read.
    void AppendUtf8(char32_t codePoint, std::string& out)
    {
        if (codePoint <= 0x7F)
        {
            out += static_cast<char>(codePoint);
        }
        else if (codePoint <= 0x7FF)
        {
            out += static_cast<char>(0xC0 | (codePoint >> 6));
            out += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else if (codePoint <= 0xFFFF)
        {
            out += static_cast<char>(0xE0 | (codePoint >> 12));
            out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (codePoint >> 18));
            out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (codePoint & 0x3F));
        }
    }

    std::string FirstLine(const std::string& text, std::size_t limit = 200)
    {
        std::size_t end = text.find('\n');
        std::string line = end == std::string::npos ? text : text.substr(0, end);

        if (line.size() > limit)
        {
            line = line.substr(0, limit) + "...";
        }

        return line;
    }

    // -----------------------------------------------------------------------
    // The frame, as a Windows surface
    // -----------------------------------------------------------------------

    /// A device independent bitmap the frame is copied into, so the window can
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
            if (image.width == 0 || image.height == 0 ||
                image.format != RoboRenderer::PixelFormat::Rgba8)
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

            // A 32 bpp DIB is BGRA in memory and the framebuffer is RGBA, so red
            // and blue change places. The frame is opaque: forcing the alpha byte
            // keeps a blit from treating it as premultiplied.
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
    // The play window
    // -----------------------------------------------------------------------

    const wchar_t* const MainClassName = L"RoboPlayWindow";

    const int TimerTick = 1;
    const int TimerWatch = 2;
    const int TimerResize = 3;

    /// How often the caret blink and any other time-based state is stepped.
    const int TickIntervalMs = 16;

    /// How often the two files are checked for an outside change, with --watch.
    const int WatchIntervalMs = 500;

    /// How long a resize waits before the document is recompiled. Recompiling
    /// picks up media queries and viewport units for the new size; waiting keeps
    /// a drag from compiling once per pixel of it.
    const int ResizeRebuildMs = 250;

    /// A drag of the mouse that selects text and the wheel step are both in
    /// logical units; three text lines is a notch.
    const float WheelStep = 100.0f;

    struct PlayWindow
    {
        HWND window = nullptr;

        PlayOptions options;

        /// Declared before the application and the renderer so it outlives them:
        /// the renderer holds a pointer to this backend.
        SoftwareBackend backend;

        std::unique_ptr<RoboTools::CompiledApplication> application;
        std::unique_ptr<Runtime> renderer;

        FrameSurface surface;

        bool running = false;
        std::string status;
        bool statusIsError = false;

        float logicalWidth = 0.0f;
        float logicalHeight = 0.0f;

        std::string html;
        std::string css;
        std::string cssName;
        long long documentStamp = -1;
        long long styleStamp = -1;

        /// The high half of a surrogate pair split across two WM_CHAR messages.
        wchar_t pendingHighSurrogate = 0;

        std::chrono::steady_clock::time_point lastTick = std::chrono::steady_clock::now();
    };

    PlayWindow& State()
    {
        static PlayWindow state;
        return state;
    }

    void RefreshTitle()
    {
        PlayWindow& state = State();

        const std::string name =
            state.options.title.empty() ? FileNameOf(state.options.input) : state.options.title;

        std::wstring title = L"RoboPlay - " + std::wstring(name.begin(), name.end());

        if (!state.status.empty())
        {
            title += L"  [" + std::wstring(state.status.begin(), state.status.end()) + L"]";
        }

        SetWindowTextW(state.window, title.c_str());
    }

    /// Reads the two files, compiles them and binds a renderer to the result. A
    /// failure leaves the previous document in place, so a half-saved file does
    /// not blank the window.
    bool Rebuild()
    {
        PlayWindow& state = State();

        std::string error;

        if (!ReadTextFile(state.options.input, state.html))
        {
            state.status = "cannot read " + state.options.input;
            state.statusIsError = true;
            RefreshTitle();
            return false;
        }

        if (state.options.styleSheet.empty())
        {
            state.options.styleSheet = FindStyleSheet(state.options.input, state.html);
        }

        state.css.clear();
        state.cssName = FileNameOf(state.options.styleSheet);

        if (FileExists(state.options.styleSheet) && !ReadTextFile(state.options.styleSheet, state.css))
        {
            state.status = "cannot read " + state.options.styleSheet;
            state.statusIsError = true;
            RefreshTitle();
            return false;
        }

        RoboTools::HarnessOptions harness;
        harness.applicationName = "Play";
        harness.documentPath = state.options.input;
        harness.viewportWidth = state.logicalWidth;
        harness.viewportHeight = state.logicalHeight;

        // The stylesheet is compiled under the name the document links, so a
        // linked `app.css` and a picked file are the same thing to the cascade.
        if (!state.css.empty())
        {
            harness.styleSheets.emplace_back(state.cssName, state.css);
        }

        harness.sourceDirectory = state.options.assetDirectory;

        auto application = std::make_unique<RoboTools::CompiledApplication>();

        if (!application->Build(state.html, harness, error))
        {
            state.status = "compile error: " + FirstLine(error);
            state.statusIsError = true;
            RefreshTitle();
            return false;
        }

        auto renderer = std::make_unique<Runtime>(application->Document());

        RendererOptions rendererOptions;
        rendererOptions.target.logicalWidth = state.logicalWidth;
        rendererOptions.target.logicalHeight = state.logicalHeight;
        rendererOptions.target.deviceScale = 1.0f;
        rendererOptions.assetDirectory = state.options.assetDirectory.empty()
                                              ? DirectoryOf(state.options.input)
                                              : state.options.assetDirectory;
        rendererOptions.clearColor = RoboRenderer::Color{ 255, 255, 255, 255 };

        if (!renderer->Initialize(state.backend, rendererOptions))
        {
            state.status = "render error: " + FirstLine(renderer->DiagnosticLog().Format());
            state.statusIsError = true;
            RefreshTitle();
            return false;
        }

        renderer->Debug().enabled = state.options.debug;

        // The old document is released only once the new one is ready, so the
        // window always has something to show.
        state.renderer = std::move(renderer);
        state.application = std::move(application);
        state.running = true;

        state.documentStamp = FileStamp(state.options.input);
        state.styleStamp = FileStamp(state.options.styleSheet);

        state.status = std::to_string(static_cast<int>(state.logicalWidth)) + "x" +
                       std::to_string(static_cast<int>(state.logicalHeight));
        state.statusIsError = false;
        RefreshTitle();

        return true;
    }

    void RenderAndPresent()
    {
        PlayWindow& state = State();

        if (!state.running || state.renderer == nullptr)
        {
            return;
        }

        state.renderer->RenderFrame();

        // A frame that ran no paint pass left the surface alone, so there is
        // nothing to upload and nothing to repaint. That is what makes an idle
        // window cost nothing, and it still repaints when the caret blinks.
        if (state.renderer->Metrics().paintRan &&
            state.surface.Upload(state.backend.Framebuffer()))
        {
            InvalidateRect(state.window, nullptr, FALSE);
        }
    }

    void UpdateViewSize(HWND window)
    {
        PlayWindow& state = State();

        RECT client{};
        GetClientRect(window, &client);

        state.logicalWidth = static_cast<float>(std::max(1L, client.right - client.left));
        state.logicalHeight = static_cast<float>(std::max(1L, client.bottom - client.top));

        if (state.running && state.renderer != nullptr)
        {
            state.renderer->Resize(state.logicalWidth, state.logicalHeight);
            RenderAndPresent();
        }
    }

    std::uint32_t CurrentModifiers()
    {
        std::uint32_t modifiers = 0;

        if (GetKeyState(VK_SHIFT) < 0)
        {
            modifiers |= ModifierShift;
        }

        if (GetKeyState(VK_CONTROL) < 0)
        {
            modifiers |= ModifierControl;
        }

        if (GetKeyState(VK_MENU) < 0)
        {
            modifiers |= ModifierAlt;
        }

        return modifiers;
    }

    void SendInput(const InputEvent& event)
    {
        PlayWindow& state = State();

        if (!state.running || state.renderer == nullptr)
        {
            return;
        }

        state.renderer->SendEvent(event);
        RenderAndPresent();
    }

    void SendPointer(EventType type, float x, float y, int button)
    {
        InputEvent event;
        event.type = type;
        event.x = x;
        event.y = y;
        event.button = button;
        event.modifiers = CurrentModifiers();
        SendInput(event);
    }

    Key MapVirtualKey(WPARAM key)
    {
        switch (key)
        {
        case VK_RETURN: return Key::Enter;
        case VK_ESCAPE: return Key::Escape;
        case VK_TAB: return Key::Tab;
        case VK_BACK: return Key::Backspace;
        case VK_DELETE: return Key::Delete;
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_HOME: return Key::Home;
        case VK_END: return Key::End;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_SPACE: return Key::Space;
        default: return Key::None;
        }
    }

    void PresentFrame(HWND window)
    {
        PlayWindow& state = State();

        PAINTSTRUCT paint{};
        HDC device = BeginPaint(window, &paint);

        RECT client{};
        GetClientRect(window, &client);

        // The window is cleared to white first: a document smaller than the
        // window would otherwise show whatever was behind it.
        static HBRUSH background = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(device, &client, background);

        if (state.surface.bitmap != nullptr && state.surface.width > 0 && state.surface.height > 0)
        {
            BitBlt(device, 0, 0, state.surface.width, state.surface.height, state.surface.device, 0, 0,
                   SRCCOPY);
        }

        EndPaint(window, &paint);
    }

    LRESULT CALLBACK MainProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        PlayWindow& state = State();

        switch (message)
        {
        case WM_PAINT:
            PresentFrame(window);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
            {
                UpdateViewSize(window);

                if (state.running)
                {
                    // Rebuilt a moment after the resize stops, so media queries
                    // and viewport units see the size the window ended up at.
                    KillTimer(window, TimerResize);
                    SetTimer(window, TimerResize, ResizeRebuildMs, nullptr);
                }
            }

            return 0;

        case WM_TIMER:
            if (wParam == TimerTick)
            {
                const auto now = std::chrono::steady_clock::now();
                const double delta =
                    std::chrono::duration<double>(now - state.lastTick).count();
                state.lastTick = now;

                if (state.running && state.renderer != nullptr)
                {
                    state.renderer->Update(delta);
                    RenderAndPresent();
                }

                return 0;
            }

            if (wParam == TimerResize)
            {
                KillTimer(window, TimerResize);
                Rebuild();
                RenderAndPresent();
                return 0;
            }

            if (wParam == TimerWatch)
            {
                if (FileStamp(state.options.input) != state.documentStamp ||
                    FileStamp(state.options.styleSheet) != state.styleStamp)
                {
                    Rebuild();
                    RenderAndPresent();
                }

                return 0;
            }

            break;

        case WM_MOUSEMOVE:
            SendPointer(EventType::PointerMove, static_cast<float>(static_cast<short>(LOWORD(lParam))),
                        static_cast<float>(static_cast<short>(HIWORD(lParam))), 0);
            return 0;

        case WM_LBUTTONDOWN:
            SetFocus(window);
            SendPointer(EventType::PointerDown, static_cast<float>(static_cast<short>(LOWORD(lParam))),
                        static_cast<float>(static_cast<short>(HIWORD(lParam))), 0);
            return 0;

        case WM_LBUTTONUP:
            SendPointer(EventType::PointerUp, static_cast<float>(static_cast<short>(LOWORD(lParam))),
                        static_cast<float>(static_cast<short>(HIWORD(lParam))), 0);
            return 0;

        case WM_LBUTTONDBLCLK:
            SendPointer(EventType::DoubleClick, static_cast<float>(static_cast<short>(LOWORD(lParam))),
                        static_cast<float>(static_cast<short>(HIWORD(lParam))), 0);
            return 0;

        case WM_RBUTTONDOWN:
            SendPointer(EventType::PointerDown, static_cast<float>(static_cast<short>(LOWORD(lParam))),
                        static_cast<float>(static_cast<short>(HIWORD(lParam))), 2);
            return 0;

        case WM_RBUTTONUP:
            SendPointer(EventType::PointerUp, static_cast<float>(static_cast<short>(LOWORD(lParam))),
                        static_cast<float>(static_cast<short>(HIWORD(lParam))), 2);
            return 0;

        case WM_MOUSEWHEEL:
        {
            POINT point{ static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)) };
            ScreenToClient(window, &point);

            const float notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;

            InputEvent event;
            event.type = EventType::Wheel;
            event.x = static_cast<float>(point.x);
            event.y = static_cast<float>(point.y);
            // A notch away from the user scrolls the content down the page, which
            // is a negative change in scroll offset.
            event.deltaY = -notches * WheelStep;
            event.modifiers = CurrentModifiers();
            SendInput(event);
            return 0;
        }

        case WM_KEYDOWN:
        {
            const std::uint32_t repeat = static_cast<std::uint32_t>(lParam & 0xFFFF);

            if (wParam == VK_F5)
            {
                Rebuild();
                RenderAndPresent();
                return 0;
            }

            if (wParam == VK_ESCAPE)
            {
                DestroyWindow(window);
                return 0;
            }

            const Key key = MapVirtualKey(wParam);

            if (key == Key::None)
            {
                return 0;
            }

            InputEvent event;
            event.type = EventType::KeyDown;
            event.key = key;
            event.modifiers = CurrentModifiers();

            for (std::uint32_t count = 0; count < (repeat == 0 ? 1u : repeat); ++count)
            {
                SendInput(event);
            }

            return 0;
        }

        case WM_CHAR:
        {
            const wchar_t character = static_cast<wchar_t>(wParam);

            // Backspace, tab, enter and escape are keys, not text; they arrive
            // here as control characters and are handled by WM_KEYDOWN.
            if (character == 0x08 || character == 0x09 || character == 0x0D || character == 0x1B)
            {
                return 0;
            }

            char32_t codePoint = character;

            if (character >= 0xD800 && character <= 0xDBFF)
            {
                state.pendingHighSurrogate = character;
                return 0;
            }

            if (state.pendingHighSurrogate != 0)
            {
                const wchar_t high = state.pendingHighSurrogate;
                state.pendingHighSurrogate = 0;

                if (character >= 0xDC00 && character <= 0xDFFF)
                {
                    codePoint = 0x10000 + ((static_cast<char32_t>(high) - 0xD800) << 10) +
                                (static_cast<char32_t>(character) - 0xDC00);
                }
            }

            std::string text;
            AppendUtf8(codePoint, text);

            InputEvent event;
            event.type = EventType::TextInput;
            event.text = text;
            SendInput(event);
            return 0;
        }

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 220;
            info->ptMinTrackSize.y = 160;
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            KillTimer(window, TimerTick);
            KillTimer(window, TimerWatch);
            KillTimer(window, TimerResize);
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool RegisterWindowClass(HINSTANCE instance)
    {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        windowClass.lpfnWndProc = MainProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = nullptr;
        windowClass.lpszClassName = MainClassName;

        return RegisterClassExW(&windowClass) != 0;
    }

    /// Parses one option value into a float, which is how the size options are
    /// written.
    bool ParseNumber(const std::string& text, float& value)
    {
        char* end = nullptr;
        const double parsed = std::strtod(text.c_str(), &end);
        value = static_cast<float>(parsed);
        return end != nullptr && *end == '\0';
    }

    bool ParseArguments(const std::vector<std::string>& arguments, PlayOptions& options)
    {
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const std::string& argument = arguments[index];

            const auto value = [&](std::string& out) {
                if (index + 1 >= arguments.size())
                {
                    return false;
                }

                out = arguments[++index];
                return true;
            };

            if (argument == "--css")
            {
                if (!value(options.styleSheet)) return false;
            }
            else if (argument == "--assets")
            {
                if (!value(options.assetDirectory)) return false;
            }
            else if (argument == "--title")
            {
                if (!value(options.title)) return false;
            }
            else if (argument == "--width" || argument == "--height")
            {
                std::string text;

                if (!value(text)) return false;

                float parsed = 0.0f;

                if (!ParseNumber(text, parsed) || parsed < 32.0f) return false;

                if (argument == "--width") options.width = parsed;
                else options.height = parsed;
            }
            else if (argument == "--watch")
            {
                options.watch = true;
            }
            else if (argument == "--debug")
            {
                options.debug = true;
            }
            else if (!argument.empty() && argument[0] == '-')
            {
                PrintUsage("unknown option: " + argument);
                return false;
            }
            else if (options.input.empty())
            {
                options.input = argument;
            }
            else
            {
                PrintUsage("only one document may be given");
                return false;
            }
        }

        if (options.input.empty())
        {
            PrintUsage("no document given");
            return false;
        }

        return true;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    // The runtime draws in logical pixels and the window is the frame, so the
    // two agree at 1:1 and a high DPI screen is handled by the OS scaler.
    SetProcessDPIAware();

    int argumentCount = 0;
    LPWSTR* wideArguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);

    std::vector<std::string> arguments;

    if (wideArguments != nullptr)
    {
        for (int index = 1; index < argumentCount; ++index)
        {
            arguments.push_back(Narrow(wideArguments[index]));
        }

        LocalFree(wideArguments);
    }

    PlayWindow& state = State();

    if (!ParseArguments(arguments, state.options))
    {
        return 2;
    }

    if (!RegisterWindowClass(instance))
    {
        MessageBoxA(nullptr, "the window class could not be registered", "RoboPlay", MB_OK | MB_ICONERROR);
        return 1;
    }

    RECT desired{ 0, 0, static_cast<LONG>(state.options.width), static_cast<LONG>(state.options.height) };
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);

    const std::wstring title = L"RoboPlay - " + std::wstring(state.options.input.begin(),
                                                             state.options.input.end());

    HWND window = CreateWindowExW(0, MainClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, desired.right - desired.left,
                                  desired.bottom - desired.top, nullptr, nullptr, instance, nullptr);

    if (window == nullptr)
    {
        MessageBoxA(nullptr, "the window could not be created", "RoboPlay", MB_OK | MB_ICONERROR);
        return 1;
    }

    state.window = window;

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    // The client size is known once the window exists, so the first compile sees
    // the size the document will actually be laid out in.
    RECT client{};
    GetClientRect(window, &client);
    state.logicalWidth = static_cast<float>(std::max(1L, client.right - client.left));
    state.logicalHeight = static_cast<float>(std::max(1L, client.bottom - client.top));

    if (!Rebuild())
    {
        MessageBoxA(window, state.status.c_str(), "RoboPlay", MB_OK | MB_ICONERROR);
    }

    RenderAndPresent();

    SetTimer(window, TimerTick, TickIntervalMs, nullptr);

    if (state.options.watch)
    {
        SetTimer(window, TimerWatch, WatchIntervalMs, nullptr);
    }

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}
