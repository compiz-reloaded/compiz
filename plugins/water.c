/*
 * Copyright © 2006 Novell, Inc.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <compiz.h>

#define TEXTURE_SIZE 256

#define K 0.1964f

#define PROGRAM_BUMP     0
#define PROGRAM_BUMP_SAT 1
#define PROGRAM_WATER    2
#define PROGRAM_NUM      3

#define TEXTURE_NUM  3

#define TINDEX(ws, i) (((ws)->tIndex + (i)) % TEXTURE_NUM)

#define CLAMP(v, max, min) \
    if ((v) > (max))	   \
	(v) = (max);	   \
    else if ((v) < (min))  \
	(v) = (min)

#define WATER_INITIATE_KEY_DEFAULT       "Super_L"
#define WATER_INITIATE_MODIFIERS_DEFAULT (CompPressMask | ControlMask)

#define WATER_TERMINATE_KEY_DEFAULT       "Super_L"
#define WATER_TERMINATE_MODIFIERS_DEFAULT CompReleaseMask

static int displayPrivateIndex;

typedef struct _WaterDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
} WaterDisplay;

#define WATER_SCREEN_OPTION_INITIATE  0
#define WATER_SCREEN_OPTION_TERMINATE 1
#define WATER_SCREEN_OPTION_NUM       2

typedef struct _WaterScreen {
    int	waterTime;

    CompOption opt[WATER_SCREEN_OPTION_NUM];

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    DrawWindowTextureProc  drawWindowTexture;

    int grabIndex;
    int width, height;

    XPoint p;

    GLuint program[PROGRAM_NUM];
    GLuint texture[TEXTURE_NUM];

    int     tIndex;
    GLenum  target;
    GLfloat tx, ty;

    int count;

    GLuint fbo;
    GLint  fboStatus;

    void	  *data;
    float	  *d0;
    float	  *d1;
    unsigned char *t0;
} WaterScreen;

#define GET_WATER_DISPLAY(d)				      \
    ((WaterDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define WATER_DISPLAY(d)		     \
    WaterDisplay *wd = GET_WATER_DISPLAY (d)

#define GET_WATER_SCREEN(s, wd)					  \
    ((WaterScreen *) (s)->privates[(wd)->screenPrivateIndex].ptr)

#define WATER_SCREEN(s)							   \
    WaterScreen *ws = GET_WATER_SCREEN (s, GET_WATER_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
waterGetScreenOptions (CompScreen *screen,
		       int	  *count)
{
    WATER_SCREEN (screen);

    *count = NUM_OPTIONS (ws);
    return ws->opt;
}

static Bool
waterSetScreenOption (CompScreen      *screen,
		      char	      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    WATER_SCREEN (screen);

    o = compFindOption (ws->opt, NUM_OPTIONS (ws), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case WATER_SCREEN_OPTION_INITIATE:
	if (addScreenBinding (screen, &value->bind))
	{
	    removeScreenBinding (screen, &o->value.bind);

	    if (compSetBindingOption (o, value))
		return TRUE;
	}
	break;
    case WATER_SCREEN_OPTION_TERMINATE:
	if (compSetBindingOption (o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
waterScreenInitOptions (WaterScreen *ws,
			Display     *display)
{
    CompOption *o;

    o = &ws->opt[WATER_SCREEN_OPTION_INITIATE];
    o->name			  = "initiate";
    o->shortDesc		  = "Initiate";
    o->longDesc			  = "Show switcher";
    o->type			  = CompOptionTypeBinding;
    o->value.bind.type		  = CompBindingTypeKey;
    o->value.bind.u.key.modifiers = WATER_INITIATE_MODIFIERS_DEFAULT;
    o->value.bind.u.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (WATER_INITIATE_KEY_DEFAULT));

    o = &ws->opt[WATER_SCREEN_OPTION_TERMINATE];
    o->name			  = "terminate";
    o->shortDesc		  = "Terminate";
    o->longDesc			  = "End switching";
    o->type			  = CompOptionTypeBinding;
    o->value.bind.type		  = CompBindingTypeKey;
    o->value.bind.u.key.modifiers = WATER_TERMINATE_MODIFIERS_DEFAULT;
    o->value.bind.u.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (WATER_TERMINATE_KEY_DEFAULT));
}

static const char *saturateFpString =
    "MUL temp, rgb, { 1.0, 1.0, 1.0, 0.0 };"
    "DP3 temp, temp, program.local[0];"
    "LRP rgb.xyz, program.local[0].w, rgb, temp;";

static const char *bumpFpString =
    "!!ARBfp1.0"

    "PARAM light0color = state.light[0].diffuse;"
    "PARAM ambient = state.lightmodel.ambient;"

    "TEMP rgb, normal, temp, total, bump;"

    /* get texture data */
    "TEX rgb, fragment.texcoord[0], texture[0], %s;"
    "TEX normal, fragment.texcoord[1], texture[1], %s;"

    /* remove scale and bias from the normal map */
    "SUB normal, normal, 0.5;"
    "MUL normal, normal, 2.0;"

    /* normalize the normal map */
    "DP3 temp, normal, normal;"
    "RSQ temp, temp.x;"
    "MUL normal, normal, temp;"

    /* normal dot lightdir, this should eventually be changed to a real light
       vector */
    "DP3 bump, normal, { 0.0, 0.0, 1.0, 0.0 };"

    /* add light0 color */
    "MUL_SAT total, bump, light0color;"
    "MUL total, total, 1.9;"

    /* diffuse per-vertex lighting, opacity and brightness */
    "MUL rgb, fragment.color, rgb;"

    /* saturation */
    "%s"

    /* multiply by regular texture map color */
    "MUL_SAT result.color, rgb, total;"

    "END";

