/*
 * Copyright © 2005 Novell, Inc.
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

#ifndef _COMPIZ_H
#define _COMPIZ_H

#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/sync.h>
#include <X11/Xregion.h>

#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>

#include <GL/gl.h>
#include <GL/glx.h>

typedef struct _CompPlugin  CompPlugin;
typedef struct _CompDisplay CompDisplay;
typedef struct _CompScreen  CompScreen;
typedef struct _CompWindow  CompWindow;
typedef struct _CompTexture CompTexture;

/* virtual modifiers */

#define CompModAlt        0
#define CompModMeta       1
#define CompModSuper      2
#define CompModHyper      3
#define CompModModeSwitch 4
#define CompModNumLock    5
#define CompModScrollLock 6
#define CompModNum        7

#define CompAltMask        (1 << 16)
#define CompMetaMask       (1 << 17)
#define CompSuperMask      (1 << 18)
#define CompHyperMask      (1 << 19)
#define CompModeSwitchMask (1 << 20)
#define CompNumLockMask    (1 << 21)
#define CompScrollLockMask (1 << 22)

#define CompPressMask      (1 << 23)
#define CompReleaseMask    (1 << 24)

#define CompNoMask         (1 << 25)

#define CompWindowProtocolDeleteMask	  (1 << 0)
#define CompWindowProtocolTakeFocusMask	  (1 << 1)
#define CompWindowProtocolPingMask	  (1 << 2)
#define CompWindowProtocolSyncRequestMask (1 << 3)

#define CompWindowTypeDesktopMask     (1 << 0)
#define CompWindowTypeDockMask        (1 << 1)
#define CompWindowTypeToolbarMask     (1 << 2)
#define CompWindowTypeMenuMask        (1 << 3)
#define CompWindowTypeUtilMask        (1 << 4)
#define CompWindowTypeSplashMask      (1 << 5)
#define CompWindowTypeDialogMask      (1 << 6)
#define CompWindowTypeModalDialogMask (1 << 7)
#define CompWindowTypeNormalMask      (1 << 8)
#define CompWindowTypeFullscreenMask  (1 << 9)
#define CompWindowTypeUnknownMask     (1 << 10)

#define CompWindowStateModalMask	      (1 <<  0)
#define CompWindowStateStickyMask	      (1 <<  1)
#define CompWindowStateMaximizedVertMask      (1 <<  2)
#define CompWindowStateMaximizedHorzMask      (1 <<  3)
#define CompWindowStateShadedMask	      (1 <<  4)
#define CompWindowStateSkipTaskbarMask	      (1 <<  5)
#define CompWindowStateSkipPagerMask	      (1 <<  6)
#define CompWindowStateHiddenMask	      (1 <<  7)
#define CompWindowStateFullscreenMask	      (1 <<  8)
#define CompWindowStateAboveMask	      (1 <<  9)
#define CompWindowStateBelowMask	      (1 << 10)
#define CompWindowStateDemandsAttentationMask (1 << 11)
#define CompWindowStateDisplayModalMask	      (1 << 12)

#define CompWindowActionMoveMask	 (1 << 0)
#define CompWindowActionResizeMask	 (1 << 1)
#define CompWindowActionStickMask	 (1 << 2)
#define CompWindowActionMinimizeMask     (1 << 3)
#define CompWindowActionMaximizeHorzMask (1 << 4)
#define CompWindowActionMaximizeVertMask (1 << 5)
#define CompWindowActionFullscreenMask	 (1 << 6)
#define CompWindowActionCloseMask	 (1 << 7)

#define MwmDecorAll      (1L << 0)
#define MwmDecorBorder   (1L << 1)
#define MwmDecorHandle   (1L << 2)
#define MwmDecorTitle    (1L << 3)
#define MwmDecorMenu     (1L << 4)
#define MwmDecorMinimize (1L << 5)
#define MwmDecorMaximize (1L << 6)

#define WmMoveResizeSizeTopLeft      0
#define WmMoveResizeSizeTop          1
#define WmMoveResizeSizeTopRight     2
#define WmMoveResizeSizeRight        3
#define WmMoveResizeSizeBottomRight  4
#define WmMoveResizeSizeBottom       5
#define WmMoveResizeSizeBottomLeft   6
#define WmMoveResizeSizeLeft         7
#define WmMoveResizeMove             8
#define WmMoveResizeSizeKeyboard     9
#define WmMoveResizeMoveKeyboard    10

#define OPAQUE 0xffff
#define COLOR  0xffff
#define BRIGHT 0xffff

#define RED_SATURATION_WEIGHT   0.30f
#define GREEN_SATURATION_WEIGHT 0.59f
#define BLUE_SATURATION_WEIGHT  0.11f

extern char       *programName;
extern char       **programArgv;
extern int        programArgc;
extern char       *backgroundImage;
extern char       *windowImage;
extern REGION     emptyRegion;
extern REGION     infiniteRegion;
extern GLushort   defaultColor[4];
extern Window     currentRoot;
extern Bool       testMode;
extern Bool       restartSignal;
extern CompWindow *lastFoundWindow;
extern CompWindow *lastDamagedWindow;
extern Bool       replaceCurrentWm;

extern int  defaultRefreshRate;
extern char *defaultTextureFilter;

extern char *windowTypeString[];
extern int  nWindowTypeString;

#define RESTRICT_VALUE(value, min, max)				     \
    (((value) < (min)) ? (min): ((value) > (max)) ? (max) : (value))

#define MOD(a,b) ((a) < 0 ? ((b) - ((-(a) - 1) % (b))) - 1 : (a) % (b))


/* privates.h */

#define WRAP(priv, real, func, wrapFunc) \
    (priv)->func = (real)->func;	 \
    (real)->func = (wrapFunc)

