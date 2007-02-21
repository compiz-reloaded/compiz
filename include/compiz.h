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

#define ABIVERSION 20070213

#include <stdio.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/sync.h>
#include <X11/Xregion.h>
#include <X11/XKBlib.h>

#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>

#include <GL/gl.h>
#include <GL/glx.h>

#ifdef  __cplusplus
extern "C" {
#endif

#if COMPOSITE_MAJOR > 0 || COMPOSITE_MINOR > 2
#define USE_COW
#endif

/*
 * WORDS_BIGENDIAN should be defined before including this file for
 * IMAGE_BYTE_ORDER and BITMAP_BIT_ORDER to be set correctly.
 */
#define LSBFirst 0
#define MSBFirst 1

#ifdef WORDS_BIGENDIAN
#  define IMAGE_BYTE_ORDER MSBFirst
#  define BITMAP_BIT_ORDER MSBFirst
#else
#  define IMAGE_BYTE_ORDER LSBFirst
#  define BITMAP_BIT_ORDER LSBFirst
#endif

/* For now, we never actually translate strings inside compiz */
#define  _(x) x
#define N_(x) x

typedef struct _CompPlugin	  CompPlugin;
typedef struct _CompDisplay	  CompDisplay;
typedef struct _CompScreen	  CompScreen;
typedef struct _CompWindow	  CompWindow;
typedef struct _CompTexture	  CompTexture;
typedef struct _CompIcon	  CompIcon;
typedef struct _CompWindowExtents CompWindowExtents;
typedef struct _CompProgram	  CompProgram;
typedef struct _CompFunction	  CompFunction;
typedef struct _CompFunctionData  CompFunctionData;
typedef struct _FragmentAttrib    FragmentAttrib;
typedef struct _CompCursor	  CompCursor;

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

#define CompNoMask         (1 << 25)

#define CompWindowProtocolDeleteMask	  (1 << 0)
#define CompWindowProtocolTakeFocusMask	  (1 << 1)
#define CompWindowProtocolPingMask	  (1 << 2)
#define CompWindowProtocolSyncRequestMask (1 << 3)

#define CompWindowTypeDesktopMask      (1 << 0)
#define CompWindowTypeDockMask         (1 << 1)
#define CompWindowTypeToolbarMask      (1 << 2)
#define CompWindowTypeMenuMask         (1 << 3)
#define CompWindowTypeUtilMask         (1 << 4)
#define CompWindowTypeSplashMask       (1 << 5)
#define CompWindowTypeDialogMask       (1 << 6)
#define CompWindowTypeNormalMask       (1 << 7)
#define CompWindowTypeDropdownMenuMask (1 << 8)
#define CompWindowTypePopupMenuMask    (1 << 9)
#define CompWindowTypeTooltipMask      (1 << 10)
#define CompWindowTypeNotificationMask (1 << 11)
#define CompWindowTypeComboMask	       (1 << 12)
#define CompWindowTypeDndMask	       (1 << 13)
#define CompWindowTypeModalDialogMask  (1 << 14)
#define CompWindowTypeFullscreenMask   (1 << 15)
#define CompWindowTypeUnknownMask      (1 << 16)

#define CompWindowStateModalMask	    (1 <<  0)
#define CompWindowStateStickyMask	    (1 <<  1)
#define CompWindowStateMaximizedVertMask    (1 <<  2)
#define CompWindowStateMaximizedHorzMask    (1 <<  3)
#define CompWindowStateShadedMask	    (1 <<  4)
#define CompWindowStateSkipTaskbarMask	    (1 <<  5)
#define CompWindowStateSkipPagerMask	    (1 <<  6)
#define CompWindowStateHiddenMask	    (1 <<  7)
#define CompWindowStateFullscreenMask	    (1 <<  8)
#define CompWindowStateAboveMask	    (1 <<  9)
#define CompWindowStateBelowMask	    (1 << 10)
#define CompWindowStateDemandsAttentionMask (1 << 11)
#define CompWindowStateDisplayModalMask	    (1 << 12)

#define MAXIMIZE_STATE (CompWindowStateMaximizedHorzMask | \
			CompWindowStateMaximizedVertMask)

#define CompWindowActionMoveMask	  (1 << 0)
#define CompWindowActionResizeMask	  (1 << 1)
#define CompWindowActionStickMask	  (1 << 2)
#define CompWindowActionMinimizeMask      (1 << 3)
#define CompWindowActionMaximizeHorzMask  (1 << 4)
#define CompWindowActionMaximizeVertMask  (1 << 5)
#define CompWindowActionFullscreenMask	  (1 << 6)
#define CompWindowActionCloseMask	  (1 << 7)
#define CompWindowActionShadeMask	  (1 << 8)
#define CompWindowActionChangeDesktopMask (1 << 9)

#define MwmFuncAll      (1L << 0)
#define MwmFuncResize   (1L << 1)
#define MwmFuncMove     (1L << 2)
#define MwmFuncIconify  (1L << 3)
#define MwmFuncMaximize (1L << 4)
#define MwmFuncClose    (1L << 5)

#define MwmDecorHandle   (1L << 2)
#define MwmDecorTitle    (1L << 3)
#define MwmDecorMenu     (1L << 4)
#define MwmDecorMinimize (1L << 5)
#define MwmDecorMaximize (1L << 6)

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
extern REGION     emptyRegion;
extern REGION     infiniteRegion;
extern GLushort   defaultColor[4];
extern Window     currentRoot;
extern Bool       restartSignal;
extern CompWindow *lastFoundWindow;
extern CompWindow *lastDamagedWindow;
extern Bool       replaceCurrentWm;
extern Bool       indirectRendering;
extern Bool       strictBinding;
extern Bool       useCow;
extern Bool       noDetection;

extern int  defaultRefreshRate;
extern char *defaultTextureFilter;

extern char *windowTypeString[];
extern int  nWindowTypeString;

extern int lastPointerX;
extern int lastPointerY;
extern int pointerX;
extern int pointerY;

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


/* option.c */

typedef enum {
    CompOptionTypeBool,
    CompOptionTypeInt,
    CompOptionTypeFloat,
    CompOptionTypeString,
    CompOptionTypeColor,
    CompOptionTypeAction,
    CompOptionTypeList
} CompOptionType;

typedef enum {
    CompBindingTypeNone       = 0,
    CompBindingTypeKey        = 1 << 0,
    CompBindingTypeButton     = 1 << 1,
    CompBindingTypeEdgeButton = 1 << 2
} CompBindingType;

typedef enum {
    CompActionStateInitKey     = 1 <<  0,
    CompActionStateTermKey     = 1 <<  1,
    CompActionStateInitButton  = 1 <<  2,
    CompActionStateTermButton  = 1 <<  3,
    CompActionStateInitBell    = 1 <<  4,
    CompActionStateInitEdge    = 1 <<  5,
    CompActionStateTermEdge    = 1 <<  6,
    CompActionStateInitEdgeDnd = 1 <<  7,
    CompActionStateTermEdgeDnd = 1 <<  8,
    CompActionStateCommit      = 1 <<  9,
    CompActionStateCancel      = 1 << 10
} CompActionState;

