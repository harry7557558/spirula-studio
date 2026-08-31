// GuiMain.cpp -- the GUI entry point: GLFW window + OpenGL 3.2
// core context (lowest version that also satisfies macOS) + Dear ImGui
// bootstrap, then hands every frame to gui::GuiApp.

#include "app/Tools.h"
#include "i18n/catalog/Log.h"
#include "app/AppPaths.h"
#include "app/CrashLog.h"
#include "app/gui/Fonts.h"
#include "app/gui/GuiApp.h"
#include "app/gui/Layout.h"
#include "app/gui/NavCamera.h"
#include "i18n/catalog/Brand.h"

#ifdef _WIN32
// Before the GL headers: both this and they define APIENTRY, and windows.h
// first is the order that does not warn.
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX   // keep windows.h from defining min()/max() macros
#endif
#include <windows.h>
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include "app_generated/app_icon.h"
#include "external/stb_image.h"
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "[glfw] error %d: %s\n", error, description);
}

void set_no_auto_fetch() {
#ifdef _WIN32
    _putenv_s("SS_NO_AUTO_FETCH", "1");
#else
    setenv("SS_NO_AUTO_FETCH", "1", 1);
#endif
}

// X11 only. Windows finds the same icon by name in app.rc when GLFW registers
// the window class, and macOS raises GLFW_FEATURE_UNAVAILABLE here -- a window
// there has no icon of its own, the bundle does.
#if !defined(_WIN32) && !defined(__APPLE__)
void set_window_icon(GLFWwindow* window) {
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        kAppIcon, (int)kAppIconSize, &w, &h, &channels, 4);
    if (!pixels) return;
    GLFWimage image{ w, h, pixels };
    glfwSetWindowIcon(window, 1, &image);
    stbi_image_free(pixels);
}
#endif

// The DPI factor to lay out against. macOS sizes windows in points and scales
// them itself, so ImGui's DisplaySize already carries the scale and applying
// it again drew a 1440x900 Retina screen at 1.6 where Linux draws 0.9.
// Dividing it out is a no-op where a screen coordinate is a pixel. Glyphs are
// unaffected: ImGui 1.92 rasterizes at the framebuffer scale on its own.
float layout_dpi(GLFWwindow* window) {
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    int win_w = 0, win_h = 0, fb_w = 0, fb_h = 0;
    glfwGetWindowSize(window, &win_w, &win_h);
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    if (win_w > 0 && fb_w > 0) xscale *= (float)win_w / (float)fb_w;
    return xscale > 0.0f ? xscale : 1.0f;
}

#ifdef _WIN32
// Close the terminal the loader put behind the window.

bool is_console(HANDLE h) {
    DWORD mode = 0;
    return h != nullptr && h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode);
}

void reopen_null(FILE* stream, const char* mode) {
#ifdef _MSC_VER
    FILE* unused = nullptr;
    freopen_s(&unused, "NUL", mode, stream);
#else
    (void)std::freopen("NUL", mode, stream);
#endif
}

void release_own_console() {
    if (GetConsoleWindow() == nullptr) return;      // never had one
    DWORD pids[2];
    if (GetConsoleProcessList(pids, 2) != 1) return;  // shared: a shell's

    const bool had_out = is_console(GetStdHandle(STD_OUTPUT_HANDLE));
    const bool had_err = is_console(GetStdHandle(STD_ERROR_HANDLE));
    const bool had_in  = is_console(GetStdHandle(STD_INPUT_HANDLE));
    if (!FreeConsole()) return;
    if (had_out) reopen_null(stdout, "w");
    if (had_err) reopen_null(stderr, "w");
    if (had_in)  reopen_null(stdin, "r");
}
#endif  // _WIN32

// Frame pacing. Every presented frame is a GPU context switch away from a
// running trainer: left on vsync, a 144 Hz display cost about half the card,
// and iconifying the window -- which stops the presents -- gave it back.
constexpr double kFpsBusy = 60.0;
constexpr double kFpsIdle = 15.0;
constexpr double kFpsIconified = 10.0;
constexpr double kBusyHoldSeconds = 0.5;

// Input worth a fast frame rate. The named-key range covers the keyboard and
// the mouse buttons; analog sticks arrive as no event at all.
bool ui_busy() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f ||
        io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f ||
        ImGui::IsAnyItemActive())
        return true;
    for (ImGuiKey k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END;
         k = (ImGuiKey)(k + 1))
        if (ImGui::IsKeyDown(k)) return true;
    return gui::gamepad_deflected();
}

}  // namespace