#define UNWRAP(priv, real, func) \
    (real)->func = (priv)->func

typedef union _CompPrivate {
    void	  *ptr;
    long	  val;
    unsigned long uval;
    void	  *(*fptr) (void);
} CompPrivate;

typedef int (*ReallocPrivatesProc) (int size, void *closure);

int
allocatePrivateIndex (int		  *len,
		      char		  **indices,
		      ReallocPrivatesProc reallocProc,
		      void		  *closure);

void
freePrivateIndex (int  len,
		  char *indices,
		  int  index);


/* readpng.c */

Bool
readPng (const char   *filename,
	 char	      **data,
	 unsigned int *width,
	 unsigned int *height);

Bool
readPngBuffer (const unsigned char *buffer,
	       char		   **data,
	       unsigned int	   *width,
	       unsigned int	   *height);

/* option.c */

typedef enum {
    CompOptionTypeBool,
    CompOptionTypeInt,
    CompOptionTypeFloat,
    CompOptionTypeString,
    CompOptionTypeColor,
    CompOptionTypeBinding,
    CompOptionTypeList
} CompOptionType;

typedef enum {
    CompBindingTypeKey,
    CompBindingTypeButton
} CompBindingType;

typedef struct _CompKeyBinding {
    int		 keycode;
    unsigned int modifiers;
} CompKeyBinding;

typedef struct _CompButtonBinding {
    int		 button;
    unsigned int modifiers;
} CompButtonBinding;

typedef struct {
    CompBindingType type;
    union {
	CompKeyBinding    key;
	CompButtonBinding button;
    } u;
} CompBinding;

typedef union _CompOptionValue CompOptionValue;

typedef struct {
    CompOptionType  type;
    CompOptionValue *value;
    int		    nValue;
} CompListValue;

union _CompOptionValue {
    Bool	   b;
    int		   i;
    float	   f;
    char	   *s;
    unsigned short c[4];
    CompBinding    bind;
    CompListValue  list;
};

typedef struct _CompOptionIntRestriction {
    int min;
    int max;
} CompOptionIntRestriction;

typedef struct _CompOptionFloatRestriction {
    float min;
    float max;
    float precision;
} CompOptionFloatRestriction;

typedef struct _CompOptionStringRestriction {
    char **string;
    int  nString;
} CompOptionStringRestriction;

typedef union {
    CompOptionIntRestriction    i;
    CompOptionFloatRestriction  f;
    CompOptionStringRestriction s;
} CompOptionRestriction;

typedef struct _CompOption {
    char		  *name;
    char		  *shortDesc;
    char		  *longDesc;
    CompOptionType	  type;
    CompOptionValue	  value;
    CompOptionRestriction rest;
} CompOption;

typedef CompOption *(*DisplayOptionsProc) (CompDisplay *display, int *count);
typedef CompOption *(*ScreenOptionsProc) (CompScreen *screen, int *count);

CompOption *
compFindOption (CompOption *option,
		int	    nOption,
		char	    *name,
		int	    *index);

Bool
compSetBoolOption (CompOption	   *option,
		   CompOptionValue *value);

Bool
compSetIntOption (CompOption	  *option,
		  CompOptionValue *value);

Bool
compSetFloatOption (CompOption	    *option,
		    CompOptionValue *value);

Bool
compSetStringOption (CompOption	     *option,
		     CompOptionValue *value);

Bool
compSetColorOption (CompOption	    *option,
		    CompOptionValue *value);

Bool
compSetBindingOption (CompOption      *option,
		      CompOptionValue *value);

Bool
compSetOptionList (CompOption      *option,
		   CompOptionValue *value);

unsigned int
compWindowTypeMaskFromStringList (CompOptionValue *value);


/* display.c */

typedef int CompTimeoutHandle;

#define COMP_DISPLAY_OPTION_ACTIVE_PLUGINS  0
#define COMP_DISPLAY_OPTION_TEXTURE_FILTER  1
#define COMP_DISPLAY_OPTION_CLICK_TO_FOCUS  2
#define COMP_DISPLAY_OPTION_AUTORAISE	    3
#define COMP_DISPLAY_OPTION_AUTORAISE_DELAY 4
#define COMP_DISPLAY_OPTION_NUM		    5

typedef CompOption *(*GetDisplayOptionsProc) (CompDisplay *display,
					      int	  *count);
typedef Bool (*SetDisplayOptionProc) (CompDisplay     *display,
				      char	      *name,
				      CompOptionValue *value);
typedef Bool (*SetDisplayOptionForPluginProc) (CompDisplay     *display,
					       char	       *plugin,
					       char	       *name,
					       CompOptionValue *value);

typedef Bool (*InitPluginForDisplayProc) (CompPlugin  *plugin,
					  CompDisplay *display);

typedef void (*FiniPluginForDisplayProc) (CompPlugin  *plugin,
					  CompDisplay *display);

typedef void (*HandleEventProc) (CompDisplay *display,
				 XEvent	     *event);

typedef Bool (*CallBackProc) (void *closure);

typedef void (*ForEachWindowProc) (CompWindow *window,
				   void	      *closure);

struct _CompDisplay {
    Display    *display;
    CompScreen *screens;

    char *screenPrivateIndices;
    int  screenPrivateLen;

    int compositeEvent, compositeError, compositeOpcode;
    int damageEvent, damageError;
    int randrEvent, randrError;
    int syncEvent, syncError;

    Bool shapeExtension;
    int  shapeEvent, shapeError;

    SnDisplay *snDisplay;

    Atom supportedAtom;
    Atom supportingWmCheckAtom;

    Atom utf8StringAtom;

    Atom wmNameAtom;

