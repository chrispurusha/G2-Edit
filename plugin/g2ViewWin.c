/*
 * The G2 Editor application.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
// Notes: Docs/code-notes/g2ViewWin.c.md - "// notes §k" refers there.

// notes §1

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <pthread.h>

#include <GLFW/glfw3.h>    // the GL declarations, and the GLFW key codes SynthLib's popups take

#include "renderBackend.h"
#include "defs.h"          // WHEEL_SCROLL_STEP
#include "g2Draw.h"
#include "g2View.h"
#include "g2Input.h"
#include "inputState.h"
#include "soundEngine.h"

#define TIMER_METER       (1u)
#define TIMER_TICK        (2u)
#define METER_MS          (50u)
#define TICK_MS           (16u)
#define MAX_VIEWS         (64)

typedef struct {
    HWND   hwnd;
    HDC    dc;
    HGLRC  rc;
    void * doc;          // the instance's tG2Document; owned by g2Plugin.c
    bool   ticking;
    bool   tracking;     // TrackMouseEvent armed for the WM_MOUSELEAVE
    int    lastX;
    int    lastY;
} tG2Win;

static pthread_mutex_t gViewsLock    = PTHREAD_MUTEX_INITIALIZER;
static HWND            gViews[MAX_VIEWS];
static tG2Win *        gLastDrawn    = NULL;
static wchar_t         gClassName[64] = {0};
static HINSTANCE       gModule        = NULL;
static HWND            gRootWindow    = NULL;
static HDC             gRootDc        = NULL;
static HGLRC           gRootRc        = NULL;

// ------------------------------------------------------------------------------------------------
// The pointer
// ------------------------------------------------------------------------------------------------

// notes §2
static bool gCursorHidden = false;

bool cursor_is_captured(void) {
    return gCursorHidden;
}

void cursor_capture(void) {
    if (gCursorHidden) {
        return;
    }
    gCursorHidden = true;

    while (ShowCursor(FALSE) >= 0) {
    }
}

void cursor_release(void) {
    if (gCursorHidden == false) {
        return;
    }
    gCursorHidden = false;

    while (ShowCursor(TRUE) < 0) {
    }
}

// ------------------------------------------------------------------------------------------------
// OpenGL
// ------------------------------------------------------------------------------------------------

static bool set_pixel_format(HDC dc) {
    PIXELFORMATDESCRIPTOR pfd = {0};

    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(dc, &pfd);

    return (format != 0) && (SetPixelFormat(dc, format, &pfd) == TRUE);
}

static LRESULT CALLBACK view_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// notes §3
static bool register_class(void) {
    if (gClassName[0] != 0) {
        return true;
    }
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)(void *)&register_class, &gModule);
    swprintf(gClassName, sizeof(gClassName) / sizeof(gClassName[0]), L"G2AlikeEditor%p", (void *)gModule);

    WNDCLASSEXW wc = {0};

    wc.cbSize        = sizeof(wc);
    wc.style         = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc   = view_proc;
    wc.hInstance     = gModule;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = gClassName;

    if (RegisterClassExW(&wc) == 0) {
        gClassName[0] = 0;
        return false;
    }
    return true;
}

// notes §4
static bool ensure_root_context(void) {
    if (gRootRc != NULL) {
        return true;
    }
    gRootWindow = CreateWindowExW(0, gClassName, L"", WS_POPUP, 0, 0, 1, 1, NULL, NULL, gModule, NULL);

    if (gRootWindow == NULL) {
        return false;
    }
    gRootDc = GetDC(gRootWindow);

    if (set_pixel_format(gRootDc) == false) {
        return false;
    }
    gRootRc = wglCreateContext(gRootDc);
    return gRootRc != NULL;
}

static void make_current(tG2Win * w) {
    if (wglGetCurrentContext() != w->rc) {
        wglMakeCurrent(w->dc, w->rc);
    }
}

// ------------------------------------------------------------------------------------------------
// Drawing
// ------------------------------------------------------------------------------------------------

static void enter(tG2Win * w) {
    g2_draw_enter(w->doc);
    make_current(w);
}

static void draw_now(tG2Win * w) {
    RECT r = {0};

    enter(w);
    gLastDrawn = w;
    GetClientRect(w->hwnd, &r);
    g2_draw_frame((int)(r.right - r.left), (int)(r.bottom - r.top), 1.0);
    SwapBuffers(w->dc);
}

// The click-region registry is one per process and refilled by every frame, so it holds whichever
// editor drew last - draw this one first if that was another
static void enter_for_input(tG2Win * w) {
    enter(w);

    if (gLastDrawn != w) {
        draw_now(w);
    }
}

static void redraw(tG2Win * w) {
    InvalidateRect(w->hwnd, NULL, FALSE);
}

// ------------------------------------------------------------------------------------------------
// Input
// ------------------------------------------------------------------------------------------------

static void push_modifiers(void) {
    uint32_t bits = (uint32_t)eModifierNone;

    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
        bits |= (uint32_t)eModifierShift;
    }

    if (((GetKeyState(VK_LWIN) & 0x8000) != 0) || ((GetKeyState(VK_RWIN) & 0x8000) != 0)) {
        bits |= (uint32_t)eModifierCmd;    // as GLFW maps them for the application
    }

    if ((GetKeyState(VK_MENU) & 0x8000) != 0) {
        bits |= (uint32_t)eModifierAlt;
    }

    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        bits |= (uint32_t)eModifierCtrl;
    }
    set_modifier_state(bits);
}

static void ensure_tick(tG2Win * w) {
    if (w->ticking == false) {
        w->ticking = true;
        SetTimer(w->hwnd, TIMER_TICK, TICK_MS, NULL);
    }
}

static int glfw_key_for_vk(WPARAM vk) {
    switch (vk) {
        case VK_ESCAPE: return GLFW_KEY_ESCAPE;
        case VK_RETURN: return GLFW_KEY_ENTER;
        case VK_BACK:   return GLFW_KEY_BACKSPACE;
        case VK_TAB:    return GLFW_KEY_TAB;
        case VK_DELETE: return GLFW_KEY_DELETE;
        case VK_LEFT:   return GLFW_KEY_LEFT;
        case VK_RIGHT:  return GLFW_KEY_RIGHT;
        case VK_UP:     return GLFW_KEY_UP;
        case VK_DOWN:   return GLFW_KEY_DOWN;
        case VK_HOME:   return GLFW_KEY_HOME;
        case VK_END:    return GLFW_KEY_END;
        case VK_PRIOR:  return GLFW_KEY_PAGE_UP;
        case VK_NEXT:   return GLFW_KEY_PAGE_DOWN;
        default:        return GLFW_KEY_UNKNOWN;
    }
}

// notes §5
static int shortcut_char_for_vk(WPARAM vk) {
    if ((vk >= 'A') && (vk <= 'Z')) {
        return (int)(vk - 'A' + 'a');
    }

    if ((vk >= '0') && (vk <= '9')) {
        return (int)vk;
    }

    switch (vk) {
        case VK_OEM_PLUS:  return '=';
        case VK_OEM_MINUS: return '-';
        case VK_ADD:       return '+';
        case VK_SUBTRACT:  return '-';
        default:           return 0;
    }
}

static void track_leave(tG2Win * w) {
    if (w->tracking) {
        return;
    }
    TRACKMOUSEEVENT tme = {0};

    tme.cbSize    = sizeof(tme);
    tme.dwFlags   = TME_LEAVE;
    tme.hwndTrack = w->hwnd;
    w->tracking   = (TrackMouseEvent(&tme) == TRUE);
}

static bool any_button_down(void) {
    return ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
}

// notes §6
static void recover_lost_release(tG2Win * w) {
    if ((cursor_is_captured() == false) || any_button_down()) {
        return;
    }
    g2_input_mouse_event((double)w->lastX, (double)w->lastY, eClickRelease);
    redraw(w);
}

// ------------------------------------------------------------------------------------------------
// The window
// ------------------------------------------------------------------------------------------------

static LRESULT CALLBACK view_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    tG2Win * w = (tG2Win *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (w == NULL) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_ERASEBKGND:
            return 1;    // the frame covers everything; erasing first only flickers

        case WM_PAINT: {
            PAINTSTRUCT ps;

            BeginPaint(hwnd, &ps);
            draw_now(w);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
            redraw(w);
            return 0;

        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTARROWS;   // a host's dialog must not take them

        case WM_MOUSEACTIVATE:
            SetFocus(hwnd);    // the click that focuses the window acts too
            return MA_ACTIVATE;

        case WM_TIMER:
            if (wParam == TIMER_METER) {
                enter(w);
                (void)sound_engine_meters_dirty();

                if (sound_engine_active() == true) {
                    redraw(w);
                }
            } else if (wParam == TIMER_TICK) {
                enter_for_input(w);
                recover_lost_release(w);

                if (g2_input_drag_tick() == true) {
                    redraw(w);
                } else {
                    KillTimer(hwnd, TIMER_TICK);
                    w->ticking = false;
                }
            }
            return 0;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            enter_for_input(w);
            SetFocus(hwnd);
            SetCapture(hwnd);
            push_modifiers();
            w->lastX = GET_X_LPARAM(lParam);
            w->lastY = GET_Y_LPARAM(lParam);
            g2_input_mouse_event((double)w->lastX, (double)w->lastY, eClickPress);
            ensure_tick(w);
            redraw(w);
            return 0;

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            enter_for_input(w);
            track_leave(w);
            push_modifiers();

            if ((wParam & MK_LBUTTON) != 0) {
                if (cursor_is_captured()) {
                    g2_input_drag_by((double)(x - w->lastX), (double)(y - w->lastY));
                } else {
                    g2_input_mouse_event((double)x, (double)y, eClickDrag);
                }
            } else {
                g2_input_hover((double)x, (double)y);
                ensure_tick(w);
            }
            w->lastX = x;
            w->lastY = y;
            redraw(w);
            return 0;
        }

        case WM_LBUTTONUP:
            enter_for_input(w);
            ReleaseCapture();
            push_modifiers();
            w->lastX = GET_X_LPARAM(lParam);
            w->lastY = GET_Y_LPARAM(lParam);
            g2_input_mouse_event((double)w->lastX, (double)w->lastY, eClickRelease);
            ensure_tick(w);
            redraw(w);
            return 0;

        case WM_RBUTTONUP:
            enter_for_input(w);
            push_modifiers();
            g2_input_right_click((double)GET_X_LPARAM(lParam), (double)GET_Y_LPARAM(lParam));
            ensure_tick(w);
            redraw(w);
            return 0;

        case WM_MOUSELEAVE:
            w->tracking = false;

            if (any_button_down() == false) {
                enter(w);
                g2_input_pointer_left();
                redraw(w);
            }
            return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            POINT  p     = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};    // screen coordinates
            double notch = (double)GET_WHEEL_DELTA_WPARAM(wParam) / (double)WHEEL_DELTA;

            ScreenToClient(hwnd, &p);
            enter_for_input(w);
            push_modifiers();

            if (msg == WM_MOUSEWHEEL) {
                g2_input_scroll((double)p.x, (double)p.y, 0.0, notch * WHEEL_SCROLL_STEP);
            } else {
                g2_input_scroll((double)p.x, (double)p.y, -notch * WHEEL_SCROLL_STEP, 0.0);
            }
            redraw(w);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            bool repeat = ((lParam & (1L << 30)) != 0);
            int  key    = glfw_key_for_vk(wParam);

            enter_for_input(w);
            push_modifiers();

            if ((key != GLFW_KEY_UNKNOWN) && g2_input_popup_key_glfw(key, 0u, repeat)) {
                redraw(w);
                return 0;
            }

            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                int c = shortcut_char_for_vk(wParam);

                if ((c != 0) && g2_input_key(c, true)) {    // notes §5 - Ctrl stands in for Cmd
                    redraw(w);
                    return 0;
                }
            }
            break;    // a character, if it is one, follows as WM_CHAR
        }

        case WM_CHAR: {
            unsigned int c = (unsigned int)wParam;

            if ((c < 0x20u) || ((GetKeyState(VK_CONTROL) & 0x8000) != 0)) {
                return 0;    // control characters, and Ctrl shortcuts, were dealt with above
            }
            enter_for_input(w);

            if (g2_input_popup_key_glfw(GLFW_KEY_UNKNOWN, c, (lParam & (1L << 30)) != 0)
               || g2_input_key((int)c, false)) {
                redraw(w);
            }
            return 0;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP:
            push_modifiers();    // Alt or Shift let go with the pointer still
            break;

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ------------------------------------------------------------------------------------------------
// The plug-in's side
// ------------------------------------------------------------------------------------------------

void g2_view_request_redraw(void) {
    pthread_mutex_lock(&gViewsLock);

    for (int i = 0; i < MAX_VIEWS; i++) {
        if (gViews[i] != NULL) {
            InvalidateRect(gViews[i], NULL, FALSE);    // safe from any thread: it only posts WM_PAINT
        }
    }
    pthread_mutex_unlock(&gViewsLock);
}

static void views_add(HWND hwnd, bool add) {
    pthread_mutex_lock(&gViewsLock);

    for (int i = 0; i < MAX_VIEWS; i++) {
        if (add && (gViews[i] == NULL)) {
            gViews[i] = hwnd;
            break;
        }

        if ((add == false) && (gViews[i] == hwnd)) {
            gViews[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&gViewsLock);
}

// notes §7
void * g2_view_create(void * doc, double width, double height) {
    if ((register_class() == false) || (ensure_root_context() == false)) {
        return NULL;
    }
    tG2Win * w = (tG2Win *)calloc(1, sizeof(tG2Win));

    if (w == NULL) {
        return NULL;
    }
    w->doc  = doc;
    w->hwnd = CreateWindowExW(0, gClassName, L"G2 Alike", WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                              0, 0, (int)width, (int)height, NULL, NULL, gModule, NULL);

    if (w->hwnd == NULL) {
        free(w);
        return NULL;
    }
    w->dc = GetDC(w->hwnd);

    if ((set_pixel_format(w->dc) == false) || ((w->rc = wglCreateContext(w->dc)) == NULL)
       || (wglShareLists(gRootRc, w->rc) == FALSE)) {
        if (w->rc != NULL) {
            wglDeleteContext(w->rc);
        }
        ReleaseDC(w->hwnd, w->dc);
        DestroyWindow(w->hwnd);
        free(w);
        return NULL;
    }
    g2_draw_enter(doc);    // before g2_draw_init(), which sets up the current document's top bar
    make_current(w);
    gfx_backend_choose(eRenderBackendOpenGL);
    gfx_attach_window((void *)w->hwnd);
    g2_draw_init();

    SetWindowLongPtrW(w->hwnd, GWLP_USERDATA, (LONG_PTR)w);
    SetTimer(w->hwnd, TIMER_METER, METER_MS, NULL);
    views_add(w->hwnd, true);
    return (void *)w->hwnd;
}

void g2_view_destroy(void * view) {
    HWND     hwnd = (HWND)view;
    tG2Win * w    = (tG2Win *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    views_add(hwnd, false);

    if (w == NULL) {
        DestroyWindow(hwnd);
        return;
    }
    KillTimer(hwnd, TIMER_METER);
    KillTimer(hwnd, TIMER_TICK);

    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
    cursor_release();    // an editor closed mid-drag must not leave the host without a pointer
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);

    if (gLastDrawn == w) {
        gLastDrawn = NULL;
    }
    gfx_detach_window((void *)hwnd);

    if (wglGetCurrentContext() == w->rc) {
        wglMakeCurrent(NULL, NULL);
    }
    wglDeleteContext(w->rc);    // its textures live on in the root context it shares with
    ReleaseDC(hwnd, w->dc);
    DestroyWindow(hwnd);
    free(w);
}
