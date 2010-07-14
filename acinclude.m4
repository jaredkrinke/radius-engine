# Configure paths for SDL
# Sam Lantinga 9/21/99
# stolen from Manish Singh
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor

dnl AM_PATH_SDL([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for SDL, and define SDL_CFLAGS and SDL_LIBS
dnl
AC_DEFUN([AM_PATH_SDL],
[dnl 
dnl Get the cflags and libraries from the sdl-config script
dnl
AC_ARG_WITH(sdl-prefix,[  --with-sdl-prefix=PFX   Prefix where SDL is installed (optional)],
            sdl_prefix="$withval", sdl_prefix="")
AC_ARG_WITH(sdl-exec-prefix,[  --with-sdl-exec-prefix=PFX Exec prefix where SDL is installed (optional)],
            sdl_exec_prefix="$withval", sdl_exec_prefix="")
AC_ARG_ENABLE(sdltest, [  --disable-sdltest       Do not try to compile and run a test SDL program],
		    , enable_sdltest=yes)

  if test x$sdl_exec_prefix != x ; then
    sdl_config_args="$sdl_config_args --exec-prefix=$sdl_exec_prefix"
    if test x${SDL_CONFIG+set} != xset ; then
      SDL_CONFIG=$sdl_exec_prefix/bin/sdl-config
    fi
  fi
  if test x$sdl_prefix != x ; then
    sdl_config_args="$sdl_config_args --prefix=$sdl_prefix"
    if test x${SDL_CONFIG+set} != xset ; then
      SDL_CONFIG=$sdl_prefix/bin/sdl-config
    fi
  fi

  if test "x$prefix" != xNONE; then
    PATH="$prefix/bin:$prefix/usr/bin:$PATH"
  fi
  AC_PATH_PROG(SDL_CONFIG, sdl-config, no, [$PATH])
  min_sdl_version=ifelse([$1], ,0.11.0,$1)
  AC_MSG_CHECKING(for SDL - version >= $min_sdl_version)
  no_sdl=""
  if test "$SDL_CONFIG" = "no" ; then
    no_sdl=yes
  else
    SDL_CFLAGS=`$SDL_CONFIG $sdl_config_args --cflags`
    SDL_LIBS=`$SDL_CONFIG $sdl_config_args --libs`

    sdl_major_version=`$SDL_CONFIG $sdl_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    sdl_minor_version=`$SDL_CONFIG $sdl_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    sdl_micro_version=`$SDL_CONFIG $sdl_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_sdltest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_CXXFLAGS="$CXXFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $SDL_CFLAGS"
      CXXFLAGS="$CXXFLAGS $SDL_CFLAGS"
      LIBS="$LIBS $SDL_LIBS"
dnl
dnl Now check if the installed SDL is sufficiently new. (Also sanity
dnl checks the results of sdl-config to some extent
dnl
      rm -f conf.sdltest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"

char*
my_strdup (char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = (char *)malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main (int argc, char *argv[])
{
  int major, minor, micro;
  char *tmp_version;

  /* This hangs on some systems (?)
  system ("touch conf.sdltest");
  */
  { FILE *fp = fopen("conf.sdltest", "a"); if ( fp ) fclose(fp); }

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_sdl_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_sdl_version");
     exit(1);
   }

   if (($sdl_major_version > major) ||
      (($sdl_major_version == major) && ($sdl_minor_version > minor)) ||
      (($sdl_major_version == major) && ($sdl_minor_version == minor) && ($sdl_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'sdl-config --version' returned %d.%d.%d, but the minimum version\n", $sdl_major_version, $sdl_minor_version, $sdl_micro_version);
      printf("*** of SDL required is %d.%d.%d. If sdl-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If sdl-config was wrong, set the environment variable SDL_CONFIG\n");
      printf("*** to point to the correct copy of sdl-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_sdl=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       CXXFLAGS="$ac_save_CXXFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_sdl" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$SDL_CONFIG" = "no" ; then
       echo "*** The sdl-config script installed by SDL could not be found"
       echo "*** If SDL was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the SDL_CONFIG environment variable to the"
       echo "*** full path to sdl-config."
     else
       if test -f conf.sdltest ; then
        :
       else
          echo "*** Could not run SDL test program, checking why..."
          CFLAGS="$CFLAGS $SDL_CFLAGS"
          CXXFLAGS="$CXXFLAGS $SDL_CFLAGS"
          LIBS="$LIBS $SDL_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include "SDL.h"

int main(int argc, char *argv[])
{ return 0; }
#undef  main
#define main K_and_R_C_main
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding SDL or finding the wrong"
          echo "*** version of SDL. If it is not finding SDL, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means SDL was incorrectly installed"
          echo "*** or that you have moved SDL since it was installed. In the latter case, you"
          echo "*** may want to edit the sdl-config script: $SDL_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          CXXFLAGS="$ac_save_CXXFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     SDL_CFLAGS=""
     SDL_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(SDL_CFLAGS)
  AC_SUBST(SDL_LIBS)
  rm -f conf.sdltest
])

dnl Updated 2010 by anonymous author (added -llua, -lglu32 on Windows, removed comments)
dnl
dnl Copyright 2000-2004 Erik Greenwald <erik@smluc.org>
dnl 
dnl (this is a BSD style license)
dnl 
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl 2. Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl 3. All advertising materials mentioning features or use of this software
dnl    must display the following acknowledgement:
dnl This product includes software developed by Erik Greenwald.
dnl 
dnl THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
dnl ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
dnl IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
dnl ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
dnl FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
dnl OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
dnl HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
dnl LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
dnl OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
dnl SUCH DAMAGE.
dnl 
dnl $Id: LICENSE,v 1.3 2004/08/14 21:17:09 erik Exp $
dnl 
dnl # $Id: OpenGL.m4,v 1.12 2004/01/03 16:02:33 erik Exp $
dnl #
dnl # stolen from 'famp' at http://famp.sourceforge.net (google told me to)
dnl # AC_SUBST's both GL_CFLAGS and GL_LIBS, and sets HAVE_GL
dnl #
dnl # oct 26, 2000 
dnl # * Changed CONFIGURE_OPENGL to AM_PATH_OPENGL for consistancy
dnl # -Erik Greenwald <erik@smluc.org>
dnl 
dnl 
dnl #
dnl # AM_PATH_OPENGL([ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND]])
dnl #

AC_DEFUN([AM_PATH_OPENGL],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_TARGET])

case $target in
	*-*-cygwin* | *-*-mingw*)
		;;
	*)
		AC_REQUIRE([AC_PATH_X])
		AC_REQUIRE([AC_PATH_XTRA])
		;;
esac

  GL_CFLAGS=""
  GL_LIBS=""

  AC_LANG_SAVE
  AC_LANG_C

  if ! test x"$no_x" = xyes; then
    GL_CFLAGS="$X_CFLAGS"
    GL_X_LIBS="$X_PRE_LIBS $X_LIBS $X_EXTRA_LIBS $LIBM"
  fi

  AC_ARG_WITH(gl-prefix,    
    [  --with-gl-prefix=PFX     Prefix where OpenGL is installed],
    [
      GL_CFLAGS="-I$withval/include"
      GL_LIBS="-L$withval/lib"
    ])

  AC_ARG_WITH(gl-includes,    
    [  --with-gl-includes=DIR   where the OpenGL includes are installed],
    [
      GL_CFLAGS="-I$withval"
    ])

  AC_ARG_WITH(gl-libraries,    
    [  --with-gl-libraries=DIR  where the OpenGL libraries are installed],
    [
      GL_LIBS="-L$withval"
    ])

  saved_CFLAGS="$CFLAGS"
  saved_LIBS="$LIBS"
  AC_LANG_SAVE
  AC_LANG_C
  HAVE_GL=no

  exec 8>&AC_FD_MSG

case "$target" in
	*-*-cygwin* | *-*-mingw32*)
		GL_CFLAGS=""
		GL_LIBS="-lopengl32 -lglu32"
		HAVE_GL=yes
		;;
	*-apple-darwin*)
		GL_LIBS="-framework GLUT -framework OpenGL -framework Carbon -lobjc"
		HAVE_GL=yes
		;;
	*)
		AC_SEARCH_LIBS(glFrustum,
			[GL MesaGL Mesa3d opengl opengl32],
			[
                AC_SEARCH_LIBS(gluPerspective, [GLU MesaGLU glu32],
                [
				    HAVE_GL=yes
				    GL_LIBS="$GL_LIBS $LIBS"
                ], AC_ERROR([GLU not found]), $GL_LIBS $GL_X_LIBS)
			],
			AC_ERROR([OpenGL not found]),
			$GL_LIBS $GL_X_LIBS)
		;;
esac

  LIBS="$saved_LIBS"
  CFLAGS="$saved_CFLAGS"

  exec AC_FD_MSG>&8
  AC_LANG_RESTORE

  if test "$HAVE_GL" = "yes"; then
     ifelse([$1], , :, [$1])     
  else
     GL_CFLAGS=""
     GL_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(GL_CFLAGS)
  AC_SUBST(GL_LIBS)
])

dnl # $Id: png.m4,v 1.2 2004/01/02 09:46:31 erik Exp $
dnl 
dnl #
dnl # AM_PATH_PNG([ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND]])
dnl #

AC_DEFUN([AM_PATH_PNG],
[
  AC_REQUIRE([AC_PROG_CC])

  PNG_CFLAGS=""
  PNG_LIBS=""

  AC_LANG_SAVE
  AC_LANG_C

  AC_ARG_WITH(png-prefix,    
    [  --with-png-prefix=PFX     Prefix where PNG is installed],
    [
      PNG_CFLAGS="-I$withval/include"
      PNG_LIBS="-L$withval/lib"
    ])

  AC_ARG_WITH(png-includes,    
    [  --with-png-includes=DIR   where the PNG includes are installed],
    [
      PNG_CFLAGS="-I$withval"
    ])

  AC_ARG_WITH(png-libraries,    
    [  --with-png-libraries=DIR  where the PNG libraries are installed],
    [
      PNG_LIBS="-L$withval"
    ])

  saved_CFLAGS="$CFLAGS"
  saved_LIBS="$LIBS"
  AC_LANG_SAVE
  AC_LANG_C
  HAVE_PNG=no

  exec 8>&AC_FD_MSG


  AC_CHECK_LIB(z, uncompress, AC_CHECK_LIB(png, png_create_info_struct, [
	PNG_LIBS="$PNG_LIBS -lpng -lz"
	HAVE_PNG=yes
    ],,$PNG_LIBS))

  LIBS="$saved_LIBS"
  CFLAGS="$saved_CFLAGS"

  exec AC_FD_MSG>&8
  AC_LANG_RESTORE

  if test "$HAVE_PNG" = "yes"; then
     ifelse([$1], , :, [$1])     
  else
     PNG_CFLAGS=""
     PNG_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(PNG_CFLAGS)
  AC_SUBST(PNG_LIBS)
])

AC_DEFUN([AM_PATH_LUA],
[
  AC_REQUIRE([AC_PROG_CC])

  LUA_CFLAGS=''
  LUA_LIBS=''

  PKG_CHECK_MODULES([LUA], [lua5.1], [HAVE_LUA=yes])

  AC_LANG_PUSH([C])
  AC_LANG_POP()

  if test "$HAVE_LUA" = "yes"; then
     ifelse([$1], , :, [$1])     
  else
     LUA_CFLAGS=""
     LUA_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  
  AC_SUBST(LUA_CFLAGS)
  AC_SUBST(LUA_LIBS)
])

AC_DEFUN([AM_PATH_PHYSFS],
[
  AC_REQUIRE([AC_PROG_CC])

  PHYSFS_CFLAGS=""
  PHYSFS_LIBS=""

  AC_LANG_PUSH([C])

  AC_ARG_WITH([physfs-prefix],
    [  --with-physfs-prefix=PFX     Prefix where PhysicsFS is installed],
    [
      PHYSFS_CFLAGS="-I$withval/include"
      PHYSFS_LIBS="-L$withval/lib"
    ])

  AC_ARG_WITH([physfs-includes],
    [  --with-physfs-includes=DIR   where the PhysicsFS includes are installed],
    [
      PHYSFS_CFLAGS="-I$withval"
    ])

  AC_ARG_WITH([physfs-libraries],
    [  --with-physfs-libraries=DIR  where the PhysicsFS libraries are installed],
    [
      PHYSFS_LIBS="-L$withval"
    ])

  saved_CFLAGS="$CFLAGS"
  saved_LIBS="$LIBS"
  HAVE_PHYSFS=no

  AC_CHECK_LIB([physfs],[PHYSFS_init],[
    HAVE_PHYSFS=yes
	PHYSFS_LIBS="$PHYSFS_LIBS -lphysfs"
    ],
    AC_SEARCH_LIBS([PHYSFS_init], [physfs], [
      HAVE_PHYSFS=yes
      PHYSFS_LIBS="$PHYSFS_LIBS -lphysfs -lz"
      ], [HAVE_PHYSFS=no], "-lz")
  )

  LIBS="$saved_LIBS"
  CFLAGS="$saved_CFLAGS"

  AC_LANG_POP()

  if test "$HAVE_PHYSFS" = "yes"; then
     ifelse([$1], , :, [$1])     
  else
     PHYSFS_CFLAGS=""
     PHYSFS_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(PHYSFS_CFLAGS)
  AC_SUBST(PHYSFS_LIBS)
])

AC_DEFUN([AM_PATH_SDL_SOUND],
[
  AC_REQUIRE([AC_PROG_CC])

  SDL_SOUND_CFLAGS=""
  SDL_SOUND_LIBS=""

  AC_LANG_PUSH([C])

  AC_ARG_WITH([sdl-sound-prefix],
    [  --with-sdl-sound-prefix=PFX     Prefix where SDL_Sound is installed],
    [
      SDL_SOUND_CFLAGS="-I$withval/include"
      SDL_SOUND_LIBS="-L$withval/lib"
    ])

  AC_ARG_WITH([sdl-sound-includes],
    [  --with-sdl-sound-includes=DIR   where the SDL_Sound includes are installed],
    [
      SDL_SOUND_CFLAGS="-I$withval"
    ])

  AC_ARG_WITH([sdl-sound-libraries],
    [  --with-sdl-sound-libraries=DIR  where the SDL_Sound libraries are installed],
    [
      SDL_SOUND_LIBS="-L$withval"
    ])

  saved_CFLAGS="$CFLAGS"
  saved_LIBS="$LIBS"
  HAVE_SDL_SOUND=no

  AC_CHECK_HEADER([SDL/SDL_sound.h], [
	SDL_SOUND_LIBS="$SDL_SOUND_LIBS -lSDL_sound"
	HAVE_SDL_SOUND=yes
    ])
dnl  dnl TODO: How to determine SDL_sound dependencies? Hopefully better than just testing them all one at a time...
dnl  AC_CHECK_LIB(SDL_sound, Sound_Init, [
dnl	SDL_SOUND_LIBS="$SDL_SOUND_LIBS -lSDL_sound"
dnl	HAVE_SDL_SOUND=yes
dnl    ])

  LIBS="$saved_LIBS"
  CFLAGS="$saved_CFLAGS"

  AC_LANG_POP()

  if test "$HAVE_SDL_SOUND" = "yes"; then
     ifelse([$1], , :, [$1])     
  else
     SDL_SOUND_CFLAGS=""
     SDL_SOUND_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(SDL_SOUND_CFLAGS)
  AC_SUBST(SDL_SOUND_LIBS)
])