typedef struct _CompKeyBinding {
    int		 keycode;
    unsigned int modifiers;
} CompKeyBinding;

typedef struct _CompButtonBinding {
    int		 button;
    unsigned int modifiers;
} CompButtonBinding;

typedef union _CompOptionValue CompOptionValue;

typedef struct _CompOption CompOption;
typedef struct _CompAction CompAction;

typedef Bool (*CompActionCallBackProc) (CompDisplay	*d,
					CompAction	*action,
					CompActionState state,
					CompOption	*option,
					int		nOption);

struct _CompAction {
    CompActionCallBackProc initiate;
    CompActionCallBackProc terminate;

    CompActionState state;

    CompBindingType   type;
    CompKeyBinding    key;
    CompButtonBinding button;

    Bool bell;

    unsigned int edgeMask;
    int		 edgeButton;
};

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
    CompAction     action;
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

struct _CompOption {
    char		  *name;
    char		  *shortDesc;
    char		  *longDesc;
    CompOptionType	  type;
    CompOptionValue	  value;
    CompOptionRestriction rest;
};

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
compSetActionOption (CompOption      *option,
		     CompOptionValue *value);

Bool
compSetOptionList (CompOption      *option,
		   CompOptionValue *value);

unsigned int
compWindowTypeMaskFromStringList (CompOptionValue *value);

Bool
getBoolOptionNamed (CompOption *option,
		    int	       nOption,
		    char       *name,
		    Bool       defaultValue);

int
getIntOptionNamed (CompOption *option,
		   int	      nOption,
		   char	      *name,
		   int	      defaultValue);

float
getFloatOptionNamed (CompOption *option,
		     int	nOption,
		     char	*name,
		     float	defaultValue);

char *
getStringOptionNamed (CompOption *option,
		      int	 nOption,
		      char	 *name,
		      char	 *defaultValue);

unsigned short *
getColorOptionNamed (CompOption	    *option,
		     int	    nOption,
		     char	    *name,
		     unsigned short *defaultValue);

char *
keyBindingToString (CompDisplay    *d,
		    CompKeyBinding *key);

char *
buttonBindingToString (CompDisplay       *d,
		       CompButtonBinding *button);

Bool
stringToKeyBinding (CompDisplay    *d,
		    const char     *binding,
		    CompKeyBinding *key);

Bool
stringToButtonBinding (CompDisplay	 *d,
		       const char	 *binding,
		       CompButtonBinding *button);

char *
edgeToString (unsigned int edge);

Bool
stringToColor (const char     *color,
	       unsigned short *rgba);

char *
colorToString (unsigned short *rgba);

char *
optionTypeToString (CompOptionType type);


/* display.c */

typedef int CompTimeoutHandle;
typedef int CompWatchFdHandle;
typedef int CompFileWatchHandle;

#define COMP_DISPLAY_OPTION_ACTIVE_PLUGINS                0
#define COMP_DISPLAY_OPTION_TEXTURE_FILTER                1
#define COMP_DISPLAY_OPTION_CLICK_TO_FOCUS                2
#define COMP_DISPLAY_OPTION_AUTORAISE                     3
#define COMP_DISPLAY_OPTION_AUTORAISE_DELAY               4
#define COMP_DISPLAY_OPTION_CLOSE_WINDOW                  5
#define COMP_DISPLAY_OPTION_MAIN_MENU                     6
#define COMP_DISPLAY_OPTION_RUN_DIALOG                    7
#define COMP_DISPLAY_OPTION_COMMAND0                      8
#define COMP_DISPLAY_OPTION_COMMAND1                      9
#define COMP_DISPLAY_OPTION_COMMAND2                      10
#define COMP_DISPLAY_OPTION_COMMAND3                      11
#define COMP_DISPLAY_OPTION_COMMAND4                      12
#define COMP_DISPLAY_OPTION_COMMAND5                      13
#define COMP_DISPLAY_OPTION_COMMAND6                      14
#define COMP_DISPLAY_OPTION_COMMAND7                      15
#define COMP_DISPLAY_OPTION_COMMAND8                      16
#define COMP_DISPLAY_OPTION_COMMAND9                      17
#define COMP_DISPLAY_OPTION_COMMAND10                     18
#define COMP_DISPLAY_OPTION_COMMAND11                     19
#define COMP_DISPLAY_OPTION_RUN_COMMAND0                  20
#define COMP_DISPLAY_OPTION_RUN_COMMAND1                  21
#define COMP_DISPLAY_OPTION_RUN_COMMAND2                  22
#define COMP_DISPLAY_OPTION_RUN_COMMAND3                  23
#define COMP_DISPLAY_OPTION_RUN_COMMAND4                  24
#define COMP_DISPLAY_OPTION_RUN_COMMAND5                  25
#define COMP_DISPLAY_OPTION_RUN_COMMAND6                  26
#define COMP_DISPLAY_OPTION_RUN_COMMAND7                  27
#define COMP_DISPLAY_OPTION_RUN_COMMAND8                  28
#define COMP_DISPLAY_OPTION_RUN_COMMAND9                  29
#define COMP_DISPLAY_OPTION_RUN_COMMAND10                 30
#define COMP_DISPLAY_OPTION_RUN_COMMAND11                 31
#define COMP_DISPLAY_OPTION_SLOW_ANIMATIONS               32
#define COMP_DISPLAY_OPTION_RAISE_WINDOW                  33
#define COMP_DISPLAY_OPTION_LOWER_WINDOW                  34
#define COMP_DISPLAY_OPTION_UNMAXIMIZE_WINDOW             35
#define COMP_DISPLAY_OPTION_MINIMIZE_WINDOW               36
#define COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW               37
#define COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW_HORZ          38
#define COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW_VERT          39
#define COMP_DISPLAY_OPTION_OPACITY_INCREASE              40
#define COMP_DISPLAY_OPTION_OPACITY_DECREASE              41
#define COMP_DISPLAY_OPTION_SCREENSHOT                    42
#define COMP_DISPLAY_OPTION_RUN_SCREENSHOT                43
#define COMP_DISPLAY_OPTION_WINDOW_SCREENSHOT             44
#define COMP_DISPLAY_OPTION_RUN_WINDOW_SCREENSHOT         45
#define COMP_DISPLAY_OPTION_WINDOW_MENU                   46
#define COMP_DISPLAY_OPTION_SHOW_DESKTOP                  47
#define COMP_DISPLAY_OPTION_RAISE_ON_CLICK                48
#define COMP_DISPLAY_OPTION_AUDIBLE_BELL                  49
#define COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED       50
#define COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED_HORZ  51
#define COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED_VERT  52
#define COMP_DISPLAY_OPTION_HIDE_SKIP_TASKBAR_WINDOWS     53
#define COMP_DISPLAY_OPTION_TOGGLE_WINDOW_SHADED          54
#define COMP_DISPLAY_OPTION_IGNORE_HINTS_WHEN_MAXIMIZED   55
#define COMP_DISPLAY_OPTION_TERMINAL			  56
#define COMP_DISPLAY_OPTION_RUN_TERMINAL		  57
#define COMP_DISPLAY_OPTION_PING_DELAY			  58
#define COMP_DISPLAY_OPTION_NUM				  59

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

