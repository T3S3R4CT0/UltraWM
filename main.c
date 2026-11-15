#include "stdio.h"
#include "string.h"
#include "X11/Xlib.h"
#include "X11/keysym.h"
#include "X11/Xatom.h"
#include "stdlib.h"
#include "unistd.h"
#include "signal.h"
#include "stdint.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

const int maxWindows = 12;
const int maxWorkspaces = 10;

typedef enum {
    LAYOUT_MASTER,
    LAYOUT_SUPERIOR_MASTER,
    LAYOUT_DWINDLE,
    LAYOUT_FIBONACCI,
    LAYOUT_SCROLL,
} Layout;

typedef struct {
    Window windows[36];
    size_t numberOfWindows;
    int focusedWindowIndex;
    Layout layout;
} Workspace;

typedef struct {
    char* focusedBorderColorName;
    char* unfocusedBorderColorName;
    char* terminalName;
    char* wallpaperPath;
    char* launcher;
    char* statusBar;
    int borderSize;
    int innerGaps;
    int outerGaps;
} Configuration;

typedef struct {
    Workspace workspaces[10];
    Configuration configuration;
    lua_State* lua;
    Display* display;
    XEvent event;
    Window root;
    unsigned long focusedBorderColor;
    unsigned long unfocusedBorderColor;
    int currentWorkspace;
    int screen;
    int screenWidth;
    int screenHeight;
    int borderWidth;
    bool running;
} Globals;

static Globals globals;

void allocateColor(char* name, unsigned long* color, bool* failed, char errorBuffer[1024]) {
    XColor screenColor;
    XColor exactColor;

    Colormap colormap = DefaultColormap(globals.display, globals.screen);

    if (!XAllocNamedColor(globals.display, colormap, name, &screenColor, &exactColor)) {
        strncpy(errorBuffer, "Could not allocate color", 1024);
        *failed = true;
        return;
    }

    *color = screenColor.pixel;
}

void setWindowsBorderColor() {
    const int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    const int focusedWindowIndex = globals.workspaces[globals.currentWorkspace].focusedWindowIndex;

    for (int i = 0; i < numberOfWindows; i++) {
        Window window = globals.workspaces[globals.currentWorkspace].windows[i];

        if (i == focusedWindowIndex) {
            XSetWindowBorder(globals.display, window, globals.focusedBorderColor);
        }
        else {
            XSetWindowBorder(globals.display, window, globals.unfocusedBorderColor);
        }
    }

    XFlush(globals.display);
}

void setWindowsBorderSize() {
    for (size_t i = 0; i < globals.workspaces[globals.currentWorkspace].numberOfWindows; i++) {
        Window window = globals.workspaces[globals.currentWorkspace].windows[i];

        XSetWindowBorderWidth(globals.display, window, globals.configuration.borderSize);
    }
}

void spawn(char* args[]) {
    pid_t pid = fork();

    if (pid < 0) {
        return;
    }

    if (pid == 0) {
        char* display = XDisplayString(globals.display);

        if (display == NULL ||
            setenv("DISPLAY", display, 1) == -1 ||
            setenv("WAYLAND_DISPLAY", "", 1) == -1) {
            perror("setenv");
            _exit(127);
        }

        setenv("DISPLAY", display, 1);

        execvp(args[0], args);
        _exit(1);
    }
}

void spawnPicom() {
     spawn((char*[]){"picom", "-b", NULL});
}

void spawnTerminal() {
    spawn((char*[]){globals.configuration.terminalName, NULL});
}

void spawnLauncher() {
    spawn((char*[]){"sh", "-c", globals.configuration.launcher, NULL});
}

void spawnWallpaper() {
    spawn((char*[]){"feh", "--bg-scale", globals.configuration.wallpaperPath, NULL});
}

void spawnStatusBar() {
    spawn((char*[]){"sh", "-c", globals.configuration.statusBar, NULL});
}

void freeStuff() {
    lua_close(globals.lua);
}