    Atom winTypeAtom;
    Atom winTypeDesktopAtom;
    Atom winTypeDockAtom;
    Atom winTypeToolbarAtom;
    Atom winTypeMenuAtom;
    Atom winTypeUtilAtom;
    Atom winTypeSplashAtom;
    Atom winTypeDialogAtom;
    Atom winTypeNormalAtom;

    Atom winOpacityAtom;
    Atom winBrightnessAtom;
    Atom winSaturationAtom;
    Atom winActiveAtom;

    Atom workareaAtom;

    Atom desktopViewportAtom;
    Atom desktopGeometryAtom;
    Atom currentDesktopAtom;
    Atom numberOfDesktopsAtom;

    Atom winStateAtom;
    Atom winStateModalAtom;
    Atom winStateStickyAtom;
    Atom winStateMaximizedVertAtom;
    Atom winStateMaximizedHorzAtom;
    Atom winStateShadedAtom;
    Atom winStateSkipTaskbarAtom;
    Atom winStateSkipPagerAtom;
    Atom winStateHiddenAtom;
    Atom winStateFullscreenAtom;
    Atom winStateAboveAtom;
    Atom winStateBelowAtom;
    Atom winStateDemandsAttentionAtom;
    Atom winStateDisplayModalAtom;

    Atom winActionMoveAtom;
    Atom winActionResizeAtom;
    Atom winActionStickAtom;
    Atom winActionMinimizeAtom;
    Atom winActionMaximizeHorzAtom;
    Atom winActionMaximizeVertAtom;
    Atom winActionFullscreenAtom;
    Atom winActionCloseAtom;

    Atom wmAllowedActionsAtom;

    Atom wmStrutAtom;
    Atom wmStrutPartialAtom;

    Atom clientListAtom;
    Atom clientListStackingAtom;

    Atom frameExtentsAtom;
    Atom frameWindowAtom;

    Atom wmStateAtom;
    Atom wmChangeStateAtom;
    Atom wmProtocolsAtom;
    Atom wmClientLeaderAtom;

    Atom wmDeleteWindowAtom;
    Atom wmTakeFocusAtom;
    Atom wmPingAtom;
    Atom wmSyncRequestAtom;

    Atom wmSyncRequestCounterAtom;

    Atom closeWindowAtom;
    Atom wmMoveResizeAtom;
    Atom moveResizeWindowAtom;

    Atom showingDesktopAtom;

    Atom xBackgroundAtom[2];

    Atom panelActionAtom;
    Atom panelActionMainMenuAtom;
    Atom panelActionRunDialogAtom;

    Atom mwmHintsAtom;

    Atom managerAtom;
    Atom targetsAtom;
    Atom multipleAtom;
    Atom timestampAtom;
    Atom versionAtom;
    Atom atomPairAtom;

    unsigned int      lastPing;
    CompTimeoutHandle pingHandle;

    GLenum textureFilter;

    Window activeWindow;

    Window below;
    char   displayString[256];

    unsigned int modMask[CompModNum];
    unsigned int ignoredModMask;

    CompOption opt[COMP_DISPLAY_OPTION_NUM];

    CompTimeoutHandle autoRaiseHandle;
    Window	      autoRaiseWindow;

    CompOptionValue plugin;
    Bool	    dirtyPluginList;

    SetDisplayOptionProc	  setDisplayOption;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;

    InitPluginForDisplayProc initPluginForDisplay;
    FiniPluginForDisplayProc finiPluginForDisplay;

    HandleEventProc handleEvent;

    CompPrivate *privates;
};

extern CompDisplay *compDisplays;

int
allocateDisplayPrivateIndex (void);

void
freeDisplayPrivateIndex (int index);

CompOption *
compGetDisplayOptions (CompDisplay *display,
		       int	   *count);

CompTimeoutHandle
compAddTimeout (int	     time,
		CallBackProc callBack,
		void	     *closure);

void
compRemoveTimeout (CompTimeoutHandle handle);

int
compCheckForError (Display *dpy);

Bool
addDisplay (char *name,
	    char **plugin,
	    int  nPlugin);

void
focusDefaultWindow (CompDisplay *d);

void
forEachWindowOnDisplay (CompDisplay	  *display,
			ForEachWindowProc proc,
			void		  *closure);

CompScreen *
findScreenAtDisplay (CompDisplay *d,
		     Window      root);

CompWindow *
findWindowAtDisplay (CompDisplay *display,
		     Window      id);

unsigned int
virtualToRealModMask (CompDisplay  *d,
		      unsigned int modMask);

void
updateModifierMappings (CompDisplay *d);

void
eventLoop (void);

void
handleSelectionRequest (CompDisplay *display,
			XEvent      *event);

void
handleSelectionClear (CompDisplay *display,
		      XEvent      *event);


/* event.c */

#define EV_BUTTON(opt, event)						\
    ((opt)->value.bind.type == CompBindingTypeButton &&			\
     (opt)->value.bind.u.button.button == (event)->xbutton.button &&	\
     ((opt)->value.bind.u.button.modifiers & (event)->xbutton.state) == \
     (opt)->value.bind.u.button.modifiers)

#define EV_KEY(opt, event)					  \
    ((opt)->value.bind.type == CompBindingTypeKey &&		  \
     (opt)->value.bind.u.key.keycode == (event)->xkey.keycode &&  \
     ((opt)->value.bind.u.key.modifiers & (event)->xkey.state) == \
     (opt)->value.bind.u.key.modifiers)

void
handleEvent (CompDisplay *display,
	     XEvent      *event);

void
handleSyncAlarm (CompWindow *w);


/* paint.c */

#define MULTIPLY_USHORT(us1, us2)		 \
    (((GLuint) (us1) * (GLuint) (us2)) / 0xffff)

