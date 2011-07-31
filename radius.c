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

int radius_execute_application(const char *argv0, const char *application_name, const char *data_dir_override)
{
    r_status_t status = (application_name != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    if (R_SUCCEEDED(status))
    {
        r_state_t radius_state;
        r_state_t *rs = &radius_state;
        r_status_t status = r_state_init(rs, argv0);

        if (R_SUCCEEDED(status))
        {
            /* Initialize file system first because it will setup stdout/stderr as necessary */
            char **data_dirs = NULL;
            char *user_dir = NULL;
            char *script_path = NULL;
            char *default_font_path = NULL;

            /* Set up application-based default values */
            status = r_file_system_allocate_application_paths(rs, application_name, data_dir_override, &data_dirs, &user_dir, &script_path, &default_font_path);

            if (R_SUCCEEDED(status))
            {
                status = r_file_system_start(rs, data_dirs, user_dir, script_path);

                if (R_SUCCEEDED(status))
                {
                    status = r_log_file_start(rs, application_name);

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
                            status = r_video_start(rs, application_name, default_font_path);

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

                        r_log_file_end(rs);
                    }
                    else
                    {
                        r_log_error(rs, "Could not open log file");
                    }

                    r_file_system_end(rs);
                }
                else
                {
                    r_log_error(rs, "Could not initialize file system");
                }

                r_file_system_free_application_paths(rs, &data_dirs, &user_dir, &script_path, &default_font_path);
            }
            else
            {
                r_log_error(rs, "Could not allocate application-specific paths");
            }

            r_state_cleanup(rs);
        }
    }

    return (int)status;
}

