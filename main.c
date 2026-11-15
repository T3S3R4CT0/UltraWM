#include "stdio.h"
#include "string.h"
#include "X11/Xlib.h"
#include "X11/keysym.h"
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

typedef enum {
     ACTION_KEY_SPAWN_TERMINAL,
     ACTION_KEY_KILL_WINDOW,
     ACTION_KEY_PREVIOUS_WINDOW,
     ACTION_KEY_NEXT_WINDOW,
     ACTION_KEY_SET_WORKSPACE_1,
     ACTION_KEY_SET_WORKSPACE_2,
     ACTION_KEY_SET_WORKSPACE_3,
     ACTION_KEY_SET_WORKSPACE_4,
     ACTION_KEY_SET_WORKSPACE_5,
     ACTION_KEY_SET_WORKSPACE_6,
     ACTION_KEY_SET_WORKSPACE_7,
     ACTION_KEY_SET_WORKSPACE_9,
     ACTION_KEY_SET_WORKSPACE_8,
     ACTION_KEY_SET_WORKSPACE_0,
     ACTION_KEY_SET_MASTER_LAYOUT,
     ACTION_KEY_SET_SUPERIOR_MASTER_LAYOUT,
     ACTION_KEY_SET_DWINDLE_LAYOUT,
     ACTION_KEY_SET_FIBONACCI_LAYOUT,
     ACTiON_KEY_SET_SCROLL_LAYOUT,
     ACTION_KEY_RELOAD_CONFIGURATION,
     ACTION_KEY_SPAWN_LAUNCHER,
     ACTION_KEY_EXIT,
     ACTION_KEY_TOGGLE_FULLSCREEN,
     ACTION_KEY_SIZE
} ActionKey;

typedef enum {
     MOD_SUPER = Mod4Mask,
     MOD_ALT = Mod1Mask
} Mod;

typedef struct {
     KeySym sym;
     void (*action)(void);
     Mod mod;
} Key;

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
    bool isFullscren;
} Globals;

static Globals globals;

void spawnTerminal();
void killClient();
void switchWorkspace(int workspace);
void switchLayout(Layout layout);
void readConfiguration();
void spawnLauncher();
void toggleFullscreen();
void tile();
void focusWindow(int index);

void actionSpawnTerminal() {
     spawnTerminal();
}

void actionKillWindow() {
     killClient();
}

void actionPreviousWindow() {
     int* focusedWindowIndex = &globals.workspaces[globals.currentWorkspace].focusedWindowIndex;

     if (*focusedWindowIndex <= 0) return;
     (*focusedWindowIndex)--;
     focusWindow(*focusedWindowIndex);

     tile();
}

void actionNextWindow() {
      int* focusedWindowIndex = &globals.workspaces[globals.currentWorkspace].focusedWindowIndex;
     int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;
     
     if (*focusedWindowIndex >= numberOfWindows) return;
     (*focusedWindowIndex)++;
     focusWindow(*focusedWindowIndex);

     tile();
}

void actionSetWorkspace1() {
     switchWorkspace(1);
}

void actionSetWorkspace2() {
     switchWorkspace(2);
}

void actionSetWorkspace3() {
     switchWorkspace(3);
}

void actionSetWorkspace4() {
     switchWorkspace(4);
}

void actionSetWorkspace5() {
     switchWorkspace(5);
}

void actionSetWorkspace6() {
     switchWorkspace(6);
}

void actionSetWorkspace7() {
     switchWorkspace(7);
}

void actionSetWorkspace8() {
     switchWorkspace(8);
}

void actionSetWorkspace9() {
     switchWorkspace(9);
}

void actionSetWorkspace0() {
     switchWorkspace(0);
}

void actionSetMasterLayout() {
     switchLayout(LAYOUT_MASTER);
}

void actionSetSuperiorMasterLayout() {
     switchLayout(LAYOUT_SUPERIOR_MASTER);
}

void actionSetDwindleLayout() {
     switchLayout(LAYOUT_DWINDLE);
}

void actionSetFibonacciLayout() {
     switchLayout(LAYOUT_FIBONACCI);
}

void actionSetScrollLayout() {
     switchLayout(LAYOUT_SCROLL);
}

void actionReloadConfiguration() {
     readConfiguration();
}

void actionSpawnLauncher() {
     spawnLauncher();
}

void actionExit() {
     globals.running = false;
}

void actionToggleFullScreen() {
     toggleFullscreen();
}