/* camera distance from screen, 0.5 * tan (FOV) */
#define DEFAULT_Z_CAMERA 0.866025404f

typedef struct _ScreenPaintAttrib {
    GLfloat xRotate;
    GLfloat yRotate;
    GLfloat vRotate;
    GLfloat xTranslate;
    GLfloat yTranslate;
    GLfloat zTranslate;
    GLfloat zCamera;
} ScreenPaintAttrib;

typedef struct _WindowPaintAttrib {
    GLushort opacity;
    GLushort brightness;
    GLushort saturation;
    GLfloat  xScale;
    GLfloat  yScale;
} WindowPaintAttrib;

extern ScreenPaintAttrib defaultScreenPaintAttrib;
extern WindowPaintAttrib defaultWindowPaintAttrib;

typedef struct _CompMatrix {
    float xx; float yx;
    float xy; float yy;
    float x0; float y0;
} CompMatrix;

#define COMP_TEX_COORD_X(m, vx) ((m)->xx * (vx) + (m)->x0)
#define COMP_TEX_COORD_Y(m, vy) ((m)->yy * (vy) + (m)->y0)

#define COMP_TEX_COORD_XY(m, vx, vy)		\
    ((m)->xx * (vx) + (m)->xy * (vy) + (m)->x0)
#define COMP_TEX_COORD_YX(m, vx, vy)		\
    ((m)->yx * (vx) + (m)->yy * (vy) + (m)->y0)


typedef void (*PreparePaintScreenProc) (CompScreen *screen,
					int	   msSinceLastPaint);

typedef void (*DonePaintScreenProc) (CompScreen *screen);

#define PAINT_SCREEN_REGION_MASK		   (1 << 0)
#define PAINT_SCREEN_FULL_MASK			   (1 << 1)
#define PAINT_SCREEN_TRANSFORMED_MASK		   (1 << 2)
#define PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK (1 << 3)

typedef Bool (*PaintScreenProc) (CompScreen		 *screen,
				 const ScreenPaintAttrib *sAttrib,
				 Region			 region,
				 unsigned int		 mask);

typedef void (*PaintTransformedScreenProc) (CompScreen		    *screen,
					    const ScreenPaintAttrib *sAttrib,
					    unsigned int	    mask);


#define PAINT_WINDOW_SOLID_MASK			(1 << 0)
#define PAINT_WINDOW_TRANSLUCENT_MASK		(1 << 1)
#define PAINT_WINDOW_TRANSFORMED_MASK           (1 << 2)
#define PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK (1 << 3)

typedef Bool (*PaintWindowProc) (CompWindow		 *window,
				 const WindowPaintAttrib *attrib,
				 Region			 region,
				 unsigned int		 mask);

typedef void (*AddWindowGeometryProc) (CompWindow *window,
				       CompMatrix *matrix,
				       int	  nMatrix,
				       Region	  region,
				       Region	  clip);

typedef void (*DrawWindowGeometryProc) (CompWindow *window);

#define PAINT_BACKGROUND_ON_TRANSFORMED_SCREEN_MASK (1 << 0)
#define PAINT_BACKGROUND_WITH_STENCIL_MASK          (1 << 1)

typedef void (*PaintBackgroundProc) (CompScreen   *screen,
				     Region	  region,
				     unsigned int mask);


void
preparePaintScreen (CompScreen *screen,
		    int	       msSinceLastPaint);

void
donePaintScreen (CompScreen *screen);

void
translateRotateScreen (const ScreenPaintAttrib *sa);

void
paintTransformedScreen (CompScreen		*screen,
			const ScreenPaintAttrib *sAttrib,
			unsigned int	        mask);

Bool
paintScreen (CompScreen		     *screen,
	     const ScreenPaintAttrib *sAttrib,
	     Region		     region,
	     unsigned int	     mask);

Bool
moreWindowVertices (CompWindow *w,
		    int        newSize);

Bool
moreWindowIndices (CompWindow *w,
		   int        newSize);

void
addWindowGeometry (CompWindow *w,
		   CompMatrix *matrix,
		   int	      nMatrix,
		   Region     region,
		   Region     clip);

void
drawWindowGeometry (CompWindow *w);

void
drawWindowTexture (CompWindow		   *w,
		   CompTexture		   *texture,
		   const WindowPaintAttrib *attrib,
		   unsigned int		   mask);

Bool
paintWindow (CompWindow		     *w,
	     const WindowPaintAttrib *attrib,
	     Region		     region,
	     unsigned int	     mask);

void
paintBackground (CompScreen   *screen,
		 Region	      region,
		 unsigned int mask);


/* texture.c */

#define POWER_OF_TWO(v) ((v & (v - 1)) == 0)

typedef enum {
    COMP_TEXTURE_FILTER_FAST,
    COMP_TEXTURE_FILTER_GOOD
} CompTextureFilter;

struct _CompTexture {
    GLuint     name;
    GLenum     target;
    GLfloat    dx, dy;
    GLXPixmap  pixmap;
    GLenum     filter;
    GLenum     wrap;
    CompMatrix matrix;
    Bool       oldMipmaps;
};

void
initTexture (CompScreen  *screen,
	     CompTexture *texture);

void
finiTexture (CompScreen  *screen,
	     CompTexture *texture);

Bool
readImageToTexture (CompScreen   *screen,
		    CompTexture  *texture,
		    const char	 *imageFileName,
		    unsigned int *width,
		    unsigned int *height);

Bool
readImageBufferToTexture (CompScreen	      *screen,
			  CompTexture	      *texture,
			  const unsigned char *imageBuffer,
			  unsigned int	      *returnWidth,
			  unsigned int	      *returnHeight);