typedef void (*HandleCompizEventProc) (CompDisplay *display,
				       char        *pluginName,
				       char        *eventName,
				       CompOption  *option,
				       int         nOption);

typedef Bool (*CallBackProc) (void *closure);

typedef void (*ForEachWindowProc) (CompWindow *window,
				   void	      *closure);

typedef Bool (*FileToImageProc) (CompDisplay *display,
				 const char  *path,
				 const char  *name,
				 int	     *width,
				 int	     *height,
				 int	     *stride,
				 void	     **data);

typedef Bool (*ImageToFileProc) (CompDisplay *display,
				 const char  *path,
				 const char  *name,
				 const char  *format,
				 int	     width,
				 int	     height,
				 int	     stride,
				 void	     *data);

#define NOTIFY_CREATE_MASK (1 << 0)
#define NOTIFY_DELETE_MASK (1 << 1)
#define NOTIFY_MOVE_MASK   (1 << 2)

typedef void (*FileWatchCallBackProc) (const char *name,
				       void	  *closure);

typedef struct _CompFileWatch {
    struct _CompFileWatch *next;
    char		  *path;
    int			  mask;
    FileWatchCallBackProc callBack;
    void		  *closure;
    CompFileWatchHandle   handle;
} CompFileWatch;

typedef void (*FileWatchAddedProc) (CompDisplay	  *display,
				    CompFileWatch *fileWatch);

typedef void (*FileWatchRemovedProc) (CompDisplay   *display,
				      CompFileWatch *fileWatch);

struct _CompDisplay {
    Display    *display;
    CompScreen *screens;

    char *screenPrivateIndices;
    int  screenPrivateLen;

    int compositeEvent, compositeError, compositeOpcode;
    int damageEvent, damageError;
    int randrEvent, randrError;
    int syncEvent, syncError;
    int fixesEvent, fixesError, fixesVersion;

    Bool shapeExtension;
    int  shapeEvent, shapeError;

    Bool xkbExtension;
    int  xkbEvent, xkbError;

    Bool xineramaExtension;
    int  xineramaEvent, xineramaError;

    XineramaScreenInfo *screenInfo;
    int		       nScreenInfo;

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
    Atom winTypeDropdownMenuAtom;
    Atom winTypePopupMenuAtom;
    Atom winTypeTooltipAtom;
    Atom winTypeNotificationAtom;
    Atom winTypeComboAtom;
    Atom winTypeDndAtom;

    Atom winOpacityAtom;
    Atom winBrightnessAtom;
    Atom winSaturationAtom;
    Atom winActiveAtom;
    Atom winDesktopAtom;

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
    Atom winActionShadeAtom;
    Atom winActionChangeDesktopAtom;

    Atom wmAllowedActionsAtom;

    Atom wmStrutAtom;
    Atom wmStrutPartialAtom;

    Atom wmUserTimeAtom;

    Atom wmIconAtom;

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
    Atom restackWindowAtom;

    Atom showingDesktopAtom;

    Atom xBackgroundAtom[2];

    Atom toolkitActionAtom;
    Atom toolkitActionMainMenuAtom;
    Atom toolkitActionRunDialogAtom;
    Atom toolkitActionWindowMenuAtom;
    Atom toolkitActionForceQuitDialogAtom;

    Atom mwmHintsAtom;

    Atom xdndAwareAtom;
    Atom xdndEnterAtom;
    Atom xdndLeaveAtom;
    Atom xdndPositionAtom;
    Atom xdndStatusAtom;
    Atom xdndDropAtom;

    Atom managerAtom;
    Atom targetsAtom;
    Atom multipleAtom;
    Atom timestampAtom;
    Atom versionAtom;
    Atom atomPairAtom;

    Atom startupIdAtom;

    unsigned int      lastPing;
    CompTimeoutHandle pingHandle;

    GLenum textureFilter;

    Window activeWindow;

    Window below;
    char   displayString[256];

    XModifierKeymap *modMap;
    unsigned int    modMask[CompModNum];
    unsigned int    ignoredModMask;

    KeyCode escapeKeyCode;
    KeyCode returnKeyCode;

    CompOption opt[COMP_DISPLAY_OPTION_NUM];

    CompTimeoutHandle autoRaiseHandle;
    Window	      autoRaiseWindow;

    CompOptionValue plugin;
    Bool	    dirtyPluginList;

    CompFileWatch *fileWatch;

    SetDisplayOptionProc	  setDisplayOption;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;

    InitPluginForDisplayProc initPluginForDisplay;
    FiniPluginForDisplayProc finiPluginForDisplay;

    HandleEventProc       handleEvent;
    HandleCompizEventProc handleCompizEvent;

    FileToImageProc fileToImage;
    ImageToFileProc imageToFile;