static Key keys[] = {
     (Key) { //spawn terminal
          .sym = XK_Return,
          .mod = MOD_SUPER,
          .action = actionSpawnTerminal
     },

     (Key) { //kill window
          .sym = XK_q,
          .mod = MOD_SUPER,
          .action = actionKillWindow
     },

     (Key) { //previous window
          .sym = XK_j,
          .mod = MOD_SUPER,
          .action = actionPreviousWindow
     },

     (Key) { //next window
          .sym = XK_k,
          .mod = MOD_SUPER,
          .action = actionNextWindow
     },

     (Key) { //set workspace 1
          .sym = XK_1,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace1
     },

     (Key) { //set workspace 2
          .sym = XK_2,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace2
     },

     (Key) { //set workspace 3
          .sym = XK_3,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace3
     },

     (Key) { //set workspace 4
          .sym = XK_4,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace4
     },

     (Key) { //set workspace 5
          .sym = XK_5,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace5
     },

     (Key) { //set workspace 6
          .sym = XK_6,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace6
     },

     (Key) { //set workspace 7
          .sym = XK_7,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace7
     },
     
     (Key) { //set workspace 8
          .sym = XK_8,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace8
     },

     (Key) { //set workspace 9
          .sym = XK_9,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace9
     },

     (Key) { //set workspace 0
          .sym = XK_0,
          .mod = MOD_SUPER,
          .action = actionSetWorkspace0
     },

     (Key) { //set  master layout
          .sym = XK_1,
          .mod = MOD_ALT,
          .action = actionSetMasterLayout
     },

     (Key) { //set superior master layout
          .sym = XK_2,
          .mod = MOD_ALT,
          .action = actionSetSuperiorMasterLayout
     },

     (Key) { //set dwindle layout
          .sym = XK_3,
          .mod = MOD_ALT,
          .action = actionSetDwindleLayout
     },

     (Key) { //set fibonacci layout
          .sym = XK_4,
          .mod = MOD_ALT,
          .action = actionSetFibonacciLayout
     },

     (Key) { //set scroll layout
          .sym = XK_5,
          .mod = MOD_ALT,
          .action = actionSetScrollLayout
     },


     (Key) { //reload configuration
          .sym = XK_r,
          .mod = MOD_SUPER,
          .action = actionReloadConfiguration
     },

     (Key) { //spawn launcher
          .sym = XK_space,
          .mod = MOD_SUPER,
          .action = actionSpawnLauncher
     },

     (Key) {
          .sym = XK_e, //exit session
          .mod = MOD_SUPER,
          .action = actionExit
     },

     (Key) {
          .sym = XK_F11, //toggle full screen
          .mod = MOD_SUPER,
          .action = actionToggleFullScreen
     }
};

Key getKey() {
     return (Key) {
          .sym = XLookupKeysym(&globals.event.xkey, 0),
          .mod = globals.event.xkey.state & (Mod4Mask | Mod1Mask | ShiftMask | ControlMask)
     };
}

bool compareKeys(Key a, Key b) {
     return (a.sym == b.sym && a.mod == b.mod);
}

void toggleFullscreen();

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

void scrollLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;

    if (numberOfWindows == 1) {
         placeWindow(windows[0], 0, 0, globals.screenWidth, globals.screenHeight);
         return;
    }

    int focusedWindowIndex = globals.workspaces[globals.currentWorkspace].focusedWindowIndex;
    int halfHeight = globals.screenHeight / 2;
     

    if (focusedWindowIndex - 1 >= 0) placeWindow(windows[focusedWindowIndex - 1], 0, halfHeight, globals.screenWidth, halfHeight);
    placeWindow(windows[focusedWindowIndex], 0, 0, globals.screenWidth, halfHeight);

    for (int i = 0; i < numberOfWindows; i++) {
          if (i == focusedWindowIndex || i == focusedWindowIndex - 1) continue;
          XMoveResizeWindow(globals.display, windows[i], globals.screenWidth + 10, globals.screenHeight, 1, 1);
     }
         
    XSync(globals.display, false);
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
    case LAYOUT_SCROLL:
        scrollLayout();
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

     if (globals.isFullscren) {
          toggleFullscreen();
          globals.workspaces[globals.currentWorkspace].focusedWindowIndex = 0;
     }

}

void toggleFullscreen() {
     Window window = globals.workspaces[globals.currentWorkspace].windows[globals.workspaces[globals.currentWorkspace].focusedWindowIndex];

     if (!globals.isFullscren) {

          for (int i = 0; i < maxWindows; i++) {
               Window current = globals.workspaces[globals.currentWorkspace].windows[i];
               XUnmapWindow(globals.display, current);
          }
          
          XMoveResizeWindow(globals.display, window, 0, 0, globals.screenWidth, globals.screenHeight);
          XMapWindow(globals.display, window);

          globals.isFullscren = true;

          return;
     }
     
     XUnmapWindow(globals.display, window);
     tile();

     for (int i = 0; i < maxWindows; i++) {
          Window current = globals.workspaces[globals.currentWorkspace].windows[i];
          XMapWindow(globals.display, current);
     }

     globals.isFullscren = false;
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

    spawnStatusBar();
}

void loadKeys() {
    XSelectInput(globals.display, globals.root, SubstructureRedirectMask | SubstructureNotifyMask);
     
    for (int i = 0; i < ACTION_KEY_SIZE; i++) {
          XGrabKey(globals.display, XKeysymToKeycode(globals.display, keys[i].sym), (keys[i].mod == MOD_SUPER) ? Mod4Mask : Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
    }
        

    XSync(globals.display, False);
}

void mapRequest() {
    if (globals.isFullscren) toggleFullscreen();

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

void keyboardInput() {
    int* index = &globals.workspaces[globals.currentWorkspace].focusedWindowIndex;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;

    Key key = getKey();
     
    for (int i = 0; i < ACTION_KEY_SIZE; i++) {
         if (compareKeys(key, keys[i])) {
              keys[i].action();
              break;
         }
    }
}

void handleKeys() {
    keyboardInput();
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

    spawnStatusBar();
    mainLoop();

    freeStuff();
    return 0;
}
