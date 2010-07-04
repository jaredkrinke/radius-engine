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

#include <stdio.h>
#include <lua.h>

#include "r_state.h"
#include "r_script.h"
#include "r_log.h"
#include "r_file_system.h"
#include "r_video.h"
#include "r_audio.h"
#include "r_event.h"
#include "r_string.h"
#include "r_platform.h"
#include "radius.h"

static int radius_execute_internal(const char *argv0, const char *application_name, const char *data_dir, const char *user_dir, const char *script_path, const char *default_font_path)
{
    r_state_t radius_state;
    r_state_t *rs = &radius_state;
    r_status_t status = r_state_init(rs, argv0);

    if (R_SUCCEEDED(status))
    {
        /* Initialize file system first because it will setup stdout/stderr as necessary */
        char *data_dir_internal = NULL;
        char *user_dir_internal = NULL;
        char *script_path_internal = NULL;
        char *default_font_path_internal = NULL;

        /* Set up application-based default values, if necessary */
        if (application_name != NULL)
        {
            status = r_file_system_allocate_application_paths(rs, application_name, &data_dir_internal, &user_dir_internal, &script_path_internal, &default_font_path_internal);

            if (R_SUCCEEDED(status))
            {
                data_dir = data_dir_internal;
                user_dir = user_dir_internal;
                script_path = script_path_internal;
                default_font_path = default_font_path_internal;
            }
        }

        if (R_SUCCEEDED(status))
        {
            status = r_file_system_start(rs, data_dir, user_dir);

            if (R_SUCCEEDED(status))
            {
                status = r_script_start(rs);

                if (R_SUCCEEDED(status))
                {
                    /* Add file system script functions */
                    status = r_file_system_setup_script(rs);
                }

                if (R_SUCCEEDED(status))
                {
                    status = r_video_start(rs, default_font_path);

                    if (R_SUCCEEDED(status))
                    {
                        status = r_audio_start(rs);

                        if (R_SUCCEEDED(status))
                        {
                            status = r_event_start(rs);

                            if (R_SUCCEEDED(status))
                            {
                                lua_State *ls = rs->script_state;

                                lua_pushliteral(ls, "require");
                                lua_rawget(ls, LUA_GLOBALSINDEX);
                                lua_pushstring(ls, script_path);
                                status = (lua_pcall(ls, 1, 0, 0) == 0) ? R_SUCCESS : RS_F_INVALID_ARGUMENT;

                                if (R_SUCCEEDED(status))
                                {
                                    status = R_SCRIPT_SET_ERROR_CONTEXT(rs, l_panic);

                                    if (R_SUCCEEDED(status))
                                    {
                                        /* Main event loop */
                                        r_event_loop(rs);
                                    }
                                }
                                else
                                {
                                    r_log_error(rs, lua_tostring(ls, -1));
                                    lua_pop(ls, 1);
                                }

                                r_event_end(rs);
                            }
                            else
                            {
                                r_log_error(rs, "Could not initialize event library");
                            }

                            r_audio_end(rs);
                        }
                        else
                        {
                            r_log_error(rs, "Could not initialize audio library");
                        }

                        r_video_end(rs);
                    }
                    else
                    {
                        r_log_error(rs, "Could not initialize video library");
                    }

                    r_script_end(rs);
                }
                else
                {
                    r_log_error(rs, "Could not initialize scripting engine");
                }

                r_file_system_end(rs);
            }
            else
            {
                r_log_error(rs, "Could not initialize file system");
            }

            if (application_name != NULL)
            {
                r_file_system_free_application_paths(rs, &data_dir_internal, &user_dir_internal, &script_path_internal, &default_font_path_internal);
            }
        }

        r_state_cleanup(rs);
    }

    return (int)status;
}

int radius_execute_raw(const char *argv0, const char *data_dir, const char *user_dir, const char *script_path, const char *default_font_path)
{
    r_status_t status = (data_dir != NULL && user_dir != NULL && script_path != NULL && default_font_path != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    if (R_SUCCEEDED(status))
    {
        status = radius_execute_internal(argv0, NULL, data_dir, user_dir, script_path, default_font_path);
    }

    return status;
}

int radius_execute_application(const char *argv0, const char *application_name)
{
    r_status_t status = (application_name != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    if (R_SUCCEEDED(status))
    {
        status = radius_execute_internal(argv0, application_name, NULL, NULL, NULL, NULL);
    }

    return status;
}