    FileWatchAddedProc   fileWatchAdded;
    FileWatchRemovedProc fileWatchRemoved;

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

CompWatchFdHandle
compAddWatchFd (int	     fd,
		short int    events,
		CallBackProc callBack,
		void	     *closure);

void
compRemoveWatchFd (CompWatchFdHandle handle);

CompFileWatchHandle
addFileWatch (CompDisplay	    *display,
	      const char	    *path,
	      int		    mask,
	      FileWatchCallBackProc callBack,
	      void		    *closure);

void
removeFileWatch (CompDisplay	     *display,
		 CompFileWatchHandle handle);

void
fileWatchAdded (CompDisplay   *display,
		CompFileWatch *fileWatch);

void
fileWatchRemoved (CompDisplay   *display,
		  CompFileWatch *fileWatch);

int
compCheckForError (Display *dpy);

void
addScreenToDisplay (CompDisplay *display,
		    CompScreen *s);

Bool
addDisplay (char *name,
	    char **plugin,
	    int  nPlugin);

Time
getCurrentTimeFromDisplay (CompDisplay *d);

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

CompWindow *
findTopLevelWindowAtDisplay (CompDisplay *d,
			     Window      id);

unsigned int
virtualToRealModMask (CompDisplay  *d,
		      unsigned int modMask);

void
updateModifierMappings (CompDisplay *d);

unsigned int
keycodeToModifiers (CompDisplay *d,
		    int         keycode);

void
eventLoop (void);

void
handleSelectionRequest (CompDisplay *display,
			XEvent      *event);

void
handleSelectionClear (CompDisplay *display,
		      XEvent      *event);

void
warpPointer (CompDisplay *display,
	     int	 dx,
	     int	 dy);

Bool
setDisplayAction (CompDisplay     *display,
		  CompOption      *o,
		  CompOptionValue *value);

Bool
readImageFromFile (CompDisplay *display,
		   const char  *name,
		   int	       *width,
		   int	       *height,
		   void	       **data);

Bool
writeImageToFile (CompDisplay *display,
		  const char  *path,
		  const char  *name,
		  const char  *format,
		  int	      width,
		  int	      height,
		  void	      *data);

Bool
fileToImage (CompDisplay *display,
	     const char	 *path,
	     const char	 *name,
	     int	 *width,
	     int	 *height,
	     int	 *stride,
	     void	 **data);

Bool
imageToFile (CompDisplay *display,
	     const char	 *path,
	     const char	 *name,
	     const char	 *format,
	     int	 width,
	     int	 height,
	     int	 stride,
	     void	 *data);

CompCursor *
findCursorAtDisplay (CompDisplay *display);


/* event.c */

void
handleEvent (CompDisplay *display,
	     XEvent      *event);

void
handleCompizEvent (CompDisplay *display,
		   char        *pluginName,
		   char        *eventName,
		   CompOption  *option,
		   int         nOption);

void
handleSyncAlarm (CompWindow *w);

Bool
eventMatches (CompDisplay *display,
	      XEvent      *event,
	      CompOption  *option);

Bool
eventTerminates (CompDisplay *display,
		 XEvent      *event,
		 CompOption  *option);

void
clearTargetOutput (CompDisplay	*display,
		   unsigned int mask);

/* paint.c */

#define MULTIPLY_USHORT(us1, us2)		 \
    (((GLuint) (us1) * (GLuint) (us2)) / 0xffff)

/* camera distance from screen, 0.5 * tan (FOV) */
#define DEFAULT_Z_CAMERA 0.866025404f

typedef struct _CompTransform {
    float m[16];
} CompTransform;

/* XXX: ScreenPaintAttrib will be removed */
typedef struct _ScreenPaintAttrib {
    GLfloat xRotate;
    GLfloat yRotate;
    GLfloat vRotate;
    GLfloat xTranslate;
    GLfloat yTranslate;
    GLfloat zTranslate;
    GLfloat zCamera;
} ScreenPaintAttrib;

/* XXX: scale and translate fields will be removed */
typedef struct _WindowPaintAttrib {
    GLushort opacity;
    GLushort brightness;
    GLushort saturation;
    GLfloat  xScale;
    GLfloat  yScale;
    GLfloat  xTranslate;
    GLfloat  yTranslate;
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
#define PAINT_SCREEN_CLEAR_MASK			   (1 << 4)

typedef Bool (*PaintScreenProc) (CompScreen		 *screen,
				 const ScreenPaintAttrib *sAttrib,
				 const CompTransform	 *transform,
				 Region			 region,
				 int		         output,
				 unsigned int		 mask);

typedef void (*PaintTransformedScreenProc) (CompScreen		    *screen,
					    const ScreenPaintAttrib *sAttrib,
					    const CompTransform	    *transform,
					    Region		    region,
					    int			    output,
					    unsigned int	    mask);

/* XXX: ApplyScreenTransformProc will be removed */
typedef void (*ApplyScreenTransformProc) (CompScreen		  *screen,
					  const ScreenPaintAttrib *sAttrib,
					  int			  output,
					  CompTransform	          *transform);


#define PAINT_WINDOW_SOLID_MASK			(1 << 0)
#define PAINT_WINDOW_TRANSLUCENT_MASK		(1 << 1)
#define PAINT_WINDOW_TRANSFORMED_MASK           (1 << 2)
#define PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK (1 << 3)
#define PAINT_WINDOW_NO_CORE_INSTANCE_MASK	(1 << 4)

typedef Bool (*PaintWindowProc) (CompWindow		 *window,
				 const WindowPaintAttrib *attrib,
				 const CompTransform     *transform,
				 Region			 region,
				 unsigned int		 mask);

typedef Bool (*DrawWindowProc) (CompWindow	     *window,
				const CompTransform  *transform,
				const FragmentAttrib *fragment,
				Region		     region,
				unsigned int	     mask);

typedef void (*AddWindowGeometryProc) (CompWindow *window,
				       CompMatrix *matrix,
				       int	  nMatrix,
				       Region	  region,
				       Region	  clip);

typedef void (*DrawWindowTextureProc) (CompWindow	    *w,
				       CompTexture	    *texture,
				       const FragmentAttrib *fragment,
				       unsigned int	    mask);

typedef void (*DrawWindowGeometryProc) (CompWindow *window);

typedef void (*PaintCursorProc) (CompCursor	     *cursor,
				 const CompTransform *transform,
				 Region		     region,
				 unsigned int	     mask);

#define PAINT_BACKGROUND_ON_TRANSFORMED_SCREEN_MASK (1 << 0)

typedef void (*PaintBackgroundProc) (CompScreen   *screen,
				     Region	  region,
				     unsigned int mask);

void
preparePaintScreen (CompScreen *screen,
		    int	       msSinceLastPaint);

void
donePaintScreen (CompScreen *screen);

void
transformToScreenSpace (CompScreen    *screen,
			int	      output,
			float         z,
			CompTransform *transform);

/* XXX: prepareXCoords will be removed */
void
prepareXCoords (CompScreen *screen,
		int	   output,
		float      z);

void
paintTransformedScreen (CompScreen		*screen,
			const ScreenPaintAttrib *sAttrib,
			const CompTransform	*transform,
			Region			region,
			int			output,
			unsigned int	        mask);

/* XXX: applyScreenTransform will be removed */
void
applyScreenTransform (CompScreen	      *screen,
		      const ScreenPaintAttrib *sAttrib,
		      int		      output,
		      CompTransform	      *transform);

Bool
paintScreen (CompScreen		     *screen,
	     const ScreenPaintAttrib *sAttrib,
	     const CompTransform     *transform,
	     Region		     region,
	     int		     output,
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
drawWindowTexture (CompWindow		*w,
		   CompTexture		*texture,
		   const FragmentAttrib	*fragment,
		   unsigned int		mask);

Bool
drawWindow (CompWindow		 *w,
	    const CompTransform  *transform,
	    const FragmentAttrib *fragment,
	    Region		 region,
	    unsigned int	 mask);

Bool
paintWindow (CompWindow		     *w,
	     const WindowPaintAttrib *attrib,
	     const CompTransform     *transform,
	     Region		     region,
	     unsigned int	     mask);

void
paintCursor (CompCursor		 *cursor,
	     const CompTransform *transform,
	     Region		 region,
	     unsigned int	 mask);

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
    Bool       mipmap;
    int        refCount;
};

void
initTexture (CompScreen  *screen,
	     CompTexture *texture);

void
finiTexture (CompScreen  *screen,
	     CompTexture *texture);

CompTexture *
createTexture (CompScreen *screen);

void
destroyTexture (CompScreen  *screen,
		CompTexture *texture);

Bool
imageBufferToTexture (CompScreen   *screen,
		      CompTexture  *texture,
		      char	   *image,
		      unsigned int width,
		      unsigned int height);

Bool
readImageToTexture (CompScreen   *screen,
		    CompTexture  *texture,
		    const char	 *imageFileName,
		    unsigned int *width,
		    unsigned int *height);

Bool
iconToTexture (CompScreen *screen,
	       CompIcon   *icon);

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
disableTexture (CompScreen  *screen,
		CompTexture *texture);


/* screen.c */

#define COMP_SCREEN_OPTION_DETECT_REFRESH_RATE 0
#define COMP_SCREEN_OPTION_LIGHTING	       1
#define COMP_SCREEN_OPTION_REFRESH_RATE	       2
#define COMP_SCREEN_OPTION_HSIZE	       3
#define COMP_SCREEN_OPTION_VSIZE	       4
#define COMP_SCREEN_OPTION_OPACITY_STEP        5
#define COMP_SCREEN_OPTION_UNREDIRECT_FS       6
#define COMP_SCREEN_OPTION_DEFAULT_ICON        7
#define COMP_SCREEN_OPTION_SYNC_TO_VBLANK      8
#define COMP_SCREEN_OPTION_NUMBER_OF_DESKTOPS  9
#define COMP_SCREEN_OPTION_DETECT_OUTPUTS      10
#define COMP_SCREEN_OPTION_OUTPUTS	       11
#define COMP_SCREEN_OPTION_FOCUS_PREVENTION    12
#define COMP_SCREEN_OPTION_NUM		       13

#ifndef GLX_EXT_texture_from_pixmap
#define GLX_BIND_TO_TEXTURE_RGB_EXT        0x20D0
#define GLX_BIND_TO_TEXTURE_RGBA_EXT       0x20D1
#define GLX_BIND_TO_MIPMAP_TEXTURE_EXT     0x20D2
#define GLX_BIND_TO_TEXTURE_TARGETS_EXT    0x20D3
#define GLX_Y_INVERTED_EXT                 0x20D4
#define GLX_TEXTURE_FORMAT_EXT             0x20D5
#define GLX_TEXTURE_TARGET_EXT             0x20D6
#define GLX_MIPMAP_TEXTURE_EXT             0x20D7
#define GLX_TEXTURE_FORMAT_NONE_EXT        0x20D8
#define GLX_TEXTURE_FORMAT_RGB_EXT         0x20D9
#define GLX_TEXTURE_FORMAT_RGBA_EXT        0x20DA
#define GLX_TEXTURE_1D_BIT_EXT             0x00000001
#define GLX_TEXTURE_2D_BIT_EXT             0x00000002
#define GLX_TEXTURE_RECTANGLE_BIT_EXT      0x00000004
#define GLX_TEXTURE_1D_EXT                 0x20DB
#define GLX_TEXTURE_2D_EXT                 0x20DC
#define GLX_TEXTURE_RECTANGLE_EXT          0x20DD
#define GLX_FRONT_LEFT_EXT                 0x20DE
#endif

typedef void (*FuncPtr) (void);
typedef FuncPtr (*GLXGetProcAddressProc) (const GLubyte *procName);

typedef void    (*GLXBindTexImageProc)    (Display	 *display,
					   GLXDrawable	 drawable,
					   int		 buffer,
					   int		 *attribList);
typedef void    (*GLXReleaseTexImageProc) (Display	 *display,
					   GLXDrawable	 drawable,
					   int		 buffer);
typedef void    (*GLXQueryDrawableProc)   (Display	 *display,
					   GLXDrawable	 drawable,
					   int		 attribute,
					   unsigned int  *value);

typedef void (*GLXCopySubBufferProc) (Display     *display,
				      GLXDrawable drawable,
				      int	  x,
				      int	  y,
				      int	  width,
				      int	  height);

typedef int (*GLXGetVideoSyncProc)  (unsigned int *count);
typedef int (*GLXWaitVideoSyncProc) (int	  divisor,
				     int	  remainder,
				     unsigned int *count);

#ifndef GLX_VERSION_1_3
typedef struct __GLXFBConfigRec *GLXFBConfig;
#endif

typedef GLXFBConfig *(*GLXGetFBConfigsProc) (Display *display,
					     int     screen,
					     int     *nElements);
typedef int (*GLXGetFBConfigAttribProc) (Display     *display,
					 GLXFBConfig config,
					 int	     attribute,
					 int	     *value);
typedef GLXPixmap (*GLXCreatePixmapProc) (Display     *display,
					  GLXFBConfig config,
					  Pixmap      pixmap,
					  const int   *attribList);

typedef void (*GLActiveTextureProc) (GLenum texture);
typedef void (*GLClientActiveTextureProc) (GLenum texture);

typedef void (*GLGenProgramsProc) (GLsizei n,
				   GLuint  *programs);
typedef void (*GLDeleteProgramsProc) (GLsizei n,
				      GLuint  *programs);
typedef void (*GLBindProgramProc) (GLenum target,
				   GLuint program);
typedef void (*GLProgramStringProc) (GLenum	  target,
				     GLenum	  format,
				     GLsizei	  len,
				     const GLvoid *string);
typedef void (*GLProgramParameter4fProc) (GLenum  target,
					  GLuint  index,
					  GLfloat x,
					  GLfloat y,
					  GLfloat z,
					  GLfloat w);

typedef void (*GLGenFramebuffersProc) (GLsizei n,
				       GLuint  *framebuffers);
typedef void (*GLDeleteFramebuffersProc) (GLsizei n,
					  GLuint  *framebuffers);
typedef void (*GLBindFramebufferProc) (GLenum target,
				       GLuint framebuffer);
typedef GLenum (*GLCheckFramebufferStatusProc) (GLenum target);
typedef void (*GLFramebufferTexture2DProc) (GLenum target,
					    GLenum attachment,
					    GLenum textarget,
					    GLuint texture,
					    GLint  level);
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

typedef Bool (*DamageCursorRectProc) (CompCursor *c,
				      Bool       initial,
				      BoxPtr     rect);


typedef void (*GetOutputExtentsForWindowProc) (CompWindow	 *w,
					       CompWindowExtents *output);

typedef Bool (*FocusWindowProc) (CompWindow *window);

typedef void (*WindowResizeNotifyProc) (CompWindow *window);

typedef void (*WindowMoveNotifyProc) (CompWindow *window,
				      int	 dx,
				      int	 dy,
				      Bool	 immediate);

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

typedef void (*WindowStateChangeNotifyProc) (CompWindow *window);

typedef void (*OutputChangeNotifyProc) (CompScreen *screen);

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
    Bool       active;
    Cursor     cursor;
    const char *name;
} CompGrab;

typedef struct _CompGroup {
    struct _CompGroup *next;
    unsigned int      refCnt;
    Window	      id;
} CompGroup;

typedef struct _CompStartupSequence {
    struct _CompStartupSequence *next;
    SnStartupSequence		*sequence;
    unsigned int		viewportX;
    unsigned int		viewportY;
} CompStartupSequence;

typedef struct _CompFBConfig {
    GLXFBConfig fbConfig;
    int         yInverted;
    int         mipmap;
    int         textureFormat;
} CompFBConfig;

#define NOTHING_TRANS_FILTER 0
#define SCREEN_TRANS_FILTER  1
#define WINDOW_TRANS_FILTER  2

#define SCREEN_EDGE_LEFT	0
#define SCREEN_EDGE_RIGHT	1
#define SCREEN_EDGE_TOP		2
#define SCREEN_EDGE_BOTTOM	3
#define SCREEN_EDGE_TOPLEFT	4
#define SCREEN_EDGE_TOPRIGHT	5
#define SCREEN_EDGE_BOTTOMLEFT	6
#define SCREEN_EDGE_BOTTOMRIGHT 7
#define SCREEN_EDGE_NUM		8

typedef struct _CompScreenEdge {
    Window	 id;
    unsigned int count;
} CompScreenEdge;

struct _CompIcon {
    CompTexture texture;
    int		width;
    int		height;
};

typedef struct _CompOutput {
    char       *name;
    REGION     region;
    int        width;
    int        height;
    XRectangle workArea;
} CompOutput;

typedef struct _CompCursorImage {
    struct _CompCursorImage *next;

    unsigned long serial;
    Pixmap	  pixmap;
    CompTexture   texture;
    int		  xhot;
    int	          yhot;
    int		  width;
    int	          height;
} CompCursorImage;

struct _CompCursor {
    struct _CompCursor *next;