static const char *waterFpString =
    "!!ARBfp1.0"

    "PARAM param = program.local[0];"
    "ATTRIB t11  = fragment.texcoord[0];"

    "TEMP t01, t21, t10, t12;"
    "TEMP c11, c01, c21, c10, c12;"
    "TEMP prev, v, temp, accel;"

    "TEX prev, t11, texture[0], %s;"
    "TEX c11,  t11, texture[1], %s;"

    /* sample offsets */
    "ADD t01, t11, { - %f, 0.0, 0.0, 0.0 };"
    "ADD t21, t11, {   %f, 0.0, 0.0, 0.0 };"
    "ADD t10, t11, { 0.0, - %f, 0.0, 0.0 };"
    "ADD t12, t11, { 0.0,   %f, 0.0, 0.0 };"

    /* fetch nesseccary samples */
    "TEX c01, t01, texture[1], %s;"
    "TEX c21, t21, texture[1], %s;"
    "TEX c10, t10, texture[1], %s;"
    "TEX c12, t12, texture[1], %s;"

    /* x/y normals from height */
    "MOV v, { 0.0, 0.0, 1.0, 0.0 };"
    "SUB v.x, c12.w, c10.w;"
    "SUB v.y, c21.w, c01.w;"

    /* bumpiness */
    "MUL v, v, { 4.0, 4.0, 1.0, 0.0 };"

    /* normalize */
    "MAD temp, v.x, v.x, 1.0;"
    "MAD temp, v.y, v.y, temp;"
    "RSQ temp, temp.x;"
    "MUL_SAT v, v, temp;"

    /* done with computing the normal, continue with computing the next
       height value */
    "ADD accel, c10, c12;"
    "ADD accel, c01, accel;"
    "ADD accel, c21, accel;"
    "MAD accel, -4.0, c11, accel;"

    /* store new height in alpha component */
    "MAD v.w, 2.0, c11, -prev.w;"
    "MAD v.w, accel, param.x, v.w;"

    /* fade out height */
    "MUL v.w, v.w, param.y;"

    "MOV result.color, v;"

    "END";

static int
loadWaterPrograms (CompScreen *s)
{
    char buffer[1024];

    WATER_SCREEN (s);

    if (!ws->program[PROGRAM_WATER])
	(*s->genPrograms) (1, &ws->program[PROGRAM_WATER]);

    if (ws->target == GL_TEXTURE_2D)
	sprintf (buffer, waterFpString,
		 "2D", "2D",
		 1.0f / ws->width,  1.0f / ws->width,
		 1.0f / ws->height, 1.0f / ws->height,
		 "2D", "2D", "2D", "2D");
    else
	sprintf (buffer, waterFpString,
		 "RECT", "RECT",
		 1.0f, 1.0f, 1.0f, 1.0f,
		 "RECT", "RECT", "RECT", "RECT");

    (*s->bindProgram) (GL_FRAGMENT_PROGRAM_ARB, ws->program[PROGRAM_WATER]);
    (*s->programString) (GL_FRAGMENT_PROGRAM_ARB,
			 GL_PROGRAM_FORMAT_ASCII_ARB,
			 strlen (buffer), buffer);

    return 1;
}

