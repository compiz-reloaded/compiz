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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <setjmp.h>

#include <compiz.h>

#define HOME_IMAGEDIR ".compiz/images"

#define PNG_SIG_SIZE 8

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
readPngData (png_struct	  *png,
	     png_info	  *info,
	     char	  **data,
	     unsigned int *width,
	     unsigned int *height)
{
    png_uint_32	 png_width, png_height;
    int		 depth, color_type, interlace, i;
    unsigned int pixel_size;
    png_byte	 **row_pointers;

    png_read_info (png, info);

    png_get_IHDR (png, info,
		  &png_width, &png_height, &depth,
		  &color_type, &interlace, NULL, NULL);

    *width  = png_width;
    *height = png_height;

    /* convert palette/gray image to rgb */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
	png_set_palette_to_rgb (png);

    /* expand gray bit depth if needed */
    if (color_type == PNG_COLOR_TYPE_GRAY && depth < 8)
	png_set_gray_1_2_4_to_8 (png);

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
    *data = (char *) malloc (png_width * png_height * pixel_size);
    if (*data == NULL)
	return FALSE;

    row_pointers = (png_byte **) malloc (png_height * sizeof (char *));
    if (!row_pointers)
    {
	free (*data);
	return FALSE;
    }

    for (i = 0; i < png_height; i++)
	row_pointers[i] = (png_byte *) (*data + i * png_width * pixel_size);

    png_read_image (png, row_pointers);
    png_read_end (png, info);

    free (row_pointers);

    return TRUE;
}

Bool
openImageFile (const char *filename,
	       char	  **returnFilename,
	       FILE	  **returnFile)
{
    FILE *file;
    char *image = NULL;

    file = fopen (filename, "r");
    if (!file)
    {
	char *home;

	home = getenv ("HOME");
	if (home)
	{
	    image = malloc (strlen (home) +
			    strlen (HOME_IMAGEDIR) +
			    strlen (filename) + 3);
	    if (image)
	    {
		sprintf (image, "%s/%s/%s", home, HOME_IMAGEDIR, filename);
		file = fopen (image, "r");
		if (!file)
		{
		    free (image);
		    image = NULL;
		}
	    }
	}

	if (!file)
	{
	    image = malloc (strlen (IMAGEDIR) + strlen (filename) + 2);
	    if (image)
	    {
		sprintf (image, "%s/%s", IMAGEDIR, filename);
		file = fopen (image, "r");
	    }

	    if (!file)
	    {
		if (image)
		    free (image);

		return FALSE;
	    }
	}
    }

    if (returnFilename)
    {
	if (image)
	    *returnFilename = image;
	else
	    *returnFilename = strdup (filename);
    }
    else
    {
	if (image)
	    free (image);
    }

    if (returnFile)
    {
	*returnFile = file;
    }
    else
    {
	fclose (file);
    }

    return TRUE;
}

Bool
readPng (const char   *filename,
	 char	      **data,
	 unsigned int *width,
	 unsigned int *height)
{
    unsigned char png_sig[PNG_SIG_SIZE];
    FILE	  *file;
    int		  sig_bytes;
    png_struct	  *png;
    png_info	  *info;
    Bool	  status;

    if (!openImageFile (filename, NULL, &file))
	return FALSE;

    sig_bytes = fread (png_sig, 1, PNG_SIG_SIZE, file);
    if (png_check_sig (png_sig, sig_bytes) == 0)
    {
	fclose (file);
	return FALSE;
    }

    png = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
	return FALSE;

    info = png_create_info_struct (png);
    if (info == NULL)
    {
	png_destroy_read_struct (&png, NULL, NULL);
	fclose (file);

	return FALSE;
    }

    png_init_io (png, file);
    png_set_sig_bytes (png, sig_bytes);

    status = readPngData (png, info, data, width, height);

    png_destroy_read_struct (&png, &info, NULL);
    fclose (file);

    return status;
}

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

Bool
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
    if (png_check_sig (png_sig, PNG_SIG_SIZE) == 0)
	return FALSE;

    png = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
	return FALSE;

    info = png_create_info_struct (png);
    if (info == NULL)
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