void setExtraConfiguration() {
    bool failed;
    char errorBuffer[1024];

    allocateColor(globals.configuration.focusedBorderColorName, &globals.focusedBorderColor, &failed, errorBuffer);
    if (failed) {
        fprintf(stderr, "Error: %s\n", errorBuffer);
        exit(1);
    }

    allocateColor(globals.configuration.unfocusedBorderColorName, &globals.unfocusedBorderColor, &failed, errorBuffer);
    if (failed) {
        fprintf(stderr, "Error: %s\n", errorBuffer);
        exit(1);
    }

    setWindowsBorderColor();
    setWindowsBorderSize();
    spawnWallpaper();
    spawnStatusBar();
    spawnPicom();
}

void defaultConfiguration() {
    globals.configuration.focusedBorderColorName = "#ffffff";
    globals.configuration.unfocusedBorderColorName = "#444444";
    globals.configuration.terminalName = "alacritty";
    globals.configuration.wallpaperPath = "";
    globals.configuration.launcher = NULL;
    globals.configuration.borderSize = 1;
    globals.configuration.statusBar = "";
    globals.configuration.innerGaps = 0;
    globals.configuration.outerGaps = 0;
}

bool loadLua(char* filePath) {
    globals.lua = luaL_newstate();
    if (globals.lua == NULL) {
        fprintf(stderr, "Error: Could not open lua\n");
        return false;
    }

    luaL_openlibs(globals.lua);

    if (luaL_dofile(globals.lua, filePath) != LUA_OK) {
        return false;
    }

    return true;
}

bool getLuaTable(char* table) {
    lua_getglobal(globals.lua, table);
    if (!lua_istable(globals.lua, -1)) {
        fprintf(stderr, "Error: %s is not a table\n", table);
        lua_pop(globals.lua, 1);
        return false;
    }

    return true;
}

char* getLuaString(char* variable, char* fallback) {
    lua_getfield(globals.lua, -1, variable);
    if (lua_type(globals.lua, -1) != LUA_TSTRING) {
        fprintf(stderr, "Error: %s is not a string\n", variable);
        lua_pop(globals.lua, 1);
        return fallback;
    }

    char* value = (char*)lua_tostring(globals.lua, -1);
    lua_pop(globals.lua, 1);

    return value;
}

int getLuaInt(char* variable, int fallback) {
    lua_getfield(globals.lua, -1, variable);
    if (!lua_isinteger(globals.lua, -1)) {
        fprintf(stderr, "Error: %s is not a integer\n", variable);
        lua_pop(globals.lua, 1);
        return fallback;
    }

    int value = lua_tointeger(globals.lua, -1);
    lua_pop(globals.lua, 1);

    return value;
}

void readConfiguration() {
    defaultConfiguration();

    if (!loadLua("/home/mateokb/.config/ultrawm/configuration.lua")) {
        setExtraConfiguration();
        return;
    }

    if (!getLuaTable("configuration")) {
        setExtraConfiguration();
        return;
    }

    globals.configuration.focusedBorderColorName = getLuaString("focusedBorderColorName", globals.configuration.focusedBorderColorName);
    globals.configuration.unfocusedBorderColorName = getLuaString("unfocusedBorderColorName", globals.configuration.unfocusedBorderColorName);
    globals.configuration.terminalName = getLuaString("terminalName", globals.configuration.terminalName);
    globals.configuration.wallpaperPath = getLuaString("wallpaperPath", globals.configuration.wallpaperPath);
    globals.configuration.launcher = getLuaString("launcher", globals.configuration.launcher);
    globals.configuration.statusBar = getLuaString("statusBar", globals.configuration.statusBar);
    globals.configuration.borderSize = getLuaInt("borderSize", globals.configuration.borderSize);
    globals.configuration.innerGaps = getLuaInt("innerGaps", globals.configuration.innerGaps);
    globals.configuration.outerGaps = getLuaInt("outerGaps", globals.configuration.outerGaps);

    lua_pop(globals.lua, 1);

    setExtraConfiguration();
}

int xerror(Display* display, XErrorEvent* error) {
    if (error->error_code == BadAccess) {
        fprintf(stderr, "Error: Ya hay otro WM en ejecucion ");
        exit(1);
    }

    fprintf(stderr, "Error: %d\n", error->error_code);
    return 0;
}

bool isManaged(Window window) {
    int workspace = globals.currentWorkspace;
    for (size_t i = 0; i < globals.workspaces[workspace].numberOfWindows; i++) {
        if (globals.workspaces[workspace].windows[i] == window) return true;
    }

    return false;
}