    CompScreen	    *screen;
    CompCursorImage *image;

    int	x;
    int	y;

    CompMatrix matrix;
};

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
    int		      y;
    int		      hsize;		/* Number of horizontal viewports */
    int		      vsize;		/* Number of vertical viewports */
    unsigned int      nDesktop;
    unsigned int      currentDesktop;
    REGION	      region;
    Region	      damage;
    unsigned long     damageMask;
    Window	      root;
    Window	      overlay;
    Window	      output;
    XWindowAttributes attrib;
    Window	      grabWindow;
    CompFBConfig      glxPixmapFBConfigs[MAX_DEPTH + 1];
    int		      textureRectangle;
    int		      textureNonPowerOfTwo;
    int		      textureEnvCombine;
    int		      textureEnvCrossbar;
    int		      textureBorderClamp;
    GLint	      maxTextureSize;
    int		      fbo;
    int		      fragmentProgram;
    int		      maxTextureUnits;
    Cursor	      invisibleCursor;
    XRectangle        *exposeRects;
    int		      sizeExpose;
    int		      nExpose;
    CompTexture       backgroundTexture;
    Bool	      backgroundLoaded;
    unsigned int      pendingDestroys;
    int		      desktopWindowCount;
    unsigned int      mapNum;
    unsigned int      activeNum;

