/*
Copyright 2010 Jared Krinke.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <SDL.h>

#include "r_event_keys.h"
#include "r_assert.h"

static const char *key_names[SDLK_LAST];

r_status_t r_event_keys_init()
{
    int i;

    /* Initialize all key names (just in case some are not given constants) */
    for (i = 0; i < SDLK_LAST; i++)
    {
        key_names[i] = NULL;
    }

    /* Name the keys that have known names */
    key_names[SDLK_UNKNOWN] = "?";
    key_names[SDLK_BACKSPACE] = "backspace";
    key_names[SDLK_TAB] = "tab";
    key_names[SDLK_CLEAR] = "clear";
    key_names[SDLK_RETURN] = "enter";
    key_names[SDLK_PAUSE] = "pause";
    key_names[SDLK_ESCAPE] = "escape";
    key_names[SDLK_SPACE] = "space";
    key_names[SDLK_EXCLAIM] = "!";
    key_names[SDLK_QUOTEDBL] = "\"";
    key_names[SDLK_HASH] = "#";
    key_names[SDLK_DOLLAR] = "$";
    key_names[SDLK_AMPERSAND] = "&";
    key_names[SDLK_QUOTE] = "'";
    key_names[SDLK_LEFTPAREN] = "(";
    key_names[SDLK_RIGHTPAREN] = ")";
    key_names[SDLK_ASTERISK] = "*";
    key_names[SDLK_PLUS] = "+";
    key_names[SDLK_COMMA] = ",";
    key_names[SDLK_MINUS] = "-";
    key_names[SDLK_PERIOD] = ".";
    key_names[SDLK_SLASH] = "/";
    key_names[SDLK_0] = "0";
    key_names[SDLK_1] = "1";
    key_names[SDLK_2] = "2";
    key_names[SDLK_3] = "3";
    key_names[SDLK_4] = "4";
    key_names[SDLK_5] = "5";
    key_names[SDLK_6] = "6";
    key_names[SDLK_7] = "7";
    key_names[SDLK_8] = "8";
    key_names[SDLK_9] = "9";
    key_names[SDLK_COLON] = ":";
    key_names[SDLK_SEMICOLON] = ";";
    key_names[SDLK_LESS] = "<";
    key_names[SDLK_EQUALS] = "=";
    key_names[SDLK_GREATER] = ">";
    key_names[SDLK_QUESTION] = "?";
    key_names[SDLK_AT] = "@";
    key_names[SDLK_LEFTBRACKET] = "[";
    key_names[SDLK_BACKSLASH] = "\\";
    key_names[SDLK_RIGHTBRACKET] = "]";
    key_names[SDLK_CARET] = "^";
    key_names[SDLK_UNDERSCORE] = "_";
    key_names[SDLK_BACKQUOTE] = "`";
    key_names[SDLK_a] = "a";
    key_names[SDLK_b] = "b";
    key_names[SDLK_c] = "c";
    key_names[SDLK_d] = "d";
    key_names[SDLK_e] = "e";
    key_names[SDLK_f] = "f";
    key_names[SDLK_g] = "g";
    key_names[SDLK_h] = "h";
    key_names[SDLK_i] = "i";
    key_names[SDLK_j] = "j";
    key_names[SDLK_k] = "k";
    key_names[SDLK_l] = "l";
    key_names[SDLK_m] = "m";
    key_names[SDLK_n] = "n";
    key_names[SDLK_o] = "o";
    key_names[SDLK_p] = "p";
    key_names[SDLK_q] = "q";
    key_names[SDLK_r] = "r";
    key_names[SDLK_s] = "s";
    key_names[SDLK_t] = "t";
    key_names[SDLK_u] = "u";
    key_names[SDLK_v] = "v";
    key_names[SDLK_w] = "w";
    key_names[SDLK_x] = "x";
    key_names[SDLK_y] = "y";
    key_names[SDLK_z] = "z";
    key_names[SDLK_DELETE] = "delete";
    key_names[SDLK_KP0] = "kp-0";
    key_names[SDLK_KP1] = "kp-1";
    key_names[SDLK_KP2] = "kp-2";
    key_names[SDLK_KP3] = "kp-3";
    key_names[SDLK_KP4] = "kp-4";
    key_names[SDLK_KP5] = "kp-5";
    key_names[SDLK_KP6] = "kp-6";
    key_names[SDLK_KP7] = "kp-7";
    key_names[SDLK_KP8] = "kp-8";
    key_names[SDLK_KP9] = "kp-9";
    key_names[SDLK_KP_PERIOD] = "kp-period";
    key_names[SDLK_KP_DIVIDE] = "kp-div";
    key_names[SDLK_KP_MULTIPLY] = "kp-mult";
    key_names[SDLK_KP_MINUS] = "kp-min";
    key_names[SDLK_KP_PLUS] = "kp-plus";
    key_names[SDLK_KP_ENTER] = "kp-ent";
    key_names[SDLK_KP_EQUALS] = "kp-eq";
    key_names[SDLK_UP] = "up";
    key_names[SDLK_DOWN] = "down";
    key_names[SDLK_RIGHT] = "right";
    key_names[SDLK_LEFT] = "left";
    key_names[SDLK_INSERT] = "insert";
    key_names[SDLK_HOME] = "home";
    key_names[SDLK_END] = "end";
    key_names[SDLK_PAGEUP] = "pgup";
    key_names[SDLK_PAGEDOWN] = "pgdn";
    key_names[SDLK_F1] = "F1";
    key_names[SDLK_F2] = "F2";
    key_names[SDLK_F3] = "F3";
    key_names[SDLK_F4] = "F4";
    key_names[SDLK_F5] = "F5";
    key_names[SDLK_F6] = "F6";
    key_names[SDLK_F7] = "F7";
    key_names[SDLK_F8] = "F8";
    key_names[SDLK_F9] = "F9";
    key_names[SDLK_F10] = "F10";
    key_names[SDLK_F11] = "F11";
    key_names[SDLK_F12] = "F12";
    key_names[SDLK_F13] = "F13";
    key_names[SDLK_F14] = "F14";
    key_names[SDLK_F15] = "F15";
    key_names[SDLK_HELP] = "help";
    key_names[SDLK_PRINT] = "print";
    key_names[SDLK_SYSREQ] = "sysreq";
    key_names[SDLK_BREAK] = "break";
    key_names[SDLK_MENU] = "menu";
    key_names[SDLK_POWER] = "power";
    key_names[SDLK_EURO] = "euro";
    key_names[SDLK_UNDO] = "undo";
    key_names[SDLK_NUMLOCK] = "numlock";
    key_names[SDLK_CAPSLOCK] = "caps";
    key_names[SDLK_SCROLLOCK] = "scroll";
    key_names[SDLK_RSHIFT] = "rshift";
    key_names[SDLK_LSHIFT] = "lshift";
    key_names[SDLK_RCTRL] = "rctrl";
    key_names[SDLK_LCTRL] = "lctrl";
    key_names[SDLK_RALT] = "ralt";
    key_names[SDLK_LALT] = "lalt";
    key_names[SDLK_RMETA] = "rmeta";
    key_names[SDLK_LMETA] = "lmeta";
    key_names[SDLK_LSUPER] = "lmulti";
    key_names[SDLK_RSUPER] = "rmulti";
    key_names[SDLK_MODE] = "mode";
    key_names[SDLK_COMPOSE] = "compose";

    return R_SUCCESS;
}

r_status_t r_event_keys_get_name(SDLKey s, const char **key_name)
{
    r_status_t status = (key_name != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        const char *name = key_names[s];

        if (name != NULL)
        {
            *key_name = name;
        }
        else
        {
            *key_name = "?";
            status = R_S_FIELD_NOT_FOUND;
        }
    }

    return status;
}

