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
     int borderSize;
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
     for (int i = 0; i < globals.workspaces[globals.currentWorkspace].numberOfWindows; i++) {
          Window window = globals.workspaces[globals.currentWorkspace].windows[i];

          XSetWindowBorderWidth(globals.display, window, globals.configuration.borderSize);
     }
}

void freeStuff() {
     lua_close(globals.lua);
}

void defaultConfiguration() {
     globals.configuration.focusedBorderColorName = "#ffffff";
     globals.configuration.unfocusedBorderColorName = "#444444";
     globals.configuration.terminalName = "alacritty";
     globals.configuration.wallpaperPath = "";
     globals.configuration.launcher = NULL;
     globals.configuration.borderSize = 1;

}

void readConfiguration() {
     bool failed = false;
     char errorBuffer[1024];

     printf("aaa\n");
     fflush(stdout);


     globals.lua = luaL_newstate();
     if (globals.lua == NULL) {
          fprintf(stderr, "Error: Could not open lua\n");
          exit(1);
     }

     luaL_openlibs(globals.lua);

     if (luaL_dofile(globals.lua, "/home/mateokb/.config/ultrawm/configuration.lua") != LUA_OK) {
          defaultConfiguration();
          return;
     }

     lua_getglobal(globals.lua, "configuration");
     if (!lua_istable(globals.lua, -1)) {
          fprintf(stderr, "Error: Configuration is not a table");
          exit(1);
     }

     lua_getfield(globals.lua, -1, "focusedBorderColorName");
     if (lua_type(globals.lua, -1) != LUA_TSTRING) {
          fprintf(stderr, "Error: focusedBorderColorName is not a string");
          exit(1);
     }

     globals.configuration.focusedBorderColorName = (char*)lua_tostring(globals.lua, -1);
     lua_pop(globals.lua, 1);

     lua_getfield(globals.lua, -1, "unfocusedBorderColorName");
     if (lua_type(globals.lua, -1) != LUA_TSTRING) {
          fprintf(stderr, "Error: unfocusedBorderColorName is not a string\n");
          exit(1);
     }

     globals.configuration.unfocusedBorderColorName = (char*)lua_tostring(globals.lua, -1);
     lua_pop(globals.lua, 1);

     lua_getfield(globals.lua, -1, "terminalName");
     if (lua_type(globals.lua, -1) != LUA_TSTRING) {
          fprintf(stderr, "Error: terminalName is not a string\n");
          exit(1);
     }

     globals.configuration.terminalName = (char*)lua_tostring(globals.lua, -1);
     lua_pop(globals.lua, 1);

     lua_getfield(globals.lua, -1, "wallpaperPath");
     if (lua_type(globals.lua, -1) != LUA_TSTRING) {
          fprintf(stderr, "Error: wallpaperPath is not a string\n");
          exit(1);
     }

     globals.configuration.wallpaperPath = (char*)lua_tostring(globals.lua, -1);
     lua_pop(globals.lua, 1);

     lua_getfield(globals.lua, -1, "launcher");
     if (lua_type(globals.lua, -1) != LUA_TSTRING) {
          fprintf(stderr, "Error: launcher is not a string\n");
          exit(1);
     }

     globals.configuration.launcher = (char*)lua_tostring(globals.lua, -1);
     lua_pop(globals.lua, 1);
 
     lua_getfield(globals.lua, -1, "borderSize");
     if (!lua_isinteger(globals.lua, -1)) {
          fprintf(stderr, "Error: borderSize is not a integer\n");
          exit(1);
     }

     globals.configuration.borderSize = lua_tointeger(globals.lua, -1);
     lua_pop(globals.lua, 1);
     
     allocateColor(globals.configuration.focusedBorderColorName, &globals.focusedBorderColor, &failed, errorBuffer); if (failed) {
          fprintf(stderr, "Error: %s\n", errorBuffer);
          exit(1);
     }
     allocateColor(globals.configuration.unfocusedBorderColorName, &globals.unfocusedBorderColor, &failed, errorBuffer); if (failed) {
          fprintf(stderr, "Error: %s\n", errorBuffer);
          exit(1);
     }

     setWindowsBorderColor();
     setWindowsBorderSize();
}

int xerror(Display* display, XErrorEvent* error) {
     if (error->error_code == BadAccess) {
          fprintf(stderr, "Error: Ya hay otro WM en ejecucion");
          exit(1);
     }

     fprintf(stderr, "Error: %d\n", error->error_code);
     return 0;
}