    CompOutput *outputDev;
    int	       nOutputDev;
    int	       currentOutputDev;

    int overlayWindowCount;

    CompScreenEdge screenEdge[SCREEN_EDGE_NUM];

    SnMonitorContext    *snContext;
    CompStartupSequence *startupSequences;
    unsigned int        startupSequenceTimeoutHandle;

    int filter[3];

    CompGroup *groups;

    CompIcon *defaultIcon;

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
    int		   optimalRedrawTime;
    int		   frameStatus;
    int		   timeMult;
    Bool	   idle;
    int		   timeLeft;
    Bool	   pendingCommands;

    int lastFunctionId;

    CompFunction *fragmentFunctions;
    CompProgram  *fragmentPrograms;

    int saturateFunction[2][64];

    GLfloat projection[16];

    Bool clearBuffers;

    Bool lighting;
    Bool slowAnimations;

    int opacityStep;

    XRectangle workArea;

    unsigned int showingDesktopMask;

    unsigned long *desktopHintData;
    int           desktopHintSize;

    CompCursor      *cursors;
    CompCursorImage *cursorImages;

    GLXGetProcAddressProc    getProcAddress;
    GLXBindTexImageProc      bindTexImage;
    GLXReleaseTexImageProc   releaseTexImage;
    GLXQueryDrawableProc     queryDrawable;
    GLXCopySubBufferProc     copySubBuffer;
    GLXGetVideoSyncProc      getVideoSync;
    GLXWaitVideoSyncProc     waitVideoSync;
    GLXGetFBConfigsProc      getFBConfigs;
    GLXGetFBConfigAttribProc getFBConfigAttrib;
    GLXCreatePixmapProc      createPixmap;

    GLActiveTextureProc       activeTexture;
    GLClientActiveTextureProc clientActiveTexture;

    GLGenProgramsProc	     genPrograms;
    GLDeleteProgramsProc     deletePrograms;
    GLBindProgramProc	     bindProgram;
    GLProgramStringProc	     programString;
    GLProgramParameter4fProc programEnvParameter4f;
    GLProgramParameter4fProc programLocalParameter4f;

    GLGenFramebuffersProc        genFramebuffers;
    GLDeleteFramebuffersProc     deleteFramebuffers;
    GLBindFramebufferProc        bindFramebuffer;
    GLCheckFramebufferStatusProc checkFramebufferStatus;
    GLFramebufferTexture2DProc   framebufferTexture2D;
    GLGenerateMipmapProc         generateMipmap;

    GLXContext ctx;

    CompOption opt[COMP_SCREEN_OPTION_NUM];

    SetScreenOptionProc		 setScreenOption;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;

    InitPluginForScreenProc initPluginForScreen;
    FiniPluginForScreenProc finiPluginForScreen;

    PreparePaintScreenProc	  preparePaintScreen;
    DonePaintScreenProc		  donePaintScreen;
    PaintScreenProc		  paintScreen;
    PaintTransformedScreenProc	  paintTransformedScreen;
    ApplyScreenTransformProc	  applyScreenTransform;
    PaintBackgroundProc		  paintBackground;
    PaintWindowProc		  paintWindow;
    DrawWindowProc		  drawWindow;
    AddWindowGeometryProc	  addWindowGeometry;
    DrawWindowTextureProc	  drawWindowTexture;
    DamageWindowRectProc	  damageWindowRect;
    GetOutputExtentsForWindowProc getOutputExtentsForWindow;
    FocusWindowProc		  focusWindow;

    PaintCursorProc      paintCursor;
    DamageCursorRectProc damageCursorRect;

    WindowResizeNotifyProc windowResizeNotify;
    WindowMoveNotifyProc   windowMoveNotify;
    WindowGrabNotifyProc   windowGrabNotify;
    WindowUngrabNotifyProc windowUngrabNotify;