Bool
bindPixmapToTexture (CompScreen  *screen,
		     CompTexture *texture,
		     Pixmap	 pixmap,
		     int	 width,
		     int	 height,
		     int	 depth);

void
releasePixmapFromTexture (CompScreen  *screen,
			  CompTexture *texture);

void
enableTexture (CompScreen        *screen,
	       CompTexture	 *texture,
	       CompTextureFilter filter);

void
enableTextureClampToBorder (CompScreen	      *screen,
			    CompTexture	      *texture,
			    CompTextureFilter filter);

void
enableTextureClampToEdge (CompScreen	    *screen,
			  CompTexture	    *texture,
			  CompTextureFilter filter);

void
disableTexture (CompTexture *texture);


/* screen.c */

#define COMP_SCREEN_OPTION_DETECT_REFRESH_RATE 0
#define COMP_SCREEN_OPTION_LIGHTING	       1
#define COMP_SCREEN_OPTION_REFRESH_RATE	       2
#define COMP_SCREEN_OPTION_SIZE		       3
#define COMP_SCREEN_OPTION_CLOSE_WINDOW	       4
#define COMP_SCREEN_OPTION_MAIN_MENU	       5
#define COMP_SCREEN_OPTION_RUN_DIALOG	       6
#define COMP_SCREEN_OPTION_COMMAND0	       7
#define COMP_SCREEN_OPTION_RUN_COMMAND0	       8
#define COMP_SCREEN_OPTION_COMMAND1	       9
#define COMP_SCREEN_OPTION_RUN_COMMAND1	       10
#define COMP_SCREEN_OPTION_COMMAND2	       11
#define COMP_SCREEN_OPTION_RUN_COMMAND2	       12
#define COMP_SCREEN_OPTION_COMMAND3	       13
#define COMP_SCREEN_OPTION_RUN_COMMAND3	       14
#define COMP_SCREEN_OPTION_COMMAND4	       15
#define COMP_SCREEN_OPTION_RUN_COMMAND4	       16
#define COMP_SCREEN_OPTION_COMMAND5	       17
#define COMP_SCREEN_OPTION_RUN_COMMAND5	       18
#define COMP_SCREEN_OPTION_COMMAND6	       19
#define COMP_SCREEN_OPTION_RUN_COMMAND6	       20
#define COMP_SCREEN_OPTION_COMMAND7	       21
#define COMP_SCREEN_OPTION_RUN_COMMAND7	       22
#define COMP_SCREEN_OPTION_COMMAND8	       23
#define COMP_SCREEN_OPTION_RUN_COMMAND8	       24
#define COMP_SCREEN_OPTION_COMMAND9	       25
#define COMP_SCREEN_OPTION_RUN_COMMAND9	       26
#define COMP_SCREEN_OPTION_COMMAND10	       27
#define COMP_SCREEN_OPTION_RUN_COMMAND10       28
#define COMP_SCREEN_OPTION_COMMAND11	       29
#define COMP_SCREEN_OPTION_RUN_COMMAND11       30
#define COMP_SCREEN_OPTION_SLOW_ANIMATIONS     31
#define COMP_SCREEN_OPTION_NUM		       32

typedef void (*FuncPtr) (void);
typedef FuncPtr (*GLXGetProcAddressProc) (const GLubyte *procName);

#ifndef GLX_EXT_render_texture
#define GLX_TEXTURE_TARGET_EXT              0x6001
#define GLX_TEXTURE_2D_EXT                  0x6002
#define GLX_TEXTURE_RECTANGLE_EXT           0x6003
#define GLX_NO_TEXTURE_EXT                  0x6004
#define GLX_FRONT_LEFT_EXT                  0x6005
#endif

typedef Bool    (*GLXBindTexImageProc)    (Display	 *display,
					   GLXDrawable	 drawable,
					   int		 buffer);
typedef Bool    (*GLXReleaseTexImageProc) (Display	 *display,
					   GLXDrawable	 drawable,
					   int		 buffer);
typedef void    (*GLXQueryDrawableProc)   (Display	 *display,
					   GLXDrawable	 drawable,
					   int		 attribute,
					   unsigned int  *value);

typedef void (*GLActiveTextureProc) (GLenum texture);
typedef void (*GLClientActiveTextureProc) (GLenum texture);
typedef void (*GLGenerateMipmapProc) (GLenum target);


#define MAX_DEPTH 32

typedef CompOption *(*GetScreenOptionsProc) (CompScreen *screen,
					     int	*count);
typedef Bool (*SetScreenOptionProc) (CompScreen      *screen,
				     char	     *name,
				     CompOptionValue *value);
typedef Bool (*SetScreenOptionForPluginProc) (CompScreen      *screen,
					      char	      *plugin,
					      char	      *name,
					      CompOptionValue *value);

typedef Bool (*InitPluginForScreenProc) (CompPlugin *plugin,
					 CompScreen *screen);

typedef void (*FiniPluginForScreenProc) (CompPlugin *plugin,
					 CompScreen *screen);

typedef Bool (*DamageWindowRectProc) (CompWindow *w,
				      Bool       initial,
				      BoxPtr     rect);

typedef Bool (*DamageWindowRegionProc) (CompWindow *w,
					Region     region);

typedef void (*SetWindowScaleProc) (CompWindow *w,
				    float      xScale,
				    float      yScale);

typedef Bool (*FocusWindowProc) (CompWindow *window);

typedef void (*WindowResizeNotifyProc) (CompWindow *window);

typedef void (*WindowMoveNotifyProc) (CompWindow *window,
				      int	 dx,
				      int	 dy);

#define CompWindowGrabKeyMask    (1 << 0)
#define CompWindowGrabButtonMask (1 << 1)
#define CompWindowGrabMoveMask   (1 << 2)
#define CompWindowGrabResizeMask (1 << 3)

