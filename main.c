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
#include "sys/wait.h"
#include "pwd.h"
#include "limits.h"
#include "fcntl.h"
#include "sys/stat.h"

const int maxWindows = 12;
const int maxWorkspaces = 10;

typedef enum {
    LAYOUT_MASTER, LAYOUT_SUPERIOR_MASTER, LAYOUT_DWINDLE,
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
    Window windows[12];
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
    char* picom;
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

static inline Workspace getWorkspace() {
     return globals.workspaces[globals.currentWorkspace];
}

static inline Workspace getSpecificWorkspace(int index) {
     return globals.workspaces[index];
}

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
     
     if (*focusedWindowIndex >= numberOfWindows - 1) return;
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

    if (color != NULL && *color != 0) XFreeColors(globals.display, colormap, color, 1, 0);

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
     signal(SIGCHLD, SIG_IGN);

    pid_t pid = fork();

    if (pid < 0) {
        return;
    }

    if (pid == 0) {
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

void freeConfiguration() {
    free(globals.configuration.focusedBorderColorName);
    free(globals.configuration.unfocusedBorderColorName);
    free(globals.configuration.terminalName);
    free(globals.configuration.wallpaperPath);
    free(globals.configuration.launcher);
    free(globals.configuration.statusBar);
    free(globals.configuration.picom);
}



void freeStuff() {
    Colormap colormap = DefaultColormap(globals.display, globals.screen);    

    XFreeColors(globals.display, colormap, &globals.focusedBorderColor, 1, 0);
    XFreeColors(globals.display, colormap, &globals.unfocusedBorderColor, 1, 0);


    freeConfiguration();
}

void setExtraConfiguration() {
    bool failed = false;
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
}

bool loadLua(char* filePath) {
    globals.lua = luaL_newstate();

    if (globals.lua == NULL) {
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

        exit(1);
    }

    return true;
}

char* getLuaString(char* variable) {
    lua_getfield(globals.lua, -1, variable);
    if (lua_type(globals.lua, -1) != LUA_TSTRING) {
        fprintf(stderr, "Error: %s is not a string\n", variable);
        lua_pop(globals.lua, 1);
          
        exit(1);
    }

    char* value = (char*)lua_tostring(globals.lua, -1);
    lua_pop(globals.lua, 1);

    return strdup(value);
}

int getLuaInt(char* variable) {
    lua_getfield(globals.lua, -1, variable);
    if (!lua_isinteger(globals.lua, -1)) {
        fprintf(stderr, "Error: %s is not a integer\n", variable);
        lua_pop(globals.lua, 1);
    }

    int value = lua_tointeger(globals.lua, -1);
    lua_pop(globals.lua, 1);

    return value;
}


bool getLuaSpawnTable() {
     lua_getfield(globals.lua, -1, "spawn");
     if (!lua_istable(globals.lua, -1)) {
          fprintf(stderr, "Error: %s is not a table", "spawn");
          lua_pop(globals.lua, 1);
          
          return true;
     }

     char* terminalCommand = getLuaString("terminal");
     char* launcherCommand = getLuaString("launcher");

     if (globals.configuration.terminalName != NULL) free(globals.configuration.terminalName);
     if (globals.configuration.launcher != NULL) free(globals.configuration.launcher);

     globals.configuration.terminalName = terminalCommand;
     globals.configuration.launcher = launcherCommand;

     lua_pop(globals.lua, 1);
}

bool getLuaAutostartTable() {
     lua_getfield(globals.lua, -1, "autostart");
     if (!lua_istable(globals.lua, -1)) {
          fprintf(stderr, "Error: %s is not a table", "autostart");
          lua_pop(globals.lua, 1);
          return true;
     }

     char* wallpaperPathCommand = getLuaString("wallpaperPath");
     char* statusBarCommand = getLuaString("statusBar");
     char* picomCommand = getLuaString("picom");


     if (globals.configuration.wallpaperPath != NULL) free(globals.configuration.wallpaperPath);
     if (globals.configuration.statusBar != NULL) free(globals.configuration.statusBar);
     if (globals.configuration.picom != NULL) free(globals.configuration.picom);

     globals.configuration.wallpaperPath = wallpaperPathCommand;
     globals.configuration.statusBar = statusBarCommand;
     globals.configuration.picom = picomCommand;

     lua_pop(globals.lua, 1);
}

bool getLuaWindowTable() {
     lua_getfield(globals.lua, -1, "window");
     if (!lua_istable(globals.lua, -1)) {
          fprintf(stderr, "Error: %s is not a table", "window");
          lua_pop(globals.lua, 1);
          
          return true;
     }
     
     int borderSizeValue = getLuaInt("borderSize");
     int innerGapsValue = getLuaInt("innerGaps");
     int outerGapsValue = getLuaInt("outerGaps");
     char* focusedBorderColorNameValue = getLuaString("focusedBorderColorName");
     char* unfocusedBorderColorNameValue = getLuaString("unfocusedBorderColorName");


     if (globals.configuration.focusedBorderColorName != NULL) free(globals.configuration.focusedBorderColorName);
     if (globals.configuration.unfocusedBorderColorName != NULL) free(globals.configuration.unfocusedBorderColorName);

     globals.configuration.borderSize = borderSizeValue;
     globals.configuration.innerGaps = innerGapsValue;
     globals.configuration.outerGaps = outerGapsValue;
     globals.configuration.focusedBorderColorName = focusedBorderColorNameValue;
     globals.configuration.unfocusedBorderColorName = unfocusedBorderColorNameValue;

     lua_pop(globals.lua, 1);
}

void createDefaultCnnfigurationFile() {
    char* home = getpwuid(getuid())->pw_dir;
    char buffer[PATH_MAX];
    snprintf(buffer, PATH_MAX, "%s/.config/", home);

    chdir(buffer);
    mkdir("ultrawm", 0755);
    chdir("ultrawm");

    FILE* configuration = fopen("configuration.lua", "w");
    if (!configuration) {
         fprintf(stderr, "Error at opening configuration.lua\n");
         exit(1);
    }
     
     const char* defaultConfigurationString =
        "configuration = {\n"
        "    spawn = {\n"
        "        terminal = \"alacritty\",\n"
        "        launcher = \"rofi -show drun\",\n"
        "    },\n"
        "    autostart = {\n"
        "        wallpaperPath = \"\",\n"
        "        statusBar     = \"polybar\",\n"
        "        picom         = \"picom -b\",\n"
        "    },\n"
        "    window = {\n"
        "        focusedBorderColorName   = \"#ffffff\",\n"
        "        unfocusedBorderColorName = \"#111111\",\n"
        "        borderSize = 1,\n"
        "        innerGaps  = 10,\n"
        "        outerGaps  = 10,\n"
        "    },\n"
        "}\n";

    
    fprintf(configuration, "%s", defaultConfigurationString);

    fclose(configuration);

}

void readConfiguration() {
    defaultConfiguration();
     
    char* home = getpwuid(getuid())->pw_dir;
    char buffer[PATH_MAX];
    snprintf(buffer, PATH_MAX, "%s/.config/ultrawm/configuration.lua", home);
     
    if (!loadLua(buffer)) {
        createDefaultCnnfigurationFile();
        loadLua(buffer);
    }

    if (!getLuaTable("configuration")) {

        setExtraConfiguration();
        return;
    }

    getLuaSpawnTable();
    getLuaAutostartTable();
    getLuaWindowTable();

    lua_pop(globals.lua, 1);

    setExtraConfiguration();

    lua_close(globals.lua);
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
            globals.workspaces[workspace].focusedWindowIndex = globals.workspaces[workspace].numberOfWindows - 1;
            return;
        }
    }

    if (globals.workspaces[workspace].numberOfWindows > 0 && globals.workspaces[workspace].numberOfWindows < maxWindows) focusWindow(globals.workspaces[workspace].numberOfWindows - 1);
}

