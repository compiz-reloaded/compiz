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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <setjmp.h>

#include <compiz-core.h>

static CompMetadata pngMetadata;

#define PNG_SIG_SIZE 8

static int displayPrivateIndex;

typedef struct _PngDisplay {
    FileToImageProc fileToImage;
    ImageToFileProc imageToFile;
} PngDisplay;

#define GET_PNG_DISPLAY(d)					 \
    ((PngDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define PNG_DISPLAY(d)			 \
    PngDisplay *pd = GET_PNG_DISPLAY (d)


static void
premultiplyData (png_structp   png,
		 png_row_infop row_info,
		 png_bytep     data)
{
    unsigned int i;

    for (i = 0; i < row_info->rowbytes; i += 4)
    {
	unsigned char *base = &data[i];
	unsigned char blue  = base[0];
	unsigned char green = base[1];
	unsigned char red   = base[2];
	unsigned char alpha = base[3];
	int	      p;

	red   = (unsigned) red   * (unsigned) alpha / 255;
	green = (unsigned) green * (unsigned) alpha / 255;
	blue  = (unsigned) blue  * (unsigned) alpha / 255;

	p = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
	memcpy (base, &p, sizeof (int));
    }
}

static Bool
readPngData (png_struct	*png,
	     png_info	*info,
	     void	**data,
	     int	*width,
	     int	*height)
{
    png_uint_32	 png_width, png_height;
    int		 depth, color_type, interlace, i;
    unsigned int pixel_size;
    png_byte	 **row_pointers;
    char	 *d;

    png_read_info (png, info);

    png_get_IHDR (png, info,
		  &png_width, &png_height, &depth,
		  &color_type, &interlace, NULL, NULL);

    *width  = (int) png_width;
    *height = (int) png_height;

    /* convert palette/gray image to rgb */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
	png_set_palette_to_rgb (png);

    /* expand gray bit depth if needed */
    if (color_type == PNG_COLOR_TYPE_GRAY && depth < 8)
	png_set_expand_gray_1_2_4_to_8 (png);

    /* transform transparency to alpha */
    if (png_get_valid(png, info, PNG_INFO_tRNS))
	png_set_tRNS_to_alpha (png);

    if (depth == 16)
	png_set_strip_16 (png);

    if (depth < 8)
	png_set_packing (png);

    /* convert grayscale to RGB */
    if (color_type == PNG_COLOR_TYPE_GRAY ||
	color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
	png_set_gray_to_rgb (png);

    if (interlace != PNG_INTERLACE_NONE)
	png_set_interlace_handling (png);

    png_set_bgr (png);
    png_set_filler (png, 0xff, PNG_FILLER_AFTER);

    png_set_read_user_transform_fn (png, premultiplyData);

    png_read_update_info (png, info);

    pixel_size = 4;
    d = (char *) malloc (png_width * png_height * pixel_size);
    if (!d)
	return FALSE;

    *data = d;

    row_pointers = (png_byte **) malloc (png_height * sizeof (char *));
    if (!row_pointers)
    {
	free (d);
	return FALSE;
    }

    for (i = 0; i < png_height; i++)
	row_pointers[i] = (png_byte *) (d + i * png_width * pixel_size);

    png_read_image (png, row_pointers);
    png_read_end (png, info);

    free (row_pointers);

    return TRUE;
}

static Bool
readPngFileToImage (FILE *file,
		    int  *width,
		    int  *height,
		    void **data)
{
    unsigned char png_sig[PNG_SIG_SIZE];
    int		  sig_bytes;
    png_struct	  *png;
    png_info	  *info;
    Bool	  status;

    sig_bytes = fread (png_sig, 1, PNG_SIG_SIZE, file);
    if (png_sig_cmp (png_sig, 0, sig_bytes) != 0)
	return FALSE;

    png = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
	return FALSE;

    info = png_create_info_struct (png);
    if (!info)
    {
	png_destroy_read_struct (&png, NULL, NULL);

	return FALSE;
    }

    png_init_io (png, file);
    png_set_sig_bytes (png, sig_bytes);

    status = readPngData (png, info, data, width, height);

    png_destroy_read_struct (&png, &info, NULL);

    return status;
}

#if 0
static void
userReadData (png_structp png_ptr,
	      png_bytep   data,
	      png_size_t  length)
{
    const unsigned char **buffer = (const unsigned char **)
	png_get_io_ptr (png_ptr);

    memcpy (data, *buffer, length);
    *buffer += length;
}

static Bool
readPngBuffer (const unsigned char *buffer,
	       char		   **data,
	       unsigned int	   *width,
	       unsigned int	   *height)
{
    unsigned char	png_sig[PNG_SIG_SIZE];
    png_struct		*png;
    png_info		*info;
    const unsigned char *b = buffer + PNG_SIG_SIZE;
    Bool		status;

    memcpy (png_sig, buffer, PNG_SIG_SIZE);
    if (png_sig_cmp (png_sig, 0, PNG_SIG_SIZE) != 0)
	return FALSE;

    png = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
	return FALSE;

    info = png_create_info_struct (png);
    if (!info)
    {
	png_destroy_read_struct (&png, NULL, NULL);
	return FALSE;
    }

    png_set_read_fn (png, (void *) &b, userReadData);
    png_set_sig_bytes (png, PNG_SIG_SIZE);

    status = readPngData (png, info, data, width, height);

    png_destroy_read_struct (&png, &info, NULL);

    return status;
}
#endif

static Bool
writePng (unsigned char *buffer,
	  png_rw_ptr	writeFunc,
	  void		*closure,
	  int		width,
	  int		height,
	  int		stride)
{
    png_struct	 *png;
    png_info	 *info;
    png_byte	 **rows;
    png_color_16 white;
    int		 i;

    rows = malloc (height * sizeof (png_byte *));
    if (!rows)
	return FALSE;

    for (i = 0; i < height; i++)
	rows[height - i - 1] = buffer + i * stride;

    png = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
    {
	free (rows);

	return FALSE;
    }

    info = png_create_info_struct (png);
    if (!info)
    {
	png_destroy_read_struct (&png, NULL, NULL);
	free (rows);

	return FALSE;
    }

    if (setjmp (png_jmpbuf (png)))
    {
	png_destroy_read_struct (&png, NULL, NULL);
	free (rows);

	return FALSE;
    }

    png_set_write_fn (png, closure, writeFunc, NULL);

    png_set_IHDR (png, info,
		  width, height, 8,
		  PNG_COLOR_TYPE_RGB_ALPHA,
		  PNG_INTERLACE_NONE,
		  PNG_COMPRESSION_TYPE_DEFAULT,
		  PNG_FILTER_TYPE_DEFAULT);

    white.red   = 0xff;
    white.blue  = 0xff;
    white.green = 0xff;

    png_set_bKGD (png, info, &white);

    png_write_info (png, info);
    png_write_image (png, rows);
    png_write_end (png, info);

    png_destroy_write_struct (&png, &info);
    free (rows);

    return TRUE;
}

static void
stdioWriteFunc (png_structp png,
		png_bytep   data,
		png_size_t  size)
{
    FILE *fp;

    fp = png_get_io_ptr (png);
    if (fwrite (data, 1, size, fp) != size)
	png_error (png, "Write Error");
}

static char *
pngExtension (const char *name)
{

    if (strlen (name) > 4)
    {
	if (strcasecmp (name + (strlen (name) - 4), ".png") == 0)
	    return "";
    }

    return ".png";
}

static Bool
pngImageToFile (CompDisplay *d,
		const char  *path,
		const char  *name,
		const char  *format,
		int	    width,
		int	    height,
		int	    stride,
		void	    *data)
{
    Bool status = FALSE;
    char *extension = pngExtension (name);
    char *file;
    FILE *fp;
    int  len;

    PNG_DISPLAY (d);

    len = (path ? strlen (path) : 0) + strlen (name) + strlen (extension) + 2;

    file = malloc (len);
    if (file)
    {
	if (path)
	    sprintf (file, "%s/%s%s", path, name, extension);
	else
	    sprintf (file, "%s%s", name, extension);
    }

    if (file && strcasecmp (format, "png") == 0)
    {
	fp = fopen (file, "wb");
	if (fp)
	{
	    status = writePng (data, stdioWriteFunc, fp, width, height, stride);
	    fclose (fp);
	}

	if (status)
	{
	    free (file);
	    return TRUE;
	}
    }

    UNWRAP (pd, d, imageToFile);
    status = (*d->imageToFile) (d, path, name, format, width, height, stride,
				data);
    WRAP (pd, d, imageToFile, pngImageToFile);

    if (!status && file)
    {
	fp = fopen (file, "wb");
	if (fp)
	{
	    status = writePng (data, stdioWriteFunc, fp, width, height, stride);
	    fclose (fp);
	}
    }

    if (file)
	free (file);

    return status;
}

static Bool
pngFileToImage (CompDisplay *d,
		const char  *path,
		const char  *name,
		int	    *width,
		int	    *height,
		int	    *stride,
		void	    **data)
{
    Bool status = FALSE;
    char *extension = pngExtension (name);
    char *file;
    int  len;

    PNG_DISPLAY (d);

    len = (path ? strlen (path) : 0) + strlen (name) + strlen (extension) + 2;

    file = malloc (len);
    if (file)
    {
	FILE *fp;

	if (path)
	    sprintf (file, "%s/%s%s", path, name, extension);
	else
	    sprintf (file, "%s%s", name, extension);

	fp = fopen (file, "r");
	if (fp)
	{
	    status = readPngFileToImage (fp,
					 width,
					 height,
					 data);
	    fclose (fp);
	}

	free (file);

	if (status)
	{
	    *stride = *width * 4;
	    return TRUE;
	}
    }

    UNWRAP (pd, d, fileToImage);
    status = (*d->fileToImage) (d, path, name, width, height, stride, data);
    WRAP (pd, d, fileToImage, pngFileToImage);

    return status;
}

static Bool
pngInitDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    PngDisplay *pd;
    CompScreen *s;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    pd = malloc (sizeof (PngDisplay));
    if (!pd)
	return FALSE;

    WRAP (pd, d, fileToImage, pngFileToImage);
    WRAP (pd, d, imageToFile, pngImageToFile);

    d->base.privates[displayPrivateIndex].ptr = pd;

    for (s = d->screens; s; s = s->next)
	updateDefaultIcon (s);

    return TRUE;
}

static void
pngFiniDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    CompScreen *s;

    PNG_DISPLAY (d);

    UNWRAP (pd, d, fileToImage);
    UNWRAP (pd, d, imageToFile);

    for (s = d->screens; s; s = s->next)
	updateDefaultIcon (s);

    free (pd);
}

static CompBool
pngInitObject (CompPlugin *p,
	       CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) pngInitDisplay
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
pngFiniObject (CompPlugin *p,
	       CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) pngFiniDisplay
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
pngInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&pngMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&pngMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&pngMetadata, p->vTable->name);

    return TRUE;
}

static void
pngFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&pngMetadata);
}

static CompMetadata *
pngGetMetadata (CompPlugin *plugin)
{
    return &pngMetadata;
}

CompPluginVTable pngVTable = {
    "png",
    pngGetMetadata,
    pngInit,
    pngFini,
    pngInitObject,
    pngFiniObject,
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &pngVTable;
}