typedef void (*WindowGrabNotifyProc) (CompWindow   *window,
				      int	   x,
				      int	   y,
				      unsigned int state,
				      unsigned int mask);

typedef void (*WindowUngrabNotifyProc) (CompWindow *window);

#define COMP_SCREEN_DAMAGE_PENDING_MASK (1 << 0)
#define COMP_SCREEN_DAMAGE_REGION_MASK  (1 << 1)
#define COMP_SCREEN_DAMAGE_ALL_MASK     (1 << 2)

typedef struct _CompKeyGrab {
    int		 keycode;
    unsigned int modifiers;
    int		 count;
} CompKeyGrab;

typedef struct _CompButtonGrab {
    int		 button;
    unsigned int modifiers;
    int		 count;
} CompButtonGrab;

typedef struct _CompGrab {
    Bool   active;
    Cursor cursor;
} CompGrab;

typedef struct _CompGroup {
    struct _CompGroup *next;
    unsigned int      refCnt;
    Window	      id;
} CompGroup;

typedef struct _CompStartupSequence {
    struct _CompStartupSequence *next;
    SnStartupSequence		*sequence;
} CompStartupSequence;

#define NOTHING_TRANS_FILTER 0
#define SCREEN_TRANS_FILTER  1
#define WINDOW_TRANS_FILTER  2

struct _CompScreen {
    CompScreen  *next;
    CompDisplay *display;
    CompWindow	*windows;
    CompWindow	*reverseWindows;

    char *windowPrivateIndices;
    int  windowPrivateLen;

    Colormap	      colormap;
    int		      screenNum;
    int		      width;
    int		      height;
    int		      x;
    int		      size;
    REGION	      region;
    Region	      damage;
    unsigned long     damageMask;
    Window	      root;
    Window	      fake[2];
    XWindowAttributes attrib;
    Window	      grabWindow;
    XVisualInfo       *glxPixmapVisuals[MAX_DEPTH + 1];
    int		      textureRectangle;
    int		      textureNonPowerOfTwo;
    int		      textureEnvCombine;
    int		      textureEnvCrossbar;
    int		      textureBorderClamp;
    GLint	      maxTextureSize;
    int		      mipmap;
    int		      maxTextureUnits;
    Cursor	      invisibleCursor;
    XRectangle        *exposeRects;
    int		      sizeExpose;
    int		      nExpose;
    CompTexture       backgroundTexture;
    unsigned int      pendingDestroys;
    int		      desktopWindowCount;
    KeyCode	      escapeKeyCode;
    unsigned int      mapNum;
    unsigned int      activeNum;

    SnMonitorContext    *snContext;
    CompStartupSequence *startupSequences;
    unsigned int        startupSequenceTimeoutHandle;

    int filter[3];

    CompGroup *groups;

    Bool canDoSaturated;
    Bool canDoSlightlySaturated;

    Window wmSnSelectionWindow;
    Atom   wmSnAtom;
    Time   wmSnTimestamp;

    Cursor normalCursor;
    Cursor busyCursor;

    CompWindow **clientList;
    int	       nClientList;

    CompButtonGrab *buttonGrab;
    int		   nButtonGrab;
    CompKeyGrab    *keyGrab;
    int		   nKeyGrab;

    CompGrab *grabs;
    int	     grabSize;
    int	     maxGrab;

    int		   rasterX;
    int		   rasterY;
    struct timeval lastRedraw;
    int		   nextRedraw;
    int		   redrawTime;
    int            optimalRedrawTime;
    int            frameStatus;

    GLint stencilRef;

    Bool lighting;
    Bool slowAnimations;

    XRectangle workArea;

    unsigned int showingDesktopMask;

    GLXGetProcAddressProc  getProcAddress;
    GLXBindTexImageProc    bindTexImage;
    GLXReleaseTexImageProc releaseTexImage;
    GLXQueryDrawableProc   queryDrawable;

    GLActiveTextureProc       activeTexture;
    GLClientActiveTextureProc clientActiveTexture;
    GLGenerateMipmapProc      generateMipmap;

    GLXContext ctx;

    CompOption opt[COMP_SCREEN_OPTION_NUM];

    SetScreenOptionProc		 setScreenOption;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;

    InitPluginForScreenProc initPluginForScreen;
    FiniPluginForScreenProc finiPluginForScreen;

    PreparePaintScreenProc       preparePaintScreen;
    DonePaintScreenProc	         donePaintScreen;
    PaintScreenProc	         paintScreen;
    PaintTransformedScreenProc   paintTransformedScreen;
    PaintBackgroundProc          paintBackground;
    PaintWindowProc	         paintWindow;
    AddWindowGeometryProc        addWindowGeometry;
    DrawWindowGeometryProc       drawWindowGeometry;
    DamageWindowRectProc         damageWindowRect;
    FocusWindowProc		 focusWindow;
    SetWindowScaleProc		 setWindowScale;

    WindowResizeNotifyProc windowResizeNotify;
    WindowMoveNotifyProc   windowMoveNotify;
    WindowGrabNotifyProc   windowGrabNotify;
    WindowUngrabNotifyProc windowUngrabNotify;

    CompPrivate *privates;
};

int
allocateScreenPrivateIndex (CompDisplay *display);

void
freeScreenPrivateIndex (CompDisplay *display,
			int	    index);

CompOption *
compGetScreenOptions (CompScreen *screen,
		      int	 *count);

void
configureScreen (CompScreen	 *s,
		 XConfigureEvent *ce);

void
updateScreenBackground (CompScreen  *screen,
			CompTexture *texture);

void
detectRefreshRateOfScreen (CompScreen *s);