static void placeWindow(Window window, int x, int y, int width, int height) {
    int outerGaps = globals.configuration.outerGaps;
    int innerGaps = globals.configuration.innerGaps;

    XMoveResizeWindow (
              globals.display, 
              window, 
              outerGaps + x + innerGaps/2, 
              outerGaps + y + innerGaps/2, 
              width - innerGaps, 
              height - innerGaps
    );
}

void masterLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    if (numberOfWindows == 0) return;

    int outerGaps = globals.configuration.outerGaps;
    int screenWidth = globals.screenWidth  - 2 * outerGaps;
    int screenHeight = globals.screenHeight - 2 * outerGaps;

    if (numberOfWindows == 1) { placeWindow(windows[0], 0, 0, screenWidth, screenHeight); return; }

    int midWidth = screenWidth / 2;
    int each     = screenHeight / (numberOfWindows - 1);
    placeWindow(windows[0], 0, 0, midWidth, screenHeight);

    for (int i = 1; i < numberOfWindows; i++) {
        placeWindow(windows[i], midWidth, (i - 1) * each, screenWidth - midWidth, each);
    }

    XSync(globals.display, False);
}

void superiorMasterLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    if (numberOfWindows == 0) return;

    int outerGaps = globals.configuration.outerGaps;
    int screenWidth = globals.screenWidth  - 2 * outerGaps;
    int screenHeight = globals.screenHeight - 2 * outerGaps;

    if (numberOfWindows == 1) { placeWindow(windows[0], 0, 0, screenWidth, screenHeight); return; }

    int midHeight = screenHeight / 2;
    int each      = screenWidth / (numberOfWindows - 1);
    placeWindow(windows[0], 0, 0, screenWidth, midHeight);

    for (int i = 1; i < numberOfWindows; i++)
        placeWindow(windows[i], (i - 1) * each, midHeight, each, screenHeight - midHeight);

    XSync(globals.display, False);
}

void dwindleLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    if (numberOfWindows == 0) return;

    int outerGaps = globals.configuration.outerGaps;
    int screenWidth = globals.screenWidth  - 2 * outerGaps;
    int screenHeight = globals.screenHeight - 2 * outerGaps;

    if (numberOfWindows == 1) { placeWindow(windows[0], 0, 0, screenWidth, screenHeight); return; }

    int x = 0;
    int y = 0;
    int width = screenWidth;
    int height = screenHeight;

    for (int i = 0; i < numberOfWindows - 1; i++) {
        if (i % 2 == 0) {
            int halfWidth = width / 2;
            placeWindow(windows[i], x, y, halfWidth, height);

            x += halfWidth; 
            width -= halfWidth;
        } 
        else {
            int halfHeight = height / 2;
            placeWindow(windows[i], x, y, width, halfHeight);

            y += halfHeight; 
            height -= halfHeight;
        }
    }
    placeWindow(windows[numberOfWindows-1], x, y, width, height);
    XSync(globals.display, False);
}

void fibonacciLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;
    if (numberOfWindows == 0) return;

    int outerGaps = globals.configuration.outerGaps;
    int screenWidth = globals.screenWidth  - 2*outerGaps;
    int screenHeight = globals.screenHeight - 2*outerGaps;

    if (numberOfWindows == 1) { 
         placeWindow(windows[0], 0, 0, screenWidth, screenHeight); 
         return; 
    }

    int x = 0;
    int y = 0;
    int width = screenWidth; 
    int height = screenHeight;

    for (int i = 0; i < numberOfWindows - 1; i++) {
        switch (i % 4) {
        case 0: { 
               int halfWidth = width / 2; 
               placeWindow(windows[i], x, y, halfWidth, height);        
               x += halfWidth; 
               width -= halfWidth; 
               
               break; 
        }
        case 1: { 
               int halfHeight = height / 2; 
               placeWindow(windows[i], x, y, width, halfHeight);        
               y += halfHeight; 
               height -= halfHeight; 
               
               break; 
        }
        case 2: { 
                int halfWidth = width / 2; 
                placeWindow(windows[i], x + width - halfWidth, y, halfWidth, height);             
                width -= halfWidth; 
                break; 
        }
        case 3: { 
               int halfHeight = height/2; 
               placeWindow(windows[i], x, y + height - halfHeight, width, halfHeight);             
               height -= halfHeight; 

               break; 
        }
        }
    }
    placeWindow(windows[numberOfWindows - 1], x, y, width, height);
    XSync(globals.display, False);
}

void scrollLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;

    int outerGaps = globals.configuration.outerGaps;
    int innerGaps = globals.configuration.innerGaps;

    int screenWidth = globals.screenWidth  - 2*outerGaps;
    int screenHeight = globals.screenHeight - 2*outerGaps;


    if (numberOfWindows == 1) {
         placeWindow(windows[0], 0, 0, screenWidth, screenHeight);
         return;
    }

    int focusedWindowIndex = globals.workspaces[globals.currentWorkspace].focusedWindowIndex;
    int halfHeight = screenHeight / 2;
    
    if (focusedWindowIndex % 3 != 0) {
          if (focusedWindowIndex - 1 >= 0) placeWindow(windows[focusedWindowIndex - 1], 0, halfHeight, globals.screenWidth, halfHeight);
          placeWindow(windows[focusedWindowIndex], 0, 0, screenWidth, halfHeight);
    }
    else {
          placeWindow(windows[focusedWindowIndex], 0, halfHeight, screenWidth, halfHeight);
          if (focusedWindowIndex + 1 < numberOfWindows) placeWindow(windows[focusedWindowIndex + 1], 0, 0, globals.screenWidth, halfHeight);

    }



    for (int i = 0; i < numberOfWindows; i++) {
          if (i == focusedWindowIndex) continue;
          if ((focusedWindowIndex % 3 != 0) && i == focusedWindowIndex - 1) continue;
          if ((focusedWindowIndex % 3 == 0) && i == focusedWindowIndex + 1) continue;
          XMoveResizeWindow(globals.display, windows[i], screenWidth + 10, screenHeight, 1, 1);
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
}

void loadKeys() {
    XSelectInput(globals.display, globals.root, SubstructureRedirectMask | SubstructureNotifyMask);
     
    for (int i = 0; i < ACTION_KEY_SIZE; i++) {
          XGrabKey(
                    globals.display, 
                    XKeysymToKeycode(globals.display, keys[i].sym), 
                    (keys[i].mod == MOD_SUPER) ? Mod4Mask : Mod1Mask, 
                    globals.root, 
                    True, 
                    GrabModeAsync, 
                    GrabModeAsync
          );
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

void autostart() {
    spawnStatusBar();
    spawnPicom();
    spawnWallpaper();
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
    autostart();

    mainLoop();

    freeStuff();
    return 0;
}