void focusWindow(int index) {
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    if (index < 0 || index >= numberOfWindows) return;

    int workspace = globals.currentWorkspace;
    int window = globals.workspaces[globals.currentWorkspace].windows[index];

    globals.workspaces[workspace].focusedWindowIndex = index;
    int focused = globals.workspaces[workspace].focusedWindowIndex;

    XSetInputFocus(globals.display, globals.workspaces[workspace].windows[focused], RevertToPointerRoot, CurrentTime);
    XRaiseWindow(globals.display, window);
    setWindowsBorderColor();

    printf("%d\n", focused);
}

void addWindow(Window window) {
    int workspace = globals.currentWorkspace;
    int numberOfWindows = globals.workspaces[workspace].numberOfWindows;

    globals.workspaces[workspace].windows[globals.workspaces[workspace].numberOfWindows++] = window;
}

void removeWindow(Window window) {
    int workspace = globals.currentWorkspace;
    int numberOfClients = globals.workspaces[workspace].numberOfWindows;
    Window* windows = globals.workspaces[workspace].windows;

    for (size_t i = 0; i < numberOfClients; i++) {
        if (windows[i] == window) {
            for (size_t j = i; j + 1 < numberOfClients; j++) {
                windows[j] = windows[j + 1];
            }

            globals.workspaces[workspace].numberOfWindows--;
            return;
        }
    }

    if (globals.workspaces[workspace].numberOfWindows > 0 && globals.workspaces[workspace].numberOfWindows < maxWindows) focusWindow(globals.workspaces[workspace].numberOfWindows - 1);
}

static void placeWindow(Window win, int x, int y, int w, int h) {
    int o = globals.configuration.outerGaps;
    int i = globals.configuration.innerGaps;
    XMoveResizeWindow(globals.display, win, o + x + i/2, o + y + i/2, w - i, h - i);
}

void masterLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int n = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    if (n == 0) return;

    int o = globals.configuration.outerGaps;
    int W = globals.screenWidth  - 2*o;
    int H = globals.screenHeight - 2*o;

    if (n == 1) { placeWindow(windows[0], 0, 0, W, H); return; }

    int midWidth = W / 2;
    int each     = H / (n - 1);
    placeWindow(windows[0], 0, 0, midWidth, H);
    for (int i = 1; i < n; i++)
        placeWindow(windows[i], midWidth, (i-1)*each, W - midWidth, each);

    XSync(globals.display, False);
}

void superiorMasterLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int n = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    if (n == 0) return;

    int o = globals.configuration.outerGaps;
    int W = globals.screenWidth  - 2*o;
    int H = globals.screenHeight - 2*o;

    if (n == 1) { placeWindow(windows[0], 0, 0, W, H); return; }

    int midHeight = H / 2;
    int each      = W / (n - 1);
    placeWindow(windows[0], 0, 0, W, midHeight);
    for (int i = 1; i < n; i++)
        placeWindow(windows[i], (i-1)*each, midHeight, each, H - midHeight);

    XSync(globals.display, False);
}

void dwindleLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int n = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    if (n == 0) return;

    int o = globals.configuration.outerGaps;
    int W = globals.screenWidth  - 2*o;
    int H = globals.screenHeight - 2*o;

    if (n == 1) { placeWindow(windows[0], 0, 0, W, H); return; }

    int x = 0, y = 0, w = W, h = H;
    for (int i = 0; i < n - 1; i++) {
        if (i % 2 == 0) {
            int halfW = w / 2;
            placeWindow(windows[i], x, y, halfW, h);
            x += halfW; w -= halfW;
        } else {
            int halfH = h / 2;
            placeWindow(windows[i], x, y, w, halfH);
            y += halfH; h -= halfH;
        }
    }
    placeWindow(windows[n-1], x, y, w, h);
    XSync(globals.display, False);
}

void fibonacciLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int n = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    if (n == 0) return;

    int o = globals.configuration.outerGaps;
    int W = globals.screenWidth  - 2*o;
    int H = globals.screenHeight - 2*o;

    if (n == 1) { placeWindow(windows[0], 0, 0, W, H); return; }

    int x = 0, y = 0, w = W, h = H;
    for (int i = 0; i < n - 1; i++) {
        switch (i % 4) {
        case 0: { int hw = w/2; placeWindow(windows[i], x, y, hw, h);        x += hw; w -= hw; break; }
        case 1: { int hh = h/2; placeWindow(windows[i], x, y, w, hh);        y += hh; h -= hh; break; }
        case 2: { int hw = w/2; placeWindow(windows[i], x+w-hw, y, hw, h);             w -= hw; break; }
        case 3: { int hh = h/2; placeWindow(windows[i], x, y+h-hh, w, hh);             h -= hh; break; }
        }
    }
    placeWindow(windows[n-1], x, y, w, h);
    XSync(globals.display, False);
}

void tile() {
    switch (globals.workspaces[globals.currentWorkspace].layout) {
    case LAYOUT_MASTER:
        masterLayout();
        break;

    case LAYOUT_SUPERIOR_MASTER:
        superiorMasterLayout();
        break;

    case LAYOUT_DWINDLE:
        dwindleLayout();
        break;

    case LAYOUT_FIBONACCI:
        fibonacciLayout();
        break;

    default:
        break;
    }
}

void switchWorkspace(int workspace) {
    for (int i = 0; i < globals.workspaces[globals.currentWorkspace].numberOfWindows; i++) {
        int window = globals.workspaces[globals.currentWorkspace].windows[i];
        XUnmapWindow(globals.display, window);
    }

    globals.currentWorkspace = workspace;

    for (int i = 0; i < globals.workspaces[globals.currentWorkspace].numberOfWindows; i++) {
        int window = globals.workspaces[globals.currentWorkspace].windows[i];
        XMapWindow(globals.display, window);
    }

    globals.workspaces[globals.currentWorkspace].focusedWindowIndex = 0;

    tile();
}

void switchLayout(Layout layout) {
    globals.workspaces[globals.currentWorkspace].layout = layout;
    tile();
}

void killClient() {
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;

    if (numberOfWindows < 0) return;

    if (numberOfWindows >= 0) {
        int target = globals.workspaces[globals.currentWorkspace].focusedWindowIndex;
        XKillClient(globals.display, globals.workspaces[globals.currentWorkspace].windows[target]);
    }
}

void destroy(Window deadWindow) {
    Window* focusedWindow = &globals.workspaces[globals.currentWorkspace].windows[globals.workspaces[globals.currentWorkspace].focusedWindowIndex];
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;

    removeWindow(deadWindow);

    bool wasFocused = (deadWindow == *focusedWindow);

    if (wasFocused) {
        if (numberOfWindows > 0) {
            focusWindow(globals.workspaces[globals.currentWorkspace].focusedWindowIndex);
        }
    }

    if (globals.workspaces[globals.currentWorkspace].numberOfWindows <= 0) {
        globals.workspaces[globals.currentWorkspace].focusedWindowIndex = 0;
    }

    tile();
}

void loadGlobals() {
    bool failed = false;
    char errorBuffer[1024];

    globals.display = XOpenDisplay(NULL);
    if (!globals.display) {
        fprintf(stderr, "Error: No se pudo abrir el display\n");
        exit(1);
    }

    globals.screen = DefaultScreen(globals.display);

    globals.screenWidth = DisplayWidth(globals.display, globals.screen);
    globals.screenHeight = DisplayHeight(globals.display, globals.screen);

    globals.root = DefaultRootWindow(globals.display);
    XSetErrorHandler(xerror);

    globals.currentWorkspace = 1;
    globals.running = true;
}

void loadKeys() {
    XSelectInput(globals.display, globals.root, SubstructureRedirectMask | SubstructureNotifyMask);

    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_Return), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_q), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_j), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_k), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_1), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_2), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_3), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_4), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_5), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_6), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_7), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_8), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_9), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_0), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_1), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_2), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_3), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_4), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_R), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_space), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_n), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_e), Mod4Mask, globals.root, True, GrabModeAsync, GrabModeAsync);

    XSync(globals.display, False);
}