Bool
addScreen (CompDisplay *display,
	   int	       screenNum,
	   Window      wmSnSelectionWindow,
	   Atom	       wmSnAtom,
	   Time	       wmSnTimestamp);

void
damageScreenRegion (CompScreen *screen,
		    Region     region);

void
damageScreen (CompScreen *screen);

void
damagePendingOnScreen (CompScreen *s);

void
insertWindowIntoScreen (CompScreen *s,
			CompWindow *w,
			Window	   aboveId);

void
unhookWindowFromScreen (CompScreen *s,
			CompWindow *w);

void
forEachWindowOnScreen (CompScreen	 *screen,
		       ForEachWindowProc proc,
		       void		 *closure);

CompWindow *
findWindowAtScreen (CompScreen *s,
		    Window     id);

CompWindow *
findTopLevelWindowAtScreen (CompScreen *s,
			    Window      id);

int
pushScreenGrab (CompScreen *s,
		Cursor     cursor);

void
removeScreenGrab (CompScreen *s,
		  int	     index,
		  XPoint     *restorePointer);

Bool
addScreenBinding (CompScreen  *s,
		  CompBinding *binding);

void
removeScreenBinding (CompScreen  *s,
		     CompBinding *binding);

void
updatePassiveGrabs (CompScreen *s);

void
updateWorkareaForScreen (CompScreen *s);

void
updateClientListForScreen (CompScreen *s);

Window
getActiveWindow (CompDisplay *display,
		 Window      root);

void
closeActiveWindow (CompScreen *s);

void
panelAction (CompScreen *s,
	     Atom	panelAction);

void
runCommand (CompScreen *s,
	    const char *command);

void
moveScreenViewport (CompScreen *s,
		    int	       tx,
		    Bool       sync);

void
moveWindowToViewportPosition (CompWindow *w,
			      int	 x,
			      Bool       sync);

CompGroup *
addGroupToScreen (CompScreen *s,
		  Window     id);
void
removeGroupFromScreen (CompScreen *s,
		       CompGroup  *group);

CompGroup *
findGroupAtScreen (CompScreen *s,
		   Window     id);

void
applyStartupProperties (CompScreen *screen,
			CompWindow *window);

void
enterShowDesktopMode (CompScreen *s);

void
leaveShowDesktopMode (CompScreen *s);

void
sendWindowActivationRequest (CompScreen *s,
			     Window	id);

void
screenTexEnvMode (CompScreen *s,
		  GLenum     mode);

void
screenLighting (CompScreen *s,
		Bool       lighting);

/* window.c */

#define WINDOW_INVISIBLE(w)				       \
    ((w)->attrib.map_state != IsViewable		    || \
     (!(w)->damaged)					    || \
     (w)->attrib.x + (w)->width  + (w)->output.right  <= 0  || \
     (w)->attrib.y + (w)->height + (w)->output.bottom <= 0  || \
     (w)->attrib.x - (w)->output.left >= (w)->screen->width || \
     (w)->attrib.y - (w)->output.top >= (w)->screen->height)

typedef Bool (*InitPluginForWindowProc) (CompPlugin *plugin,
					 CompWindow *window);
typedef void (*FiniPluginForWindowProc) (CompPlugin *plugin,
					 CompWindow *window);

typedef struct _CompWindowExtents {
    int left;
    int right;
    int top;
    int bottom;
} CompWindowExtents;

typedef struct _CompStruts {
    XRectangle left;
    XRectangle right;
    XRectangle top;
    XRectangle bottom;
} CompStruts;

struct _CompWindow {
    CompScreen *screen;
    CompWindow *next;
    CompWindow *prev;

    int		      refcnt;
    Window	      id;
    Window	      frame;
    unsigned int      mapNum;
    unsigned int      activeNum;
    XWindowAttributes attrib;
    int		      serverX;
    int		      serverY;
    Window	      transientFor;
    Window	      clientLeader;
    XSizeHints	      sizeHints;
    Pixmap	      pixmap;
    CompTexture       texture;
    CompMatrix        matrix;
    Damage	      damage;
    Bool	      inputHint;
    Bool	      alpha;
    GLint	      width;
    GLint	      height;
    Region	      region;
    Region	      clip;
    unsigned int      wmType;
    unsigned int      type;
    unsigned int      state;
    unsigned int      actions;
    unsigned int      protocols;
    unsigned int      mwmDecor;
    Bool	      invisible;
    Bool	      destroyed;
    Bool	      damaged;
    int		      destroyRefCnt;
    int		      unmapRefCnt;

    Bool placed;
    Bool minimized;

    char *startupId;
    char *resName;
    char *resClass;

    CompGroup *group;

    unsigned int lastPong;
    Bool	 alive;

    GLushort opacity;
    GLushort brightness;
    GLushort saturation;

    WindowPaintAttrib paint;
    WindowPaintAttrib lastPaint;
    Bool	      scaled;

    CompWindowExtents input;
    CompWindowExtents output;

    CompStruts *struts;

    XWindowChanges saveWc;
    int		   saveMask;

    XSyncCounter  syncCounter;
    XSyncValue	  syncValue;
    XSyncAlarm	  syncAlarm;
    unsigned long syncAlarmConnection;
    unsigned int  syncWaitHandle;

    Bool syncWait;
    int	 syncX;
    int	 syncY;
    int	 syncWidth;
    int	 syncHeight;
    int	 syncBorderWidth;

    XRectangle *damageRects;
    int	       sizeDamage;
    int	       nDamage;

    GLfloat  *vertices;
    int      vertexSize;
    GLushort *indices;
    int      indexSize;
    int      vCount;
    int      texUnits;

    CompPrivate *privates;
};

