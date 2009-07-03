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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compiz-core.h>

static CompMatrix _identity_matrix = {
    1.0f, 0.0f,
    0.0f, 1.0f,
    0.0f, 0.0f
};

void
initTexture (CompScreen  *screen,
	     CompTexture *texture)
{
    texture->refCount	= 1;
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
	makeScreenCurrent (screen);
	releasePixmapFromTexture (screen, texture);
	glDeleteTextures (1, &texture->name);
    }
}

CompTexture *
createTexture (CompScreen *screen)
{
    CompTexture *texture;

    texture = (CompTexture *) malloc (sizeof (CompTexture));
    if (!texture)
	return NULL;

    initTexture (screen, texture);

    return texture;
}

void
destroyTexture (CompScreen  *screen,
		CompTexture *texture)
{
    texture->refCount--;
    if (texture->refCount)
	return;

    finiTexture (screen, texture);

    free (texture);
}

static Bool
imageToTexture (CompScreen   *screen,
		CompTexture  *texture,
		const char   *image,
		unsigned int width,
		unsigned int height,
		GLenum       format,
		GLenum       type)
{
    char *data;
    int	 i;
    GLint internalFormat;

    data = malloc (4 * width * height);
    if (!data)
	return FALSE;

    for (i = 0; i < height; i++)
	memcpy (&data[i * width * 4],
		&image[(height - i - 1) * width * 4],
		width * 4);

    makeScreenCurrent (screen);
    releasePixmapFromTexture (screen, texture);

    if (screen->textureNonPowerOfTwo ||
	(POWER_OF_TWO (width) && POWER_OF_TWO (height)))
    {
	texture->target = GL_TEXTURE_2D;
	texture->matrix.xx = 1.0f / width;
	texture->matrix.yy = -1.0f / height;
	texture->matrix.y0 = 1.0f;
	texture->mipmap = TRUE;
    }
    else
    {
	texture->target = GL_TEXTURE_RECTANGLE_NV;
	texture->matrix.xx = 1.0f;
	texture->matrix.yy = -1.0f;
	texture->matrix.y0 = height;
	texture->mipmap = FALSE;
    }

    if (!texture->name)
	glGenTextures (1, &texture->name);

    glBindTexture (texture->target, texture->name);

    internalFormat =
	(screen->opt[COMP_SCREEN_OPTION_TEXTURE_COMPRESSION].value.b &&
	 screen->textureCompression ?
	 GL_COMPRESSED_RGBA_ARB : GL_RGBA);

    glTexImage2D (texture->target, 0, internalFormat, width, height, 0,
		  format, type, data);

    texture->filter = GL_NEAREST;

    glTexParameteri (texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri (texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    texture->wrap = GL_CLAMP_TO_EDGE;

    glBindTexture (texture->target, 0);

    free (data);

    return TRUE;
}

Bool
imageBufferToTexture (CompScreen   *screen,
		      CompTexture  *texture,
		      const char   *image,
		      unsigned int width,
		      unsigned int height)
{
#if IMAGE_BYTE_ORDER == MSBFirst
    return imageToTexture (screen, texture, image, width, height,
			   GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
#else
    return imageToTexture (screen, texture, image, width, height,
			   GL_BGRA, GL_UNSIGNED_BYTE);
#endif
}

Bool
imageDataToTexture (CompScreen   *screen,
		    CompTexture  *texture,
		    const char	   *image,
		    unsigned int width,
		    unsigned int height,
		    GLenum       format,
		    GLenum       type)
{
    return imageToTexture (screen, texture, image, width, height, format, type);
}


Bool
readImageToTexture (CompScreen   *screen,
		    CompTexture  *texture,
		    const char	 *imageFileName,
		    unsigned int *returnWidth,
		    unsigned int *returnHeight)
{
    void *image;
    int  width, height;
    Bool status;

    if (!readImageFromFile (screen->display, imageFileName,
			    &width, &height, &image))
	return FALSE;

    status = imageBufferToTexture (screen, texture, image, width, height);

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
    return imageBufferToTexture (screen, &icon->texture,
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
    unsigned int target = 0;
    CompFBConfig *config = &screen->glxPixmapFBConfigs[depth];
    int          attribs[7], i = 0;

    if (!config->fbConfig)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"No GLXFBConfig for depth %d",
			depth);

	return FALSE;
    }

    attribs[i++] = GLX_TEXTURE_FORMAT_EXT;
    attribs[i++] = config->textureFormat;
    attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
    attribs[i++] = config->mipmap;

    /* If no texture target is specified in the fbconfig, or only the
       TEXTURE_2D target is specified and GL_texture_non_power_of_two
       is not supported, then allow the server to choose the texture target. */
    if (config->textureTargets & GLX_TEXTURE_2D_BIT_EXT &&
       (screen->textureNonPowerOfTwo ||
       (POWER_OF_TWO (width) && POWER_OF_TWO (height))))
	target = GLX_TEXTURE_2D_EXT;
    else if (config->textureTargets & GLX_TEXTURE_RECTANGLE_BIT_EXT)
	target = GLX_TEXTURE_RECTANGLE_EXT;

    /* Workaround for broken texture from pixmap implementations,
       that don't advertise any texture target in the fbconfig. */
    if (!target)
    {
	if (!(config->textureTargets & GLX_TEXTURE_2D_BIT_EXT))
	    target = GLX_TEXTURE_RECTANGLE_EXT;
	else if (!(config->textureTargets & GLX_TEXTURE_RECTANGLE_BIT_EXT))
	    target = GLX_TEXTURE_2D_EXT;
    }

    if (target)
    {
	attribs[i++] = GLX_TEXTURE_TARGET_EXT;
	attribs[i++] = target;
    }

    attribs[i++] = None;

    makeScreenCurrent (screen);
    texture->pixmap = (*screen->createPixmap) (screen->display->display,
					       config->fbConfig, pixmap,
					       attribs);
    if (!texture->pixmap)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"glXCreatePixmap failed");

	return FALSE;
    }

    if (!target)
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
	texture->mipmap = config->mipmap;
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
	texture->mipmap = FALSE;
	break;
    default:
	compLogMessage ("core", CompLogLevelWarn,
			"pixmap 0x%x can't be bound to texture",
			(int) pixmap);

	(*screen->destroyPixmap) (screen->display->display, texture->pixmap);
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
	makeScreenCurrent (screen);
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

	(*screen->destroyPixmap) (screen->display->display, texture->pixmap);

	texture->pixmap = None;
    }
}

void
enableTexture (CompScreen	 *screen,
	       CompTexture	 *texture,
	       CompTextureFilter filter)
{
    makeScreenCurrent (screen);
    glEnable (texture->target);
    glBindTexture (texture->target, texture->name);

    if (strictBinding && texture->pixmap)
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
    makeScreenCurrent (screen);
    if (strictBinding && texture->pixmap)
    {
	glBindTexture (texture->target, texture->name);

	(*screen->releaseTexImage) (screen->display->display,
				    texture->pixmap,
				    GLX_FRONT_LEFT_EXT);
    }

    glBindTexture (texture->target, 0);
    glDisable (texture->target);
}