static int
loadBumpMapPrograms (CompScreen *s)
{
    char buffer[1024];

    WATER_SCREEN (s);

    if (!s->fragmentProgram)
    {
	fprintf (stderr, "%s: water: GL_ARB_fragment_program is missing\n",
		 programName);
	return 0;
    }

    if (ws->target == GL_TEXTURE_2D)
	sprintf (buffer, bumpFpString, "2D", "2D", "");
    else
	sprintf (buffer, bumpFpString, "RECT", "RECT", "");

    if (!ws->program[PROGRAM_BUMP])
	(*s->genPrograms) (1, &ws->program[PROGRAM_BUMP]);

    (*s->bindProgram) (GL_FRAGMENT_PROGRAM_ARB, ws->program[PROGRAM_BUMP]);
    (*s->programString) (GL_FRAGMENT_PROGRAM_ARB,
			 GL_PROGRAM_FORMAT_ASCII_ARB,
			 strlen (buffer), buffer);

    if (ws->target == GL_TEXTURE_2D)
	sprintf (buffer, bumpFpString, "2D", "2D", saturateFpString);
    else
	sprintf (buffer, bumpFpString, "RECT", "RECT", saturateFpString);

    if (!ws->program[PROGRAM_BUMP_SAT])
	(*s->genPrograms) (1, &ws->program[PROGRAM_BUMP_SAT]);

    (*s->bindProgram) (GL_FRAGMENT_PROGRAM_ARB, ws->program[PROGRAM_BUMP_SAT]);
    (*s->programString) (GL_FRAGMENT_PROGRAM_ARB,
			 GL_PROGRAM_FORMAT_ASCII_ARB,
			 strlen (buffer), buffer);

    return 1;
}

static int
ensureBumpMapPrograms (CompScreen *s)
{
    WATER_SCREEN (s);

    return (ws->program[PROGRAM_BUMP] != 0);
}