int
allocateWindowPrivateIndex (CompScreen *screen);

void
freeWindowPrivateIndex (CompScreen *screen,
			int	   index);

unsigned int
windowStateMask (CompDisplay *display,
		 Atom	     state);

unsigned int
getWindowState (CompDisplay *display,
		Window      id);

void
setWindowState (CompDisplay  *display,
		unsigned int state,
		Window       id);

unsigned int
getWindowType (CompDisplay *display,
	       Window      id);

void
recalcWindowType (CompWindow *w);

unsigned int
getMwmDecor (CompDisplay *display,
	     Window      id);

unsigned int
getProtocols (CompDisplay *display,
	      Window      id);

unsigned short
getWindowProp32 (CompDisplay	*display,
		 Window		id,
		 Atom		property,
		 unsigned short defaultValue);

void
setWindowProp32 (CompDisplay    *display,
		 Window         id,
		 Atom		property,
		 unsigned short value);

void
updateNormalHints (CompWindow *window);

void
updateWmHints (CompWindow *w);

void
updateWindowClassHints (CompWindow *window);

Window
getClientLeader (CompWindow *w);

int
getWmState (CompDisplay *display,
	    Window      id);

void
setWmState (CompDisplay *display,
	    int		state,
	    Window      id);

void
setWindowFrameExtents (CompWindow	 *w,
		       CompWindowExtents *input,
		       CompWindowExtents *output);

void
updateWindowRegion (CompWindow *w);

Bool
updateWindowStruts (CompWindow *w);

void
addWindow (CompScreen *screen,
	   Window     id,
	   Window     aboveId);

void
removeWindow (CompWindow *w);

void
destroyWindow (CompWindow *w);

void
mapWindow (CompWindow *w);

void
unmapWindow (CompWindow *w);

void
bindWindow (CompWindow *w);

void
releaseWindow (CompWindow *w);

void
moveWindow (CompWindow *w,
	    int        dx,
	    int        dy,
	    Bool       damage);

void
syncWindowPosition (CompWindow *w);

void
sendSyncRequest (CompWindow *w);

Bool
resizeWindow (CompWindow *w,
	      int	 x,
	      int	 y,
	      int	 width,
	      int	 height,
	      int	 borderWidth);

void
configureWindow (CompWindow	 *w,
		 XConfigureEvent *ce);

void
circulateWindow (CompWindow	 *w,
		 XCirculateEvent *ce);

void
addWindowDamage (CompWindow *w);

void
damageWindowOutputExtents (CompWindow *w);

Bool
damageWindowRect (CompWindow *w,
		  Bool       initial,
		  BoxPtr     rect);

void
damageWindowRegion (CompWindow *w,
		    Region     region);

void
setWindowScale (CompWindow *w,
		float      xScale,
		float      yScale);

Bool
focusWindow (CompWindow *w);

void
windowResizeNotify (CompWindow *w);

void
windowMoveNotify (CompWindow *w,
		  int	     dx,
		  int	     dy);

void
windowGrabNotify (CompWindow   *w,
		  int	       x,
		  int	       y,
		  unsigned int state,
		  unsigned int mask);

void
windowUngrabNotify (CompWindow *w);

void
moveInputFocusToWindow (CompWindow *w);

void
updateWindowAttributes (CompWindow *w);

void
activateWindow (CompWindow *w);

void
closeWindow (CompWindow *w);

void
getOuterRectOfWindow (CompWindow *w,
		      XRectangle *r);

Bool
constrainNewWindowSize (CompWindow *w,
			int        width,
			int        height,
			int        *newWidth,
			int        *newHeight);

void
hideWindow (CompWindow *w);

void
showWindow (CompWindow *w);

void
minimizeWindow (CompWindow *w);

void
unminimizeWindow (CompWindow *w);


/* plugin.c */

typedef Bool (*InitPluginProc) (CompPlugin *plugin);
typedef void (*FiniPluginProc) (CompPlugin *plugin);

typedef enum {
    CompPluginRuleBefore,
    CompPluginRuleAfter
} CompPluginRule;

typedef struct _CompPluginDep {
    CompPluginRule rule;
    char	   *plugin;
} CompPluginDep;

typedef struct _CompPluginVTable {
    char *name;
    char *shortDesc;
    char *longDesc;

    InitPluginProc init;
    FiniPluginProc fini;

    InitPluginForDisplayProc initDisplay;
    FiniPluginForDisplayProc finiDisplay;

    InitPluginForScreenProc initScreen;
    FiniPluginForScreenProc finiScreen;

    InitPluginForWindowProc initWindow;
    FiniPluginForWindowProc finiWindow;

    GetDisplayOptionsProc getDisplayOptions;
    SetDisplayOptionProc  setDisplayOption;
    GetScreenOptionsProc  getScreenOptions;
    SetScreenOptionProc   setScreenOption;

    CompPluginDep *deps;
    int		  nDeps;
} CompPluginVTable;

typedef CompPluginVTable *(*PluginGetInfoProc) (void);

struct _CompPlugin {
    CompPlugin       *next;
    void	     *dlhand;
    CompPluginVTable *vTable;
};

CompPluginVTable *
getCompPluginInfo (void);

void
screenInitPlugins (CompScreen *s);

void
screenFiniPlugins (CompScreen *s);

void
windowInitPlugins (CompWindow *w);

void
windowFiniPlugins (CompWindow *w);

CompPlugin *
findActivePlugin (char *name);

CompPlugin *
loadPlugin (char *plugin);

void
unloadPlugin (CompPlugin *p);

Bool
pushPlugin (CompPlugin *p);

CompPlugin *
popPlugin (void);


/* session.c */

void
initSession (char *smPrevClientId);

void
closeSession (void);

#endif
