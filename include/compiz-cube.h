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

#ifndef _COMPIZ_CUBE_H
#define _COMPIZ_CUBE_H

#include <compiz-core.h>

COMPIZ_BEGIN_DECLS

#define CUBE_ABIVERSION 20080424

typedef struct _CubeCore {
    SetOptionForPluginProc setOptionForPlugin;
} CubeCore;

#define CUBE_MOMODE_AUTO  0
#define CUBE_MOMODE_MULTI 1
#define CUBE_MOMODE_ONE   2
#define CUBE_MOMODE_LAST  CUBE_MOMODE_ONE

#define CUBE_DISPLAY_OPTION_ABI        0
#define CUBE_DISPLAY_OPTION_INDEX      1
#define CUBE_DISPLAY_OPTION_UNFOLD_KEY 2
#define CUBE_DISPLAY_OPTION_NEXT_KEY   3
#define CUBE_DISPLAY_OPTION_PREV_KEY   4
#define CUBE_DISPLAY_OPTION_NUM        5

typedef struct _CubeDisplay {
    int	screenPrivateIndex;

    CompOption opt[CUBE_DISPLAY_OPTION_NUM];
} CubeDisplay;

#define CUBE_SCREEN_OPTION_COLOR	           0
#define CUBE_SCREEN_OPTION_IN		           1
#define CUBE_SCREEN_OPTION_SCALE_IMAGE	           2
#define CUBE_SCREEN_OPTION_IMAGES	           3
#define CUBE_SCREEN_OPTION_SKYDOME	           4
#define CUBE_SCREEN_OPTION_SKYDOME_IMG	           5
#define CUBE_SCREEN_OPTION_SKYDOME_ANIM	           6
#define CUBE_SCREEN_OPTION_SKYDOME_GRAD_START      7
#define CUBE_SCREEN_OPTION_SKYDOME_GRAD_END        8
#define CUBE_SCREEN_OPTION_ACCELERATION	           9
#define CUBE_SCREEN_OPTION_SPEED	           10
#define CUBE_SCREEN_OPTION_TIMESTEP	           11
#define CUBE_SCREEN_OPTION_MIPMAP	           12
#define CUBE_SCREEN_OPTION_ADJUST_IMAGE	           13
#define CUBE_SCREEN_OPTION_ACTIVE_OPACITY          14
#define CUBE_SCREEN_OPTION_INACTIVE_OPACITY        15
#define CUBE_SCREEN_OPTION_TRANSPARENT_MANUAL_ONLY 16
#define CUBE_SCREEN_OPTION_MULTIOUTPUT_MODE        17
#define CUBE_SCREEN_OPTION_NUM                     18

typedef enum _PaintOrder {
    BTF = 0,
    FTB
} PaintOrder;

typedef enum _RotationState {
    RotationNone = 0,
    RotationChange,
    RotationManual
} RotationState;

typedef void (*CubeGetRotationProc) (CompScreen *s,
				     float      *x,
				     float      *v,
				     float      *progress);

typedef void (*CubeClearTargetOutputProc) (CompScreen *s,
					   float      xRotate,
					   float      vRotate);

typedef void (*CubePaintTopProc) (CompScreen			*s,
				  const ScreenPaintAttrib	*sAttrib,
				  const CompTransform		*transform,
				  CompOutput			*output,
				  int				size);

typedef void (*CubePaintBottomProc) (CompScreen			*s,
				     const ScreenPaintAttrib	*sAttrib,
				     const CompTransform	*transform,
				     CompOutput			*output,
				     int			size);

typedef void (*CubePaintInsideProc) (CompScreen			*s,
				     const ScreenPaintAttrib	*sAttrib,
				     const CompTransform	*transform,
				     CompOutput			*output,
				     int			size);

typedef Bool (*CubeCheckOrientationProc) (CompScreen              *s,
					  const ScreenPaintAttrib *sAttrib,
					  const CompTransform     *transform,
					  CompOutput              *output,
					  CompVector              *points);

typedef void (*CubePaintViewportProc) (CompScreen              *s,
				       const ScreenPaintAttrib *sAttrib,
				       const CompTransform     *transform,
				       Region                  region,
				       CompOutput              *output,
				       unsigned int            mask);

typedef Bool (*CubeShouldPaintViewportProc) (CompScreen              *s,
					     const ScreenPaintAttrib *sAttrib,
					     const CompTransform     *transform,
					     CompOutput              *output,
					     PaintOrder              order);

typedef struct _CubeScreen {
    PreparePaintScreenProc       preparePaintScreen;
    DonePaintScreenProc	         donePaintScreen;
    PaintScreenProc		 paintScreen;
    PaintOutputProc	         paintOutput;
    PaintTransformedOutputProc   paintTransformedOutput;
    EnableOutputClippingProc     enableOutputClipping;
    PaintWindowProc              paintWindow;
    ApplyScreenTransformProc     applyScreenTransform;
    OutputChangeNotifyProc       outputChangeNotify;
    InitWindowWalkerProc         initWindowWalker;

    CubeGetRotationProc	         getRotation;
    CubeClearTargetOutputProc    clearTargetOutput;
    CubePaintTopProc             paintTop;
    CubePaintBottomProc          paintBottom;
    CubePaintInsideProc          paintInside;
    CubeCheckOrientationProc     checkOrientation;
    CubePaintViewportProc        paintViewport;
    CubeShouldPaintViewportProc  shouldPaintViewport;

    CompOption opt[CUBE_SCREEN_OPTION_NUM];

    int       invert;
    int       xRotations;
    PaintOrder paintOrder;

    RotationState rotationState;

    Bool paintAllViewports;

    GLfloat  distance;
    GLushort color[3];
    GLfloat  tc[12];

    int grabIndex;

    int srcOutput;

    Bool    unfolded;
    GLfloat unfold, unfoldVelocity;

    GLfloat  *vertices;
    int      nVertices;

    GLuint skyListId;

    int		 pw, ph;
    unsigned int skyW, skyH;
    CompTexture  texture, sky;

    int	imgCurFile;

    int nOutput;
    int output[64];
    int outputMask[64];

    Bool cleared[64];

    Bool capsPainted[64];

    Bool fullscreenOutput;

    float outputXScale;
    float outputYScale;
    float outputXOffset;
    float outputYOffset;

    float desktopOpacity;
    float toOpacity;
    int   lastOpacityIndex;

    int  moMode;
    Bool recalcOutput;
} CubeScreen;

#define GET_CUBE_CORE(c)					\
    ((CubeCore *) (c)->base.privates[cubeCorePrivateIndex].ptr)

#define CUBE_CORE(c)		     \
    CubeCore *cc = GET_CUBE_CORE (c)

#define GET_CUBE_DISPLAY(d)					      \
    ((CubeDisplay *) (d)->base.privates[cubeDisplayPrivateIndex].ptr)

#define CUBE_DISPLAY(d)			   \
    CubeDisplay *cd = GET_CUBE_DISPLAY (d)

#define GET_CUBE_SCREEN(s, cd)					      \
    ((CubeScreen *) (s)->base.privates[(cd)->screenPrivateIndex].ptr)

#define CUBE_SCREEN(s)							\
    CubeScreen *cs = GET_CUBE_SCREEN (s, GET_CUBE_DISPLAY (s->display))

COMPIZ_END_DECLS

#endif
