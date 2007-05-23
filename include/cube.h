/*
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include <compiz.h>

#define CUBE_ABIVERSION 20070523

#define CUBE_DISPLAY_OPTION_ABI    0
#define CUBE_DISPLAY_OPTION_INDEX  1
#define CUBE_DISPLAY_OPTION_UNFOLD 2
#define CUBE_DISPLAY_OPTION_NEXT   3
#define CUBE_DISPLAY_OPTION_PREV   4
#define CUBE_DISPLAY_OPTION_NUM    5

typedef struct _CubeDisplay {
    int	screenPrivateIndex;

    CompOption opt[CUBE_DISPLAY_OPTION_NUM];
} CubeDisplay;

#define CUBE_SCREEN_OPTION_COLOR	      0
#define CUBE_SCREEN_OPTION_IN		      1
#define CUBE_SCREEN_OPTION_SCALE_IMAGE	      2
#define CUBE_SCREEN_OPTION_IMAGES	      3
#define CUBE_SCREEN_OPTION_SKYDOME	      4
#define CUBE_SCREEN_OPTION_SKYDOME_IMG	      5
#define CUBE_SCREEN_OPTION_SKYDOME_ANIM	      6
#define CUBE_SCREEN_OPTION_SKYDOME_GRAD_START 7
#define CUBE_SCREEN_OPTION_SKYDOME_GRAD_END   8
#define CUBE_SCREEN_OPTION_ACCELERATION	      9
#define CUBE_SCREEN_OPTION_SPEED	      10
#define CUBE_SCREEN_OPTION_TIMESTEP	      11
#define CUBE_SCREEN_OPTION_MIPMAP	      12
#define CUBE_SCREEN_OPTION_BACKGROUNDS	      13
#define CUBE_SCREEN_OPTION_ADJUST_IMAGE	      14
#define CUBE_SCREEN_OPTION_NUM                15

typedef struct _CubeScreen {
    PreparePaintScreenProc     preparePaintScreen;
    DonePaintScreenProc	       donePaintScreen;
    PaintScreenProc	       paintScreen;
    PaintTransformedScreenProc paintTransformedScreen;
    PaintBackgroundProc        paintBackground;
    ApplyScreenTransformProc   applyScreenTransform;
    SetScreenOptionProc	       setScreenOption;
    OutputChangeNotifyProc     outputChangeNotify;

    CompOption opt[CUBE_SCREEN_OPTION_NUM];

    int      invert;
    int      xrotations;
    GLfloat  distance;
    Bool     paintTopBottom;
    GLushort color[3];
    GLfloat  tc[12];

    int grabIndex;

    int srcOutput;

    Bool    unfolded;
    GLfloat unfold, unfoldVelocity;

    GLfloat  *vertices;
    int      nvertices;

    GLuint skyListId;

    int		pw, ph;
    CompTexture texture, sky;

    int	imgCurFile;

    int nOutput;
    int output[64];
    int outputMask[64];

    Bool cleared[64];

    Bool fullscreenOutput;

    float outputXScale;
    float outputYScale;
    float outputXOffset;
    float outputYOffset;

    CompTexture *bg;
    int		nBg;
} CubeScreen;

#define GET_CUBE_DISPLAY(d)					 \
    ((CubeDisplay *) (d)->privates[cubeDisplayPrivateIndex].ptr)

#define CUBE_DISPLAY(d)			   \
    CubeDisplay *cd = GET_CUBE_DISPLAY (d)

#define GET_CUBE_SCREEN(s, cd)					 \
    ((CubeScreen *) (s)->privates[(cd)->screenPrivateIndex].ptr)

#define CUBE_SCREEN(s)							\
    CubeScreen *cs = GET_CUBE_SCREEN (s, GET_CUBE_DISPLAY (s->display))