int spirula_gui_main(int argc, char** argv) {
    // First, and before anything slow: the console is on screen until it goes.
#ifdef _WIN32
    release_own_console();
#endif
    // Before anything spawns a child, including the tools this binary re-runs
    // itself as: a Finder launch starts with almost nothing on PATH.
    app::add_desktop_search_paths();
    // The one process with no console and nobody reading its stderr.
    app::set_crash_dialog(true);
    // Checkpoints are fetched by the screen that asked for them, with a
    // progress bar and a button. Inherited by the children, so nothing this
    // window starts runs a curl the user did not press anything for.
    set_no_auto_fetch();

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::fprintf(stderr, "%s\n",
                     spirula::i18n::msg::log::err_glfw_init.get());
        return 1;
    }

    // GL 3.2 core: works on Linux/Windows and is the macOS minimum.
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    // 1600x950 is what the layout was designed against; a 1440x900 Mac has no
    // room for it, and the window would open larger than its own display.
    int win_w = 1600, win_h = 950;
    if (GLFWmonitor* mon = glfwGetPrimaryMonitor()) {
        int mx = 0, my = 0, mw = 0, mh = 0;
        glfwGetMonitorWorkarea(mon, &mx, &my, &mw, &mh);
        if (mw > 0) win_w = std::min(win_w, mw);
        if (mh > 0) win_h = std::min(win_h, mh);
    }
    // The title is set from the catalog below, once GuiApp has settled the
    // language; this is only what the window is born with.
    GLFWwindow* window = glfwCreateWindow(win_w, win_h, "Spirula Studio",
                                          nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "%s\n",
                     spirula::i18n::msg::log::err_window_create.get());
        glfwTerminate();
        return 1;
    }
#if !defined(_WIN32) && !defined(__APPLE__)
    set_window_icon(window);
#endif
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);   // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;   // fixed layout; nothing worth persisting
    // Every window here has a title bar, and one of them (the mask preview)
    // has a canvas whose shapes are dragged around: without this, pulling a
    // handle drags the window out from under the picture.
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    gui::apply_style(layout_dpi(window));

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    {
        gui::GuiApp app;
        app.set_dpi_scale(layout_dpi(window));
        // The atlas has to exist before the first frame, and the language it
        // depends on is only settled once GuiApp has read its settings file.
        app.fonts().ensure();
        // Drag-and-drop onto the window: auto-detect dataset folder / photo
        // folder / video file (fires on the main thread via glfwPollEvents).
        // The whole drop is handed over at once -- several videos dropped
        // together are the inputs of one dataset, not four separate answers.
        glfwSetWindowUserPointer(window, &app);
        glfwSetDropCallback(window, [](GLFWwindow* w, int count,
                                       const char** paths) {
            auto* a = (gui::GuiApp*)glfwGetWindowUserPointer(w);
            if (!a || !paths) return;
            std::vector<std::string> dropped;
            for (int i = 0; i < count; i++)
                if (paths[i] && paths[i][0]) dropped.push_back(paths[i]);
            if (!dropped.empty()) a->handle_drop(dropped);
        });
        // `spirula-gui <path>...` -- the same auto-detection as a drop, so a
        // desktop "Open with" or a shell alias lands on the right screen.
        {
            std::vector<std::string> args;
            for (int i = 1; i < argc; i++)
                if (argv[i] && argv[i][0]) args.push_back(argv[i]);
            if (!args.empty()) app.handle_drop(args);
        }
        // The title bar is interface copy too, and the language it is in is
        // only settled after GuiApp has read its settings -- and changes again
        // whenever the picker does. Comparing the string is enough of a
        // change test: it is immortal .rodata that the current language
        // selects, so this is one pointer-deep compare per frame.
        std::string title;
        auto sync_title = [&] {
            const char* want = spirula::i18n::msg::brand::window_title.get();
            if (title == want) return;
            title = want;
            glfwSetWindowTitle(window, title.c_str());
        };
        sync_title();

        double next_frame = 0.0, busy_until = 0.0;
        while (!app.wants_exit()) {
            glfwPollEvents();
            for (double wait; (wait = next_frame - glfwGetTime()) > 0.0; )
                glfwWaitEventsTimeout(wait);
            if (glfwWindowShouldClose(window)) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                app.request_close();
            }

            // Between frames, and before NewFrame: swapping a font face
            // invalidates every ImFont pointer, and this is the only point in
            // the loop where none is live. Returns immediately unless the
            // language changed or a font download finished.
            app.fonts().ensure();
            sync_title();
            // Dragging the window to a monitor with a different DPI is the
            // only thing that changes this, and it is three GLFW reads.
            app.set_dpi_scale(layout_dpi(window));

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            app.frame();
            const bool busy = ui_busy();
            ImGui::Render();

            const double now = glfwGetTime();
            if (busy) busy_until = now + kBusyHoldSeconds;
            const bool iconified =
                glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
            next_frame = now + 1.0 / (iconified        ? kFpsIconified
                                      : now < busy_until ? kFpsBusy
                                                         : kFpsIdle);

            int w = 0, h = 0;
            glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
            glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
        // Joins worker threads (finishing a final checkpoint save if a stop
        // was just requested) and frees GL resources while the context is
        // still current.
        app.shutdown();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
