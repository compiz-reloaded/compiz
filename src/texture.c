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

#include <compiz.h>

static CompMatrix _identity_matrix = {
    1.0f, 0.0f,
    0.0f, 1.0f,
    0.0f, 0.0f
};

void
initTexture (CompScreen  *screen,
	     CompTexture *texture)
{
    texture->name	= 0;
    texture->target	= GL_TEXTURE_2D;
    texture->pixmap	= None;
    texture->filter	= GL_NEAREST;
    texture->wrap	= GL_CLAMP_TO_EDGE;
    texture->matrix     = _identity_matrix;
    texture->oldMipmaps = TRUE;
    texture->mipmap	= FALSE;
}

void
finiTexture (CompScreen  *screen,
	     CompTexture *texture)
{
    if (texture->name)
    {
	releasePixmapFromTexture (screen, texture);
	glDeleteTextures (1, &texture->name);
    }
}

static Bool
imageToTexture (CompScreen   *screen,
		CompTexture  *texture,
		char	     *image,
		unsigned int width,
		unsigned int height)
{
    char *data;
    int	 i;

    data = malloc (4 * width * height);
    if (!data)
	return FALSE;

    for (i = 0; i < height; i++)
	memcpy (&data[i * width * 4],
		&image[(height - i - 1) * width * 4],
		width * 4);

    releasePixmapFromTexture (screen, texture);

    if (screen->textureNonPowerOfTwo ||
	(POWER_OF_TWO (width) && POWER_OF_TWO (height)))
    {
	texture->target = GL_TEXTURE_2D;
	texture->matrix.xx = 1.0f / width;
	texture->matrix.yy = -1.0f / height;
	texture->matrix.y0 = 1.0f;
    }
    else
    {
	texture->target = GL_TEXTURE_RECTANGLE_NV;
	texture->matrix.xx = 1.0f;
	texture->matrix.yy = -1.0f;
	texture->matrix.y0 = height;
    }

    if (!texture->name)
	glGenTextures (1, &texture->name);

    glBindTexture (texture->target, texture->name);

    glTexImage2D (texture->target, 0, GL_RGBA, width, height, 0, GL_BGRA,

#if IMAGE_BYTE_ORDER == MSBFirst
		  GL_UNSIGNED_INT_8_8_8_8_REV,
#else
		  GL_UNSIGNED_BYTE,
#endif

		  data);

    texture->filter = GL_NEAREST;

    glTexParameteri (texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri (texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    texture->wrap = GL_CLAMP_TO_EDGE;
    texture->mipmap = TRUE;

    glBindTexture (texture->target, 0);

    free (data);

    return TRUE;
}

Bool
readImageToTexture (CompScreen   *screen,
		    CompTexture  *texture,
		    const char	 *imageFileName,
		    unsigned int *returnWidth,
		    unsigned int *returnHeight)
{
    char	 *image;
    unsigned int width, height;
    Bool	 status;

    if (!readPng (imageFileName, &image, &width, &height))
	return FALSE;

    status = imageToTexture (screen, texture, image, width, height);

    free (image);

    if (returnWidth)
	*returnWidth = width;
    if (returnHeight)
	*returnHeight = height;

    return status;
}

Bool
readImageBufferToTexture (CompScreen	      *screen,
			  CompTexture	      *texture,
			  const unsigned char *imageBuffer,
			  unsigned int	      *returnWidth,
			  unsigned int	      *returnHeight)
{
    char	 *image;
    unsigned int width, height;
    Bool	 status;

    if (!readPngBuffer (imageBuffer, &image, &width, &height))
	return FALSE;

    status = imageToTexture (screen, texture, image, width, height);

    free (image);

    if (returnWidth)
	*returnWidth = width;
    if (returnHeight)
	*returnHeight = height;

    return status;
}

Bool
iconToTexture (CompScreen *screen,
	       CompIcon   *icon)
{
    return imageToTexture (screen,
			   &icon->texture,
			   (char *) (icon + 1),
			   icon->width,
			   icon->height);
}

Bool
bindPixmapToTexture (CompScreen  *screen,
		     CompTexture *texture,
		     Pixmap	 pixmap,
		     int	 width,
		     int	 height,
		     int	 depth)
{
    unsigned int target;
    CompFBConfig *config = &screen->glxPixmapFBConfigs[depth];
    int          attribs[] = {
	GLX_TEXTURE_FORMAT_EXT, config->textureFormat,
	GLX_MIPMAP_TEXTURE_EXT, config->mipmap,
	None
    };

    if (!config->fbConfig)
    {
	fprintf (stderr, "%s: No GLXFBConfig for depth %d\n",
		 programName, depth);

	return FALSE;
    }

    texture->pixmap = (*screen->createPixmap) (screen->display->display,
					       config->fbConfig, pixmap,
					       attribs);
    if (!texture->pixmap)
    {
	fprintf (stderr, "%s: glXCreatePixmap failed\n", programName);

	return FALSE;
    }

    texture->mipmap = config->mipmap;

    (*screen->queryDrawable) (screen->display->display,
			      texture->pixmap,
			      GLX_TEXTURE_TARGET_EXT,
			      &target);
    switch (target) {
    case GLX_TEXTURE_2D_EXT:
	texture->target = GL_TEXTURE_2D;

	texture->matrix.xx = 1.0f / width;
	if (config->yInverted)
	{
	    texture->matrix.yy = 1.0f / height;
	    texture->matrix.y0 = 0.0f;
	}
	else
	{
	    texture->matrix.yy = -1.0f / height;
	    texture->matrix.y0 = 1.0f;
	}
	break;
    case GLX_TEXTURE_RECTANGLE_EXT:
	texture->target = GL_TEXTURE_RECTANGLE_ARB;

	texture->matrix.xx = 1.0f;
	if (config->yInverted)
	{
	    texture->matrix.yy = 1.0f;
	    texture->matrix.y0 = 0;
	}
	else
	{
	    texture->matrix.yy = -1.0f;
	    texture->matrix.y0 = height;
	}
	break;
    default:
	fprintf (stderr, "%s: pixmap 0x%x can't be bound to texture\n",
		 programName, (int) pixmap);

	glXDestroyGLXPixmap (screen->display->display, texture->pixmap);
	texture->pixmap = None;

	return FALSE;
    }

    if (!texture->name)
	glGenTextures (1, &texture->name);

    glBindTexture (texture->target, texture->name);

    if (!strictBinding)
    {
	(*screen->bindTexImage) (screen->display->display,
				 texture->pixmap,
				 GLX_FRONT_LEFT_EXT,
				 NULL);
    }

    texture->filter = GL_NEAREST;

    glTexParameteri (texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri (texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    texture->wrap = GL_CLAMP_TO_EDGE;

    glBindTexture (texture->target, 0);

    return TRUE;
}

void
releasePixmapFromTexture (CompScreen  *screen,
			  CompTexture *texture)
{
    if (texture->pixmap)
    {
	glEnable (texture->target);
	if (!strictBinding)
	{
	    glBindTexture (texture->target, texture->name);

	    (*screen->releaseTexImage) (screen->display->display,
					texture->pixmap,
					GLX_FRONT_LEFT_EXT);
	}

	glBindTexture (texture->target, 0);
	glDisable (texture->target);

	glXDestroyGLXPixmap (screen->display->display, texture->pixmap);

	texture->pixmap = None;
    }
}

void
enableTexture (CompScreen	 *screen,
	       CompTexture	 *texture,
	       CompTextureFilter filter)
{
    glEnable (texture->target);
    glBindTexture (texture->target, texture->name);

    if (strictBinding)
    {
	(*screen->bindTexImage) (screen->display->display,
				 texture->pixmap,
				 GLX_FRONT_LEFT_EXT,
				 NULL);
    }

    if (filter == COMP_TEXTURE_FILTER_FAST)
    {
	if (texture->filter != GL_NEAREST)
	{
	    glTexParameteri (texture->target,
			     GL_TEXTURE_MIN_FILTER,
			     GL_NEAREST);
	    glTexParameteri (texture->target,
			     GL_TEXTURE_MAG_FILTER,
			     GL_NEAREST);

	    texture->filter = GL_NEAREST;
	}
    }
    else if (texture->filter != screen->display->textureFilter)
    {
	if (screen->display->textureFilter == GL_LINEAR_MIPMAP_LINEAR)
	{
	    if (screen->textureNonPowerOfTwo && screen->fbo && texture->mipmap)
	    {
		glTexParameteri (texture->target,
				 GL_TEXTURE_MIN_FILTER,
				 GL_LINEAR_MIPMAP_LINEAR);

		if (texture->filter != GL_LINEAR)
		    glTexParameteri (texture->target,
				     GL_TEXTURE_MAG_FILTER,
				     GL_LINEAR);

		texture->filter = GL_LINEAR_MIPMAP_LINEAR;
	    }
	    else if (texture->filter != GL_LINEAR)
	    {
		glTexParameteri (texture->target,
				 GL_TEXTURE_MIN_FILTER,
				 GL_LINEAR);
		glTexParameteri (texture->target,
				 GL_TEXTURE_MAG_FILTER,
				 GL_LINEAR);

		texture->filter = GL_LINEAR;
	    }
	}
	else
	{
	    glTexParameteri (texture->target,
			     GL_TEXTURE_MIN_FILTER,
			     screen->display->textureFilter);
	    glTexParameteri (texture->target,
			     GL_TEXTURE_MAG_FILTER,
			     screen->display->textureFilter);

	    texture->filter = screen->display->textureFilter;
	}
    }

    if (texture->filter == GL_LINEAR_MIPMAP_LINEAR)
    {
	if (texture->oldMipmaps)
	{
	    (*screen->generateMipmap) (texture->target);
	    texture->oldMipmaps = FALSE;
	}
    }
}

void
disableTexture (CompScreen  *screen,
		CompTexture *texture)
{
    if (strictBinding)
    {
	glBindTexture (texture->target, texture->name);

	(*screen->releaseTexImage) (screen->display->display,
				    texture->pixmap,
				    GLX_FRONT_LEFT_EXT);
    }

    glBindTexture (texture->target, 0);
    glDisable (texture->target);
}
