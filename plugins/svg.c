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
#include <string.h>

#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#include <compiz.h>

static CompMetadata svgMetadata;

static int displayPrivateIndex;

typedef struct _SvgDisplay {
    FileToImageProc fileToImage;
} SvgDisplay;

#define GET_SVG_DISPLAY(d)				    \
    ((SvgDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define SVG_DISPLAY(d)			 \
    SvgDisplay *sd = GET_SVG_DISPLAY (d)


static Bool
readSvgFileToImage (char *file,
		    int  *width,
		    int  *height,
		    void **data)
{
    cairo_surface_t   *surface;
    FILE	      *fp;
    GError	      *error = NULL;
    RsvgHandle	      *svgHandle;
    RsvgDimensionData svgDimension;

    fp = fopen (file, "r");
    if (!fp)
	return FALSE;

    fclose (fp);

    svgHandle = rsvg_handle_new_from_file (file, &error);
    if (!svgHandle)
	return FALSE;

    rsvg_handle_get_dimensions (svgHandle, &svgDimension);

    *width  = svgDimension.width;
    *height = svgDimension.height;

    *data = malloc (svgDimension.width * svgDimension.height * 4);
    if (!*data)
    {
	rsvg_handle_free (svgHandle);
	return FALSE;
    }

    surface = cairo_image_surface_create_for_data (*data,
						   CAIRO_FORMAT_ARGB32,
						   svgDimension.width,
						   svgDimension.height,
						   svgDimension.width * 4);
    if (surface)
    {
	cairo_t *cr;

	cr = cairo_create (surface);

	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	rsvg_handle_render_cairo (svgHandle, cr);

	cairo_destroy (cr);
	cairo_surface_destroy (surface);
    }

    rsvg_handle_free (svgHandle);

    return TRUE;
}

static char *
svgExtension (const char *name)
{

    if (strlen (name) > 4)
    {
	if (strcasecmp (name + (strlen (name) - 4), ".svg") == 0)
	    return "";
    }

    return ".svg";
}

static Bool
svgFileToImage (CompDisplay *d,
		const char  *path,
		const char  *name,
		int	    *width,
		int	    *height,
		int	    *stride,
		void	    **data)
{
    Bool status = FALSE;
    char *extension = svgExtension (name);
    char *file;
    int  len;

    SVG_DISPLAY (d);

    len = (path ? strlen (path) : 0) + strlen (name) + strlen (extension) + 2;

    file = malloc (len);
    if (file)
    {
	if (path)
	    sprintf (file, "%s/%s%s", path, name, extension);
	else
	    sprintf (file, "%s%s", name, extension);

	status = readSvgFileToImage (file, width, height, data);

	free (file);

	if (status)
	{
	    *stride = *width * 4;
	    return TRUE;
	}
    }

    UNWRAP (sd, d, fileToImage);
    status = (*d->fileToImage) (d, path, name, width, height, stride, data);
    WRAP (sd, d, fileToImage, svgFileToImage);

    return status;
}

static Bool
svgInitDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    SvgDisplay *sd;
    CompScreen *s;

    sd = malloc (sizeof (SvgDisplay));
    if (!sd)
	return FALSE;

    WRAP (sd, d, fileToImage, svgFileToImage);

    d->privates[displayPrivateIndex].ptr = sd;

    for (s = d->screens; s; s = s->next) 
	updateDefaultIcon (s);

    return TRUE;
}

static void
svgFiniDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    CompScreen *s;

    SVG_DISPLAY (d);

    UNWRAP (sd, d, fileToImage);

    for (s = d->screens; s; s = s->next) 
	updateDefaultIcon (s);

    free (sd);
}

static Bool
svgInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&svgMetadata,
					 p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&svgMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&svgMetadata, p->vTable->name);

    return TRUE;
}

static void
svgFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);

    compFiniMetadata (&svgMetadata);
}

static int
svgGetVersion (CompPlugin *plugin,
	       int	  version)
{
    return ABIVERSION;
}

static CompMetadata *
svgGetMetadata (CompPlugin *plugin)
{
    return &svgMetadata;
}

CompPluginVTable svgVTable = {
    "svg",
    "Svg",
    "Svg image loader",
    svgGetVersion,
    svgGetMetadata,
    svgInit,
    svgFini,
    svgInitDisplay,
    svgFiniDisplay,
    0, /* InitScreen */
    0, /* FiniScreen */
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &svgVTable;
}