void mapRequest() {
    Window window = globals.event.xmaprequest.window;

    int numberOfClients = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    int* index = &globals.workspaces[globals.currentWorkspace].focusedWindowIndex;

    if (numberOfClients >= maxWindows) return;

    addWindow(window);
    *index = numberOfClients;
    if (*index < 0) *index = 0;

    XMapWindow(globals.display, globals.event.xmaprequest.window);
    focusWindow(*index);
    tile();

    XSetWindowBorderWidth(globals.display, window, globals.configuration.borderSize);
}

void configureRequest() {
    XConfigureRequestEvent* configureRequuestEvent = &globals.event.xconfigurerequest;

    if (isManaged(configureRequuestEvent->window)) {
        tile();
    }
    else {
        XWindowChanges windowChanges = (XWindowChanges) {
            .x = configureRequuestEvent->x,
            .y = configureRequuestEvent->y,
            .width = configureRequuestEvent->width,
            .height = configureRequuestEvent->height,
            .border_width = configureRequuestEvent->border_width,
            .sibling = configureRequuestEvent->above,
            .stack_mode = configureRequuestEvent->detail
        };

        XConfigureWindow(globals.display, configureRequuestEvent->window, configureRequuestEvent->value_mask, &windowChanges);
    }
}

void handleKeys() {
    int* index = &globals.workspaces[globals.currentWorkspace].focusedWindowIndex;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;

    KeySym sym = XLookupKeysym(&globals.event.xkey, 0);

    uint32_t state = globals.event.xkey.state & (Mod4Mask | Mod1Mask | ShiftMask | ControlMask);

    if (sym == XK_Return && state == Mod4Mask) {
        spawnTerminal();
    }
    else if (sym == XK_q && state == Mod4Mask) {
        killClient();
    }
    else if (sym == XK_j && state == Mod4Mask) {
        if (*index >= numberOfWindows - 1) return;
        focusWindow(++(*index));
    }
    else if (sym == XK_k && state == Mod4Mask) {
        if (*index < 0) return;
        focusWindow(--(*index));
    }
    else if (sym == XK_1 && state == Mod4Mask) {
        switchWorkspace(1);
    }
    else if (sym == XK_2 && state == Mod4Mask) {
        switchWorkspace(2);
    }
    else if (sym == XK_3 && state == Mod4Mask) {
        switchWorkspace(3);
    }
    else if (sym == XK_4 && state == Mod4Mask) {
        switchWorkspace(4);
    }
    else if (sym == XK_5 && state == Mod4Mask) {
        switchWorkspace(5);
    }
    else if (sym == XK_6 && state == Mod4Mask) {
        switchWorkspace(6);
    }
    else if (sym == XK_7 && state == Mod4Mask) {
        switchWorkspace(7);
    }
    else if (sym == XK_8 && state == Mod4Mask) {
        switchWorkspace(8);
    }
    else if (sym == XK_9 && state == Mod4Mask) {
        switchWorkspace(9);
    }
    else if (sym == XK_0 && state == Mod4Mask) {
        switchWorkspace(0);
    }
    else if (sym == XK_1 && state == Mod1Mask) {
        switchLayout(LAYOUT_MASTER);
    }
    else if (sym == XK_2 && state == Mod1Mask) {
        switchLayout(LAYOUT_SUPERIOR_MASTER);
    }
    else if (sym == XK_3 && state == Mod1Mask) {
        switchLayout(LAYOUT_DWINDLE);
    }
    else if (sym == XK_4 && state == Mod1Mask) {
        switchLayout(LAYOUT_FIBONACCI);
    }
    else if (sym == XK_r && state == Mod4Mask) {
        readConfiguration();
    }
    else if (sym == XK_space && state == Mod4Mask) {
        spawnLauncher();
    }
    else if (sym == XK_e && state == Mod4Mask) {
         globals.running = false;
    }

}

void mainLoop() {
    while (globals.running) {
        XNextEvent(globals.display, &globals.event);

        switch (globals.event.type) {
        case MapRequest:
            mapRequest();
            break;

        case DestroyNotify:
            destroy(globals.event.xdestroywindow.window);
            break;

        case ConfigureRequest:
            configureRequest();
            break;

        case KeyPress:
            handleKeys();
            break;

        default:
            break;
        }
    }
}

int main() {
    loadGlobals();
    loadKeys();
    readConfiguration();
    mainLoop();

    freeStuff();
    return 0;
}