    WindowStateChangeNotifyProc windowStateChangeNotify;

    OutputChangeNotifyProc outputChangeNotify;

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
setCurrentOutput (CompScreen *s,
		  int	     outputNum);

void
updateScreenBackground (CompScreen  *screen,
			CompTexture *texture);

void
detectRefreshRateOfScreen (CompScreen *s);

void
showOutputWindow (CompScreen *s);

void
hideOutputWindow (CompScreen *s);

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
		Cursor     cursor,
		const char *name);

void
updateScreenGrab (CompScreen *s,
		  int        index,
		  Cursor     cursor);

void
removeScreenGrab (CompScreen *s,
		  int	     index,
		  XPoint     *restorePointer);

Bool
otherScreenGrabExist (CompScreen *s, ...);

Bool
addScreenAction (CompScreen *s,
		 CompAction *action);

void
removeScreenAction (CompScreen *s,
		    CompAction *action);

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
toolkitAction (CompScreen *s,
	       Atom	  toolkitAction,
	       Time       eventTime,
	       Window	  window,
	       long	  data0,
	       long	  data1,
	       long	  data2);

void
runCommand (CompScreen *s,
	    const char *command);

void
moveScreenViewport (CompScreen *s,
		    int	       tx,
		    int	       ty,
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
leaveShowDesktopMode (CompScreen *s,
		      CompWindow *window);

void
sendWindowActivationRequest (CompScreen *s,
			     Window	id);

void
screenTexEnvMode (CompScreen *s,
		  GLenum     mode);

void
screenLighting (CompScreen *s,
		Bool       lighting);

void
enableScreenEdge (CompScreen *s,
		  int	     edge);

void
disableScreenEdge (CompScreen *s,
		   int	      edge);

Window
getTopWindow (CompScreen *s);

void
makeScreenCurrent (CompScreen *s);

void
finishScreenDrawing (CompScreen *s);

int
outputDeviceForPoint (CompScreen *s,
		      int	 x,
		      int	 y);

void
getCurrentOutputExtents (CompScreen *s,
			 int	    *x1,
			 int	    *y1,
			 int	    *x2,
			 int	    *y2);

void
getWorkareaForOutput (CompScreen *s,
		      int	 output,
		      XRectangle *area);

void
setNumberOfDesktops (CompScreen   *s,
		     unsigned int nDesktop);

void
setCurrentDesktop (CompScreen   *s,
		   unsigned int desktop);

void
setDefaultViewport (CompScreen *s);

void
outputChangeNotify (CompScreen *s);

void
clearScreenOutput (CompScreen   *s,
		   int	        output,
		   unsigned int mask);

Bool
updateDefaultIcon (CompScreen *screen);

CompCursor *
findCursorAtScreen (CompScreen *screen);

CompCursorImage *
findCursorImageAtScreen (CompScreen    *screen,
			 unsigned long serial);


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

struct _CompWindowExtents {
    int left;
    int right;
    int top;
    int bottom;
};

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
    int		      serverWidth;
    int		      serverHeight;
    int		      serverBorderWidth;
    Window	      transientFor;
    Window	      clientLeader;
    XSizeHints	      sizeHints;
    Pixmap	      pixmap;
    CompTexture       *texture;
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
    unsigned int      mwmFunc;
    Bool	      invisible;
    Bool	      destroyed;
    Bool	      damaged;
    Bool	      redirected;
    Bool	      managed;
    int		      destroyRefCnt;
    int		      unmapRefCnt;

    unsigned int initialViewportX;
    unsigned int initialViewportY;

    Time initialTimestamp;
    Bool initialTimestampSet;

    Bool placed;
    Bool minimized;
    Bool inShowDesktopMode;
    Bool shaded;
    Bool hidden;

    unsigned int desktop;

    int pendingUnmaps;
    int pendingMaps;

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

    CompWindowExtents input;
    CompWindowExtents output;

    CompStruts *struts;

    CompIcon **icon;
    int	     nIcon;

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

    Bool closeRequests;
    Time lastCloseRequestTime;

    XRectangle *damageRects;
    int	       sizeDamage;
    int	       nDamage;

    GLfloat  *vertices;
    int      vertexSize;
    GLushort *indices;
    int      indexSize;
    int      vCount;
    int      texUnits;
    int      texCoordSize;
    int      indexCount;

    /* must be set by addWindowGeometry */
    DrawWindowGeometryProc drawWindowGeometry;

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

void
recalcWindowActions (CompWindow *w);

unsigned int
constrainWindowState (unsigned int state,
		      unsigned int actions);

unsigned int
getWindowType (CompDisplay *display,
	       Window      id);

void
recalcWindowType (CompWindow *w);

void
getMwmHints (CompDisplay  *display,
	     Window	  id,
	     unsigned int *func,
	     unsigned int *decor);

unsigned int
getProtocols (CompDisplay *display,
	      Window      id);

unsigned int
getWindowProp (CompDisplay  *display,
	       Window	    id,
	       Atom	    property,
	       unsigned int defaultValue);

void
setWindowProp (CompDisplay  *display,
	       Window       id,
	       Atom	    property,
	       unsigned int value);

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

void
updateTransientHint (CompWindow *w);

Window
getClientLeader (CompWindow *w);

char *
getStartupId (CompWindow *w);

int
getWmState (CompDisplay *display,
	    Window      id);

void
setWmState (CompDisplay *display,
	    int		state,
	    Window      id);

void
setWindowFrameExtents (CompWindow	 *w,
		       CompWindowExtents *input);

void
updateWindowOutputExtents (CompWindow *w);

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
sendConfigureNotify (CompWindow *w);

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
	    Bool       damage,
	    Bool       immediate);

void
configureXWindow (CompWindow	 *w,
		  unsigned int	 valueMask,
		  XWindowChanges *xwc);

void
moveResizeWindow (CompWindow     *w,
		  XWindowChanges *xwc,
		  unsigned int   xwcm,
		  int            gravity);

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
addWindowDamageRect (CompWindow *w,
		     BoxPtr     rect);

void
getOutputExtentsForWindow (CompWindow	     *w,
			   CompWindowExtents *output);

void
addWindowDamage (CompWindow *w);

void
damageWindowOutputExtents (CompWindow *w);

Bool
damageWindowRect (CompWindow *w,
		  Bool       initial,
		  BoxPtr     rect);

void
damageTransformedWindowRect (CompWindow *w,
			     float	xScale,
			     float	yScale,
			     float	xTranslate,
			     float	yTranslate,
			     BoxPtr     rect);

Bool
focusWindow (CompWindow *w);

void
windowResizeNotify (CompWindow *w);

void
windowMoveNotify (CompWindow *w,
		  int	     dx,
		  int	     dy,
		  Bool	     immediate);

void
windowGrabNotify (CompWindow   *w,
		  int	       x,
		  int	       y,
		  unsigned int state,
		  unsigned int mask);