static void
allocTexture (CompScreen *s,
	      int	 index)
{
    WATER_SCREEN (s);

    glGenTextures (1, &ws->texture[index]);
    glBindTexture (ws->target, ws->texture[index]);

    glTexParameteri (ws->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (ws->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (ws->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (ws->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D (ws->target,
		  0,
		  GL_RGBA,
		  ws->width,
		  ws->height,
		  0,
		  GL_BGRA,
		  GL_UNSIGNED_BYTE,
		  ws->t0);
}

static int
fboPrologue (CompScreen *s,
	     int	tIndex)
{
    WATER_SCREEN (s);

    if (!ws->fbo)
	return 0;

    if (!ws->texture[tIndex])
	allocTexture (s, tIndex);

    (*s->bindFramebuffer) (GL_FRAMEBUFFER_EXT, ws->fbo);

    (*s->framebufferTexture2D) (GL_FRAMEBUFFER_EXT,
				GL_COLOR_ATTACHMENT0_EXT,
				ws->target, ws->texture[tIndex],
				0);

    glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
    glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);

    /* check status the first time */
    if (!ws->fboStatus)
    {
	ws->fboStatus = (*s->checkFramebufferStatus) (GL_FRAMEBUFFER_EXT);
	if (ws->fboStatus != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
	    fprintf (stderr, "%s: water: framebuffer incomplete\n",
		     programName);

	    (*s->bindFramebuffer) (GL_FRAMEBUFFER_EXT, 0);
	    (*s->deleteFramebuffers) (1, &ws->fbo);

	    glDrawBuffer (GL_BACK);
	    glReadBuffer (GL_BACK);

	    ws->fbo = 0;

	    return 0;
	}
    }

    glViewport (0, 0, ws->width, ws->height);
    glMatrixMode (GL_PROJECTION);
    glPushMatrix ();
    glLoadIdentity ();
    glOrtho (0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    glLoadIdentity ();

    return 1;
}

static void
fboEpilogue (CompScreen *s)
{
    (*s->bindFramebuffer) (GL_FRAMEBUFFER_EXT, 0);

    glViewport (0, 0, s->width, s->height);
    glMatrixMode (GL_PROJECTION);
    glPopMatrix ();
    glMatrixMode (GL_MODELVIEW);
    glPopMatrix ();

    glDrawBuffer (GL_BACK);
    glReadBuffer (GL_BACK);
}

static int
fboUpdate (CompScreen *s,
	   float      dt,
	   float      fade)
{
    WATER_SCREEN (s);

    if (!fboPrologue (s, TINDEX (ws, 1)))
	return 0;

    if (!ws->texture[TINDEX (ws, 2)])
	allocTexture (s, TINDEX (ws, 2));

    if (!ws->texture[TINDEX (ws, 0)])
	allocTexture (s, TINDEX (ws, 0));

    glEnable (ws->target);

    (*s->activeTexture) (GL_TEXTURE0_ARB);
    glBindTexture (ws->target, ws->texture[TINDEX (ws, 2)]);

    glTexParameteri (ws->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (ws->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    (*s->activeTexture) (GL_TEXTURE1_ARB);
    glBindTexture (ws->target, ws->texture[TINDEX (ws, 0)]);
    glTexParameteri (ws->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (ws->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glEnable (GL_FRAGMENT_PROGRAM_ARB);
    (*s->bindProgram) (GL_FRAGMENT_PROGRAM_ARB, ws->program[PROGRAM_WATER]);

    (*s->programLocalParameter4f) (GL_FRAGMENT_PROGRAM_ARB, 0,
				   dt * K, fade, 1.0f, 1.0f);

    glBegin (GL_QUADS);

    glTexCoord2f (0.0f, 0.0f);
    glVertex2f   (0.0f, 0.0f);
    glTexCoord2f (ws->tx, 0.0f);
    glVertex2f   (1.0f, 0.0f);
    glTexCoord2f (ws->tx, ws->ty);
    glVertex2f   (1.0f, 1.0f);
    glTexCoord2f (0.0f, ws->ty);
    glVertex2f   (0.0f, 1.0f);

    glEnd ();

    glDisable (GL_FRAGMENT_PROGRAM_ARB);

    glBindTexture (ws->target, 0);
    glTexParameteri (ws->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (ws->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    (*s->activeTexture) (GL_TEXTURE0_ARB);
    glTexParameteri (ws->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (ws->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture (ws->target, 0);

    fboEpilogue (s);

    /* increment texture index */
    ws->tIndex = TINDEX (ws, 1);

    return 1;
}

static int
fboVertices (CompScreen *s,
	     GLenum     type,
	     XPoint     *p,
	     int	n,
	     float	v)
{
    WATER_SCREEN (s);

    if (!fboPrologue (s, TINDEX (ws, 0)))
	return 0;

    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glColor4f (0.0f, 0.0f, 0.0f, v);

    glPointSize (3.0f);
    glLineWidth (1.0f);

    glScalef (1.0f / ws->width, 1.0f / ws->height, 1.0);
    glTranslatef (0.5f, 0.5f, 0.0f);

    glBegin (type);

    while (n--)
    {
	glVertex2i (p->x, p->y);
	p++;
    }

    glEnd ();

    glColor4usv (defaultColor);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    fboEpilogue (s);
}

static void
softwareUpdate (CompScreen *s,
		float      dt,
		float      fade)
{
    float	   *dTmp;
    int		   i, j;
    float	   v[2], inv;
    float	   accel, value;
    unsigned char *t0, *t;
    int		  dWidth, dHeight;

    WATER_SCREEN (s);

    if (!ws->texture[TINDEX (ws, 0)])
	allocTexture (s, TINDEX (ws, 0));

    dt   *= K * 2.0f;
    fade *= 0.99f;

    dWidth  = ws->width  + 2;
    dHeight = ws->height + 2;

#define D(d, i, j) (*((d) + dWidth * (i) + (j)))

    for (i = 1; i < dHeight - 1; i++)
    {
	for (j = 1; j < dWidth - 1; j++)
	{
	    accel = dt * (D (ws->d1, i - 1, j) +
			  D (ws->d1, i + 1, j) +
			  D (ws->d1, i, j - 1) +
			  D (ws->d1, i, j + 1) - 4.0f * D (ws->d1, i, j));

	    value = (2.0f * D (ws->d1, i, j) - D (ws->d0, i, j) + accel) * fade;

	    CLAMP (value, 0.999f, 0.001f);

	    D (ws->d0, i, j) = value;
	}
    }

    /* update border */
    memcpy (ws->d0, ws->d0 + dWidth, dWidth * sizeof (GLfloat));
    memcpy (ws->d0 + dWidth * (dHeight - 1),
	    ws->d0 + dWidth * (dHeight - 2),
	    dWidth * sizeof (GLfloat));

    for (i = 1; i < dHeight - 1; i++)
    {
	D (ws->d0, i, 0)	  = D (ws->d0, i, 1);
	D (ws->d0, i, dWidth - 1) = D (ws->d0, i, dWidth - 2);
    }

#undef D

#define D(d, i, j) (*((d) + dWidth * (i + 1) + (j + 1)))

    /* update texture */
    for (i = 0; i < ws->height; i++)
    {
	t0 = ws->t0 + (ws->width * 4 * i);

	for (j = 0; j < ws->width; j++)
	{
	    v[0] = (D (ws->d1, i + 1, j) - D (ws->d1, i - 1, j)) * 4.0f;
	    v[1] = (D (ws->d1, i, j + 1) - D (ws->d1, i, j - 1)) * 4.0f;

	    inv = 1.0f / sqrtf (v[0] * v[0] + v[1] * v[1] + 1.0f);
	    v[0] *= inv;
	    v[1] *= inv;

	    CLAMP (v[0], 1.0f, 0.0f);
	    CLAMP (v[1], 1.0f, 0.0f);

	    /* store normal map in RGB components */
	    t = t0 + (j * 4);
	    t[0] = (unsigned char) (inv  * 255.0f);
	    t[1] = (unsigned char) (v[1] * 255.0f);
	    t[2] = (unsigned char) (v[0] * 255.0f);
	}
    }

#undef D

    /* swap height maps */
    dTmp   = ws->d0;
    ws->d0 = ws->d1;
    ws->d1 = dTmp;

    if (ws->texture[TINDEX (ws, 0)])
    {
	glBindTexture (ws->target, ws->texture[TINDEX (ws, 0)]);
	glTexImage2D (ws->target,
		      0,
		      GL_RGBA,
		      ws->width,
		      ws->height,
		      0,
		      GL_BGRA,
		      GL_UNSIGNED_BYTE,
		      ws->t0);
    }
}


#define SET(x, y, v) *((ws->d1) + (ws->width + 2) * (y) + (x)) = (v)

static void
softwarePoints (CompScreen *s,
		XPoint	   *p,
		int	   n,
		float	   add)
{
    WATER_SCREEN (s);

    while (n--)
    {
	SET (p->x - 1, p->y - 1, add);
	SET (p->x, p->y - 1, add);
	SET (p->x + 1, p->y - 1, add);

	SET (p->x - 1, p->y, add);
	SET (p->x, p->y, add);
	SET (p->x + 1, p->y, add);

	SET (p->x - 1, p->y + 1, add);
	SET (p->x, p->y + 1, add);
	SET (p->x + 1, p->y + 1, add);

	p++;
    }
}

/* bresenham */
static void
softwareLines (CompScreen *s,
	       XPoint	  *p,
	       int	  n,
	       float	  v)
{
    int	 x1, y1, x2, y2;
    Bool steep;
    int  tmp;
    int  deltaX, deltaY;
    int  error = 0;
    int  yStep;
    int  x, y;

    WATER_SCREEN (s);

#define SWAP(v0, v1) \
    tmp = v0;	     \
    v0 = v1;	     \
    v1 = tmp

    while (n > 1)
    {
	x1 = p->x;
	y1 = p->y;

	p++;
	n--;

	x2 = p->x;
	y2 = p->y;

	p++;
	n--;

	steep = abs (y2 - y1) > abs (x2 - x1);
	if (steep)
	{
	    SWAP (x1, y1);
	    SWAP (x2, y2);
	}

	if (x1 > x2)
	{
	    SWAP (x1, x2);
	    SWAP (y1, y2);
	}

#undef SWAP

	deltaX = x2 - x1;
	deltaY = abs (y2 - y1);

	y = y1;
	if (y1 < y2)
	    yStep = 1;
	else
	    yStep = -1;

	for (x = x1; x <= x2; x++)
	{
	    if (steep)
	    {
		SET (y, x, v);
	    }
	    else
	    {
		SET (x, y, v);
	    }

	    error += deltaY;
	    if (2 * error >= deltaX)
	    {
		y += yStep;
		error -= deltaX;
	    }
	}
    }
}

#undef SET

static void
softwareVertices (CompScreen *s,
		  GLenum     type,
		  XPoint     *p,
		  int	     n,
		  float	     v)
{
    switch (type) {
    case GL_POINTS:
	softwarePoints (s, p, n, v);
	break;
    case GL_LINES:
	softwareLines (s, p, n, v);
	break;
    }
}

static void
waterUpdate (CompScreen *s,
	     float	dt)
{
    GLfloat fade = 1.0f;

    WATER_SCREEN (s);

    if (ws->count < 1000)
    {
	if (ws->count > 1)
	    fade = 0.90f + ws->count / 10000.0f;
	else
	    fade = 0.0f;
    }
    else
	fade = 1.0f;

    if (!fboUpdate (s, dt, fade))
	softwareUpdate (s, dt, fade);
}

static void
scaleVertices (CompScreen *s,
	       XPoint	  *p,
	       int	  n)
{
    WATER_SCREEN (s);

    while (n--)
    {
	p[n].x = (ws->width  * p[n].x) / s->width;
	p[n].y = (ws->height * p[n].y) / s->height;
    }
}

static void
waterVertices (CompScreen *s,
	       GLenum     type,
	       XPoint     *p,
	       int	  n,
	       float	  v)
{
    WATER_SCREEN (s);

    if (!ensureBumpMapPrograms (s))
	return;

    scaleVertices (s, p, n);

    if (!fboVertices (s, type, p, n, v))
	softwareVertices (s, type, p, n, v);

    if (ws->count < 3000)
	ws->count = 3000;
}

static void
waterReset (CompScreen *s)
{
    int size, i, j;

    WATER_SCREEN (s);

    ws->height = TEXTURE_SIZE;
    ws->width  = (ws->height * s->width) / s->height;

    if (s->textureNonPowerOfTwo ||
	(POWER_OF_TWO (ws->width) && POWER_OF_TWO (ws->height)))
    {
	ws->target = GL_TEXTURE_2D;
	ws->tx = ws->ty = 1.0f;
    }
    else
    {
	ws->target = GL_TEXTURE_RECTANGLE_NV;
	ws->tx = ws->width;
	ws->ty = ws->height;
    }

    if (!loadBumpMapPrograms (s))
	return;

    if (s->fbo)
    {
	loadWaterPrograms (s);
	if (!ws->fbo)
	    (*s->genFramebuffers) (1, &ws->fbo);
    }

    ws->fboStatus = 0;

    for (i = 0; i < TEXTURE_NUM; i++)
    {
	if (ws->texture[i])
	{
	    glDeleteTextures (1, &ws->texture[i]);
	    ws->texture[i] = 0;
	}
    }

    if (ws->data)
	free (ws->data);

    size = (ws->width + 2) * (ws->height + 2);

    ws->data = calloc (1, (sizeof (float) * size * 2) +
		       (sizeof (GLubyte) * ws->width * ws->height * 4));
    if (!ws->data)
	return;

    ws->d0 = ws->data;
    ws->d1 = (ws->d0 + (size));
    ws->t0 = (unsigned char *) (ws->d1 + (size));

    for (i = 0; i < ws->height; i++)
    {
	for (j = 0; j < ws->width; j++)
	{
	    (ws->t0 + (ws->width * 4 * i + j * 4))[0] = 0xff;
	}
    }
}

static void
waterDrawWindowTexture (CompWindow		*w,
			CompTexture		*texture,
			const WindowPaintAttrib *attrib,
			unsigned int		mask)
{
    WATER_SCREEN (w->screen);

    if (ws->count)
    {
	Bool    lighting = w->screen->lighting;
	GLfloat plane[4];
	int     filter;

	screenLighting (w->screen, TRUE);

	glPushMatrix ();

	if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	{
	    glTranslatef (w->attrib.x, w->attrib.y, 0.0f);
	    glScalef (attrib->xScale, attrib->yScale, 0.0f);
	    glTranslatef (-w->attrib.x, -w->attrib.y, 0.0f);

	    filter = w->screen->filter[WINDOW_TRANS_FILTER];
	}
	else if (mask & PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK)
	{
	    filter = w->screen->filter[SCREEN_TRANS_FILTER];
	}
	else
	{
	    filter = w->screen->filter[NOTHING_TRANS_FILTER];
	}

	enableTexture (w->screen, texture, filter);

	(*w->screen->activeTexture) (GL_TEXTURE1_ARB);
	glBindTexture (ws->target, ws->texture[TINDEX (ws, 0)]);

	plane[1] = plane[2] = 0.0f;
	plane[0] = ws->tx / (GLfloat) w->screen->width;
	plane[3] = 0.0f;

	glTexGeni (GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv (GL_S, GL_EYE_PLANE, plane);
	glEnable (GL_TEXTURE_GEN_S);

	plane[0] = plane[2] = 0.0f;
	plane[1] = ws->ty / (GLfloat) w->screen->height;
	plane[3] = 0.0f;

	glTexGeni (GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	glTexGenfv (GL_T, GL_EYE_PLANE, plane);
	glEnable (GL_TEXTURE_GEN_T);

	glEnable (GL_FRAGMENT_PROGRAM_ARB);

	if (w->screen->canDoSaturated && attrib->saturation != COLOR)
	{
	    (*w->screen->bindProgram) (GL_FRAGMENT_PROGRAM_ARB,
				       ws->program[PROGRAM_BUMP_SAT]);

	    (*w->screen->programLocalParameter4f) (GL_FRAGMENT_PROGRAM_ARB, 0,
						   RED_SATURATION_WEIGHT,
						   GREEN_SATURATION_WEIGHT,
						   BLUE_SATURATION_WEIGHT,
						   attrib->saturation /
						   65535.0f);
	}
	else
	{
	    (*w->screen->bindProgram) (GL_FRAGMENT_PROGRAM_ARB,
				       ws->program[PROGRAM_BUMP]);
	}

	if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	{
	    glEnable (GL_BLEND);
	    if (attrib->opacity != OPAQUE || attrib->brightness != BRIGHT)
	    {
		GLushort color;

		color = (attrib->opacity * attrib->brightness) >> 16;

		glColor4us (color, color, color, attrib->opacity);

		(*w->screen->drawWindowGeometry) (w);

		glColor4usv (defaultColor);
	    }
	    else
	    {
		(*w->screen->drawWindowGeometry) (w);
	    }

	    glDisable (GL_BLEND);
	}
	else if (attrib->brightness != BRIGHT)
	{
	    screenTexEnvMode (w->screen, GL_MODULATE);
	    glColor4us (attrib->brightness, attrib->brightness,
			attrib->brightness, BRIGHT);

	    (*w->screen->drawWindowGeometry) (w);

	    glColor4usv (defaultColor);
	    screenTexEnvMode (w->screen, GL_REPLACE);
	}
	else
	{
	    (*w->screen->drawWindowGeometry) (w);
	}

	glDisable (GL_FRAGMENT_PROGRAM_ARB);

	glDisable (GL_TEXTURE_GEN_T);
	glDisable (GL_TEXTURE_GEN_S);

	glBindTexture (ws->target, 0);
	(*w->screen->activeTexture) (GL_TEXTURE0_ARB);
	disableTexture (texture);

	glPopMatrix ();

	screenLighting (w->screen, lighting);
    }
    else
    {
	UNWRAP (ws, w->screen, drawWindowTexture);
	(*w->screen->drawWindowTexture) (w, texture, attrib, mask);
	WRAP (ws, w->screen, drawWindowTexture, waterDrawWindowTexture);
    }
}

/* TODO: a way to control the speed */
static void
waterPreparePaintScreen (CompScreen *s,
			 int	    msSinceLastPaint)
{
    WATER_SCREEN (s);

    if (ws->count)
    {
	ws->count -= 10;
	if (ws->count < 0)
	    ws->count = 0;

	waterUpdate (s, 0.8f);
    }

    UNWRAP (ws, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (ws, s, preparePaintScreen, waterPreparePaintScreen);
}

static void
waterDonePaintScreen (CompScreen *s)
{
    WATER_SCREEN (s);

    if (ws->count)
	damageScreen (s);

    UNWRAP (ws, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ws, s, donePaintScreen, waterDonePaintScreen);
}

static void
waterHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    CompScreen *s;

    WATER_DISPLAY (d);

    switch (event->type) {
    case ButtonPress:
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s)
	{
	    WATER_SCREEN (s);

	    if (ws->grabIndex)
	    {
		XPoint p;

		p.x = event->xbutton.x_root;
		p.y = event->xbutton.y_root;

		waterVertices (s, GL_POINTS, &p, 1, 0.8f);
		damageScreen (s);
	    }
	}
	break;
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	{
	    WATER_SCREEN (s);

	    if (ws->grabIndex)
	    {
		XPoint p[2];

		p[0] = ws->p;

		p[1].x = event->xmotion.x_root;
		p[1].y = event->xmotion.y_root;

		waterVertices (s, GL_LINES, p, 2, 0.2f);
		damageScreen (s);

		ws->p.x = event->xmotion.x_root;
		ws->p.y = event->xmotion.y_root;
	    }
	}
	break;
    case KeyPress:
    case KeyRelease:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    WATER_SCREEN (s);

	    /* check if some other plugin has already grabbed the screen */
	    if (s->maxGrab - ws->grabIndex)
		break;

	    if (EV_KEY (&ws->opt[WATER_SCREEN_OPTION_INITIATE], event))
	    {
		XPoint p;

		if (!ws->grabIndex)
		    ws->grabIndex = pushScreenGrab (s, None);

		p.x = event->xkey.x_root;
		p.y = event->xkey.y_root;

		ws->p = p;

		waterVertices (s, GL_POINTS, &p, 1, 0.8f);
		damageScreen (s);
	    }

	    if (EV_KEY (&ws->opt[WATER_SCREEN_OPTION_TERMINATE], event) ||
		(event->type	     == KeyPress &&
		 event->xkey.keycode == s->escapeKeyCode))
	    {
		if (ws->grabIndex)
		{
		    removeScreenGrab (s, ws->grabIndex, 0);
		    ws->grabIndex = 0;
		}
	    }
	}
    default:
	break;
    }

    UNWRAP (wd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (wd, d, handleEvent, waterHandleEvent);
}

static Bool
waterInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    WaterDisplay *wd;

    wd = malloc (sizeof (WaterDisplay));
    if (!wd)
	return FALSE;

    wd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (wd->screenPrivateIndex < 0)
    {
	free (wd);
	return FALSE;
    }

    WRAP (wd, d, handleEvent, waterHandleEvent);

    d->privates[displayPrivateIndex].ptr = wd;

    return TRUE;
}

static void
waterFiniDisplay (CompPlugin *p,
		  CompDisplay *d)
{
    WATER_DISPLAY (d);

    freeScreenPrivateIndex (d, wd->screenPrivateIndex);

    UNWRAP (wd, d, handleEvent);

    free (wd);
}

static Bool
waterInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    WaterScreen *ws;

    WATER_DISPLAY (s->display);

    ws = calloc (1, sizeof (WaterScreen));
    if (!ws)
	return FALSE;

    ws->grabIndex = 0;

    waterScreenInitOptions (ws, s->display->display);

    addScreenBinding (s, &ws->opt[WATER_SCREEN_OPTION_INITIATE].value.bind);

    WRAP (ws, s, preparePaintScreen, waterPreparePaintScreen);
    WRAP (ws, s, donePaintScreen, waterDonePaintScreen);
    WRAP (ws, s, drawWindowTexture, waterDrawWindowTexture);

    s->privates[wd->screenPrivateIndex].ptr = ws;

    waterReset (s);

    return TRUE;
}

static void
waterFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    int i;

    WATER_SCREEN (s);

    if (ws->fbo)
	(*s->deleteFramebuffers) (1, &ws->fbo);

    for (i = 0; i < TEXTURE_NUM; i++)
    {
	if (ws->texture[i])
	    glDeleteTextures (1, &ws->texture[i]);
    }

    for (i = 0; i < PROGRAM_NUM; i++)
    {
	if (ws->program[i])
	    (*s->deletePrograms) (1, &ws->program[i]);
    }

    if (ws->data)
	free (ws->data);

    UNWRAP (ws, s, preparePaintScreen);
    UNWRAP (ws, s, donePaintScreen);
    UNWRAP (ws, s, drawWindowTexture);

    free (ws);
}

static Bool
waterInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
waterFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static CompPluginVTable waterVTable = {
    "water",
    "Water Effect",
    "Adds water effects to different desktop actions",
    waterInit,
    waterFini,
    waterInitDisplay,
    waterFiniDisplay,
    waterInitScreen,
    waterFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    waterGetScreenOptions,
    waterSetScreenOption,
    NULL,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &waterVTable;
}