bool isManaged(Window window) {
     int workspace = globals.currentWorkspace;
     for (int i = 0; i < globals.workspaces[workspace].numberOfWindows; i++) {
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
}

void addWindow(Window window) {
     int workspace = globals.currentWorkspace;
     int numberOfWindows = globals.workspaces[workspace].numberOfWindows;
     
     printf("%d\n", numberOfWindows);
     fflush(stdout);
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


void masterLayout() {
     Window* windows = globals.workspaces[globals.currentWorkspace].windows;
     int numberOfClients = globals.workspaces[globals.currentWorkspace].numberOfWindows;

     if (numberOfClients == 0) return;
     if (numberOfClients == 1) {
          XMoveResizeWindow(globals.display, windows[0], 0, 0, globals.screenWidth, globals.screenHeight);
          return;
     }

     int midWidth = globals.screenWidth / 2;
     int each = globals.screenHeight / (numberOfClients - 1);
     XMoveResizeWindow(globals.display, windows[0], 0, 0, midWidth, globals.screenHeight);

     for (int i = 1; i < numberOfClients; i++) {
          XMoveResizeWindow(globals.display, windows[i], midWidth, (i - 1) * each, globals.screenWidth - midWidth, each);
     }

     XSync(globals.display, False);

}

void superiorMasterLayout() {
     Window* windows = globals.workspaces[globals.currentWorkspace].windows;
     int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;

     if (numberOfWindows == 0) return;
     if (numberOfWindows == 1) {
          XMoveResizeWindow(globals.display, windows[0], 0, 0, globals.screenWidth, globals.screenHeight);
          return;
     }

     int midHeight = globals.screenHeight / 2;
     int each = globals.screenWidth / (numberOfWindows - 1);
     
     XMoveResizeWindow(globals.display, windows[0], 0, 0, globals.screenWidth, midHeight);

     for (int i = 1; i < numberOfWindows; i++) {
          XMoveResizeWindow(globals.display, windows[i], (i - 1) * each, midHeight, each, globals.screenHeight - midHeight);
     }

     XSync(globals.display, False);
}

void dwindleLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;

    if (numberOfWindows == 0) return;
    if (numberOfWindows == 1) {
        XMoveResizeWindow(globals.display, windows[0],
                          0, 0, globals.screenWidth, globals.screenHeight);
        return;
    }

    int x = 0, y = 0;
    int w = globals.screenWidth;
    int h = globals.screenHeight;

    int halfW = w / 2;
    int halfH = h / 2;

    for (int i = 0; i < numberOfWindows - 1; i++) {
        int halfW = w / 2;
        int halfH = h / 2;

        if (i % 2 == 0) {
            XMoveResizeWindow(globals.display, windows[i], x, y, halfW, h);
            x += halfW;
            w -= halfW;
        } else {
            int halfH = h / 2;
            XMoveResizeWindow(globals.display, windows[i], x, y, w, halfH);
            y += halfH;
            h -= halfH;
        }
    }

    XMoveResizeWindow(globals.display, windows[numberOfWindows - 1], x, y, w, h);

    XSync(globals.display, False);
}

void fibonacciLayout() {
    Window* windows = globals.workspaces[globals.currentWorkspace].windows;
    int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;

    if (numberOfWindows == 0) return;
    if (numberOfWindows == 1) {
        XMoveResizeWindow(globals.display, windows[0],
                          0, 0, globals.screenWidth, globals.screenHeight);
        return;
    }

    int x = 0, y = 0;
    int w = globals.screenWidth;
    int h = globals.screenHeight;

    for (int i = 0; i < numberOfWindows - 1; i++) {
        int dir = i % 4;
        switch (dir) {
            case 0: { 
                int halfW = w / 2;
                XMoveResizeWindow(globals.display, windows[i], x, y, halfW, h);
                x += halfW;
                w -= halfW;
                break;
            }
            case 1: {
                int halfH = h / 2;
                XMoveResizeWindow(globals.display, windows[i], x, y, w, halfH);
                y += halfH;
                h -= halfH;
                break;
            }
            case 2: {
                int halfW = w / 2;
                XMoveResizeWindow(globals.display, windows[i], x + w - halfW, y, halfW, h);
                w -= halfW;
                break;
            }
            case 3: { 
                int halfH = h / 2;
                XMoveResizeWindow(globals.display, windows[i], x, y + h - halfH, w, halfH);
                h -= halfH;
                break;
            }
        }
    }

    XMoveResizeWindow(globals.display, windows[numberOfWindows - 1], x, y, w, h);

    XSync(globals.display, False);
}

void scrollLayout() {

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
     switchWorkspace(globals.currentWorkspace);
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

void spawnTerminal() {
     spawn((char*[]){globals.configuration.terminalName, NULL});
}

void spawnLauncher() {
     spawn((char*[]){"rofi", "rofi", "-show", "drun", NULL});
}

void spawnWallpaper() {
     spawn((char*[]){"feh", "--bg-scale", globals.configuration.wallpaperPath, NULL});
}

void killClient() {
     int numberOfWindows = globals.workspaces[globals.currentWorkspace].numberOfWindows;
     Window* windows = globals.workspaces[globals.currentWorkspace].windows;

     if (numberOfWindows > 0) {
          Window target = windows[numberOfWindows - 1];
          if (target != None) {
               XKillClient(globals.display, target);
          }
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

     globals.display = XOpenDisplay(NULL); if (!globals.display) {
          fprintf(stderr, "Error: No se pudo abrir el display\n");
          exit(1);
     }

     globals.screen = DefaultScreen(globals.display);

     globals.screenWidth = DisplayWidth(globals.display, globals.screen);
     globals.screenHeight = DisplayHeight(globals.display, globals.screen);

     globals.root = DefaultRootWindow(globals.display);
     XSetErrorHandler(xerror);

     globals.currentWorkspace = 1;

     readConfiguration();
}

void loadKeys() {
     XSelectInput(globals.display, globals.root, SubstructureRedirectMask | SubstructureNotifyMask);

     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_Return), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_q), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_j), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_k), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_1), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_2), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_3), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_4), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_5), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_6), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_7), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_8), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_9), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_0), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_C), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_V), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_B), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_N), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);
     XGrabKey(globals.display, XKeysymToKeycode(globals.display, XK_R), Mod1Mask, globals.root, True, GrabModeAsync, GrabModeAsync);


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

     uint32_t state = globals.event.xkey.state & (Mod4Mask |  Mod1Mask | ShiftMask | ControlMask);

     if (sym == XK_Return && state == Mod1Mask) {
          spawnTerminal();
     }
     else if (sym == XK_q && state == Mod1Mask) {
          killClient();
     }
     else if (sym == XK_j && state == Mod1Mask) {
          if (*index >= numberOfWindows - 1) return;
          focusWindow(++(*index));
     }
     else if (sym == XK_k && state == Mod1Mask) {
          if (*index < 0) return;
          focusWindow(--(*index));
     }
     else if (sym == XK_1 && state == Mod1Mask) {
          switchWorkspace(1);
     }
     else if (sym == XK_2 && state == Mod1Mask) {
          switchWorkspace(2);
     }
     else if (sym == XK_3 && state == Mod1Mask) {
          switchWorkspace(3);
     }
     else if (sym == XK_4 && state == Mod1Mask) {
          switchWorkspace(4);
     }
     else if (sym == XK_5 && state == Mod1Mask) {
          switchWorkspace(5);
     }
     else if (sym == XK_6 && state == Mod1Mask) {
          switchWorkspace(6);
     }
     else if (sym == XK_7 && state == Mod1Mask) {
          switchWorkspace(7);
     }
     else if (sym == XK_8 && state == Mod1Mask) {
          switchWorkspace(8);
     }
     else if (sym == XK_9 && state == Mod1Mask) {
          switchWorkspace(9);
     }
     else if (sym == XK_0 && state == Mod1Mask) {
          switchWorkspace(0);
     }
     else if (sym == XK_c && state == Mod1Mask) {
          switchLayout(LAYOUT_MASTER);
     }
     else if (sym == XK_v && state == Mod1Mask) {
          switchLayout(LAYOUT_SUPERIOR_MASTER);
     }
     else if (sym == XK_b && state == Mod1Mask) {
          switchLayout(LAYOUT_DWINDLE);
     }
     else if (sym == XK_n && state == Mod1Mask) {
          switchLayout(LAYOUT_FIBONACCI);
     }
     else if (sym == XK_r && state == Mod1Mask) {
          readConfiguration();
     }

}

void mainLoop() {
     while (true) {
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
     mainLoop();
     
     freeStuff();
     return 0;
}