void
windowUngrabNotify (CompWindow *w);

void
windowStateChangeNotify (CompWindow *w);

void
moveInputFocusToWindow (CompWindow *w);

void
updateWindowSize (CompWindow *w);

void
raiseWindow (CompWindow *w);

void
lowerWindow (CompWindow *w);

void
restackWindowAbove (CompWindow *w,
		    CompWindow *sibling);

void
restackWindowBelow (CompWindow *w,
		    CompWindow *sibling);

void
updateWindowAttributes (CompWindow *w,
			Bool	   aboveFs);

void
activateWindow (CompWindow *w);

void
closeWindow (CompWindow *w,
	     Time	serverTime);

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

void
maximizeWindow (CompWindow *w,
		int	   state);

Bool
getWindowUserTime (CompWindow *w,
		   Time       *time);

void
setWindowUserTime (CompWindow *w,
		   Time       time);

Bool
focusWindowOnMap (CompWindow *w);

void
unredirectWindow (CompWindow *w);

void
redirectWindow (CompWindow *w);

void
defaultViewportForWindow (CompWindow *w,
			  int	     *vx,
			  int        *vy);

CompIcon *
getWindowIcon (CompWindow *w,
	       int	  width,
	       int	  height);

void
freeWindowIcons (CompWindow *w);

int
outputDeviceForWindow (CompWindow *w);

Bool
onCurrentDesktop (CompWindow *w);

void
setDesktopForWindow (CompWindow   *w,
		     unsigned int desktop);


/* plugin.c */

#define HOME_PLUGINDIR ".compiz/plugins"

typedef int (*GetVersionProc) (CompPlugin *plugin,
			       int	  version);

typedef Bool (*InitPluginProc) (CompPlugin *plugin);
typedef void (*FiniPluginProc) (CompPlugin *plugin);

typedef enum {
    CompPluginRuleBefore,
    CompPluginRuleAfter,
    CompPluginRuleRequire
} CompPluginRule;

typedef struct _CompPluginDep {
    CompPluginRule rule;
    char	   *name;
} CompPluginDep;

typedef struct _CompPluginFeature {
    char *name;
} CompPluginFeature;

typedef struct _CompPluginVTable {
    char *name;
    char *shortDesc;
    char *longDesc;

    GetVersionProc getVersion;

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

    CompPluginFeature *features;
    int		      nFeatures;
} CompPluginVTable;

typedef CompPluginVTable *(*PluginGetInfoProc) (void);

typedef Bool (*LoadPluginProc) (CompPlugin *p,
				char	   *path,
				char	   *name);

typedef void (*UnloadPluginProc) (CompPlugin *p);

typedef char **(*ListPluginsProc) (char *path,
				   int	*n);

extern LoadPluginProc   loaderLoadPlugin;
extern UnloadPluginProc loaderUnloadPlugin;
extern ListPluginsProc  loaderListPlugins;

struct _CompPlugin {
    CompPlugin       *next;
    CompPrivate	     devPrivate;
    char	     *devType;
    CompPluginVTable *vTable;
};

Bool
initPluginForDisplay (CompPlugin  *p,
		      CompDisplay *d);

void
finiPluginForDisplay (CompPlugin  *p,
		      CompDisplay *d);

Bool
initPluginForScreen (CompPlugin *p,
		     CompScreen *s);

void
finiPluginForScreen (CompPlugin *p,
		     CompScreen *s);

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

CompPluginFeature *
findActiveFeature (char *name);

CompPlugin *
loadPlugin (char *plugin);

void
unloadPlugin (CompPlugin *p);

Bool
pushPlugin (CompPlugin *p);

CompPlugin *
popPlugin (void);

CompPlugin *
getPlugins (void);

char **
availablePlugins (int *n);


/* session.c */

void
initSession (char *smPrevClientId);

void
closeSession (void);


/* fragment.c */

#define MAX_FRAGMENT_FUNCTIONS 16

struct _FragmentAttrib {
    GLushort opacity;
    GLushort brightness;
    GLushort saturation;
    int	     nTexture;
    int	     function[MAX_FRAGMENT_FUNCTIONS];
    int	     nFunction;
    int	     nParam;
};

CompFunctionData *
createFunctionData (void);

void
destroyFunctionData (CompFunctionData *data);

Bool
addTempHeaderOpToFunctionData (CompFunctionData *data,
			       char		*name);

Bool
addParamHeaderOpToFunctionData (CompFunctionData *data,
				char		 *name);

Bool
addAttribHeaderOpToFunctionData (CompFunctionData *data,
				 char		  *name);

#define COMP_FETCH_TARGET_2D   0
#define COMP_FETCH_TARGET_RECT 1
#define COMP_FETCH_TARGET_NUM  2

Bool
addFetchOpToFunctionData (CompFunctionData *data,
			  char		   *dst,
			  char		   *offset,
			  int		   target);

Bool
addColorOpToFunctionData (CompFunctionData *data,
			  char		   *dst,
			  char		   *src);

Bool
addDataOpToFunctionData (CompFunctionData *data,
			 char		  *str);

Bool
addBlendOpToFunctionData (CompFunctionData *data,
			  char		   *str);

int
createFragmentFunction (CompScreen	 *s,
			char		 *name,
			CompFunctionData *data);

void
destroyFragmentFunction (CompScreen *s,
			 int	    id);

int
getSaturateFragmentFunction (CompScreen  *s,
			     CompTexture *texture,
			     int	 param);

int
allocFragmentTextureUnits (FragmentAttrib *attrib,
			   int		  nTexture);

int
allocFragmentParameters (FragmentAttrib *attrib,
			 int		nParam);

void
addFragmentFunction (FragmentAttrib *attrib,
		     int	    function);

void
initFragmentAttrib (FragmentAttrib	    *attrib,
		    const WindowPaintAttrib *paint);

Bool
enableFragmentAttrib (CompScreen     *s,
		      FragmentAttrib *attrib,
		      Bool	     *blending);

void
disableFragmentAttrib (CompScreen     *s,
		       FragmentAttrib *attrib);


/* matrix.c */

void
matrixRotate (CompTransform *transform,
	      float	    angle,
	      float	    x,
	      float	    y,
	      float	    z);

void
matrixScale (CompTransform *transform,
	     float	   x,
	     float	   y,
	     float	   z);

void
matrixTranslate (CompTransform *transform,
		 float	       x,
		 float	       y,
		 float	       z);


/* cursor.c */

void
addCursor (CompScreen *s);

Bool
damageCursorRect (CompCursor *c,
		  Bool       initial,
		  BoxPtr     rect);

void
addCursorDamageRect (CompCursor *c,
		     BoxPtr     rect);

void
addCursorDamage (CompCursor *c);

void
updateCursor (CompCursor    *c,
	      int	    x,
	      int	    y,
	      unsigned long serial);

#ifdef  __cplusplus
}
#endif

#endif
