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

#include <compiz-core.h>

#include <string.h>
#include <stdlib.h>

#define COMP_FUNCTION_TYPE_ARB 0
#define COMP_FUNCTION_TYPE_NUM 1

#define COMP_FUNCTION_ARB_MASK (1 << 0)
#define COMP_FUNCTION_MASK     (COMP_FUNCTION_ARB_MASK)

struct _CompProgram {
    struct _CompProgram *next;

    int *signature;
    int nSignature;

    Bool blending;

    GLuint name;
    GLenum type;
};

typedef enum {
    CompOpTypeData,
    CompOpTypeDataStore,
    CompOpTypeDataOffset,
    CompOpTypeDataBlend,
    CompOpTypeHeaderTemp,
    CompOpTypeHeaderParam,
    CompOpTypeHeaderAttrib,
    CompOpTypeColor,
    CompOpTypeFetch,
    CompOpTypeLoad
} CompOpType;

typedef struct _CompDataOp {
    CompOpType type;

    char *data;
} CompDataOp;

typedef struct _CompHeaderOp {
    CompOpType type;

    char *name;
} CompHeaderOp;

typedef struct _CompFetchOp {
    CompOpType type;

    char *dst;
    char *offset;
    int  target;
} CompFetchOp;

typedef struct _CompLoadOp {
    CompOpType type;

    char *noOffset[COMP_FETCH_TARGET_NUM];
    char *offset[COMP_FETCH_TARGET_NUM];
} CompLoadOp;

typedef struct _CompColorOp {
    CompOpType type;

    char *dst;
    char *src;
} CompColorOp;

typedef union _CompBodyOp {
    CompOpType	type;

    CompDataOp  data;
    CompFetchOp fetch;
    CompLoadOp  load;
    CompColorOp color;
} CompBodyOp;

struct _CompFunctionData {
    CompHeaderOp *header;
    int		 nHeader;

    CompBodyOp *body;
    int	       nBody;
};

struct _CompFunction {
    struct _CompFunction *next;

    int		     id;
    char	     *name;
    CompFunctionData data[COMP_FUNCTION_TYPE_NUM];
    int		     mask;
};

typedef struct _FetchInfo {
    int  indices[MAX_FRAGMENT_FUNCTIONS];
    char *data;
} FetchInfo;

typedef void (*DataOpCallBackProc) (CompDataOp *op,
				    int	       index,
				    void       *closure);

static CompLoadOp loadArbFpData = {
    CompOpTypeLoad,
    {
	"TEX output, fragment.texcoord[0], texture[0], 2D;",
	"TEX output, fragment.texcoord[0], texture[0], RECT;"
    }, {
	"TEX output, __tmp_texcoord0, texture[0], 2D;",
	"TEX output, __tmp_texcoord0, texture[0], RECT;"
    }
};

static CompFunction initialLoadFunction = {
    NULL,
    0,
    "__core_load",
    {
	{
	    NULL,
	    0,
	    (CompBodyOp *) &loadArbFpData,
	    1
	}
    },
    COMP_FUNCTION_MASK
};

static CompFunction *
findFragmentFunction (CompScreen *s,
		      int	 id)
{
    CompFunction *function;

    for (function = s->fragmentFunctions; function; function = function->next)
    {
	if (function->id == id)
	    return function;
    }

    return NULL;
}

static CompFunction *
findFragmentFunctionWithName (CompScreen *s,
			      const char *name)
{
    CompFunction *function;

    for (function = s->fragmentFunctions; function; function = function->next)
    {
	if (strcmp (function->name, name) == 0)
	    return function;
    }

    return NULL;
}

static CompProgram *
findFragmentProgram (CompScreen *s,
		     int	*signature,
		     int	nSignature)
{
    CompProgram *program;
    int		i;

    for (program = s->fragmentPrograms; program; program = program->next)
    {
	if (nSignature != program->nSignature)
	    continue;

	for (i = 0; i < nSignature; i++)
	{
	    if (program->signature[i] != signature[i])
		break;
	}

	if (i == nSignature)
	    return program;
    }

    return NULL;
}

static int
functionMaskToType (int mask)
{
    static struct {
	int type;
	int mask;
    } maskToType[] = {
	{ COMP_FUNCTION_TYPE_ARB, COMP_FUNCTION_ARB_MASK }
    };
    int i;

    for (i = 0; i < sizeof (maskToType) / sizeof (maskToType[0]); i++)
	if (mask & maskToType[i].mask)
	    return maskToType[i].type;

    return 0;
}

static void
forEachDataOpInFunction (CompFunction	    **list,
			 int		    index,
			 int		    type,
			 int		    loadTarget,
			 char		    *loadOffset,
			 Bool		    *color,
			 Bool		    *blend,
			 DataOpCallBackProc callBack,
			 void		    *closure)
{
    CompFunction *f = list[index];
    CompDataOp   dataOp;
    char	 data[256];
    Bool	 colorDone = FALSE;
    Bool	 blendDone = FALSE;
    int		 i;

    *color = FALSE;
    *blend = FALSE;

    for (i = 0; i < f->data[type].nBody; i++)
    {
	switch (f->data[type].body[i].type) {
	case CompOpTypeFetch: {
	    char *offset = loadOffset;

	    /* add offset */
	    if (f->data[type].body[i].fetch.offset)
	    {
		if (loadOffset)
		{
		    snprintf (data, 256,
			      "ADD __tmp_texcoord%d, %s, %s;",
			      index, loadOffset,
			      f->data[type].body[i].fetch.offset);

		    dataOp.type = CompOpTypeDataOffset;
		    dataOp.data = data;

		    (*callBack) (&dataOp, index, closure);

		    snprintf (data, 256, "__tmp_texcoord%d", index);

		    offset = data;
		}
		else
		{
		    offset = f->data[type].body[i].fetch.offset;
		}
	    }

	    forEachDataOpInFunction (list, index - 1, type,
				     f->data[type].body[i].fetch.target,
				     offset, &colorDone, &blendDone,
				     callBack, closure);

	    if (strcmp (f->data[type].body[i].fetch.dst, "output"))
	    {
		snprintf (data, 256,
			  "MOV %s, output;",
			  f->data[type].body[i].fetch.dst);

		dataOp.type = CompOpTypeDataStore;
		dataOp.data = data;

		/* move to destination */
		(*callBack) (&dataOp, index, closure);
	    }
	} break;
	case CompOpTypeLoad:
	    if (loadOffset)
	    {
		snprintf (data, 256,
			  "ADD __tmp_texcoord0, fragment.texcoord[0], %s;",
			  loadOffset);

		dataOp.type = CompOpTypeDataOffset;
		dataOp.data = data;

		(*callBack) (&dataOp, index, closure);

		dataOp.data = f->data[type].body[i].load.offset[loadTarget];
	    }
	    else
	    {
		dataOp.data = f->data[type].body[i].load.noOffset[loadTarget];
	    }

	    dataOp.type = CompOpTypeData;

	    (*callBack) (&dataOp, index, closure);

	    break;
	case CompOpTypeColor:
	    if (!colorDone)
	    {
		snprintf (data, 256,
			  "MUL %s, fragment.color, %s;",
			  f->data[type].body[i].color.dst,
			  f->data[type].body[i].color.src);

		dataOp.type = CompOpTypeData;
		dataOp.data = data;

		(*callBack) (&dataOp, index, closure);
	    }
	    else if (strcmp (f->data[type].body[i].color.dst,
			     f->data[type].body[i].color.src))
	    {
		snprintf (data, 256,
			  "MOV %s, %s;",
			  f->data[type].body[i].color.dst,
			  f->data[type].body[i].color.src);

		dataOp.type = CompOpTypeData;
		dataOp.data = data;

		(*callBack) (&dataOp, index, closure);
	    }
	    *color = TRUE;
	    break;
	case CompOpTypeDataBlend:
	    *blend = TRUE;
	    /* fall-through */
	case CompOpTypeData:
	    (*callBack) (&f->data[type].body[i].data, index, closure);
	    break;
	case CompOpTypeDataStore:
	case CompOpTypeDataOffset:
	case CompOpTypeHeaderTemp:
	case CompOpTypeHeaderParam:
	case CompOpTypeHeaderAttrib:
	    break;
	}
    }

    if (colorDone)
	*color = TRUE;

    if (blendDone)
	*blend = TRUE;
}

static int
forEachHeaderOpWithType (CompHeaderOp	    *header,
			 int		    nHeader,
			 int	            index,
			 CompOpType	    type,
			 char		    *prefix,
			 char		    *functionPrefix,
			 int		    count,
			 DataOpCallBackProc callBack,
			 void	            *closure)
{
    CompDataOp dataOp;
    int	       i;

    dataOp.type = CompOpTypeData;

    for (i = 0; i < nHeader; i++)
    {
	if (header[i].type == type)
	{
	    if (count)
	    {
		dataOp.data = ", ";
		(*callBack) (&dataOp, index, closure);
	    }
	    else
	    {
		dataOp.data = prefix;
		(*callBack) (&dataOp, index, closure);
	    }

	    dataOp.data = functionPrefix;
	    (*callBack) (&dataOp, index, closure);

	    dataOp.data = "_";
	    (*callBack) (&dataOp, index, closure);

	    dataOp.data = header[i].name;
	    (*callBack) (&dataOp, index, closure);

	    count++;
	}
    }

    return count;
}

static Bool
forEachDataOp (CompFunction	  **list,
	       int		  nList,
	       int		  type,
	       DataOpCallBackProc callBack,
	       void	          *closure)
{
    CompDataOp dataOp;
    Bool       colorDone;
    Bool       blendDone;
    int	       i, count;

    dataOp.type = CompOpTypeData;

    count = 1;

    dataOp.data = "TEMP output";

    (*callBack) (&dataOp, nList, closure);

    for (i = 0; i < nList; i++)
	count = forEachHeaderOpWithType (list[i]->data[type].header,
					 list[i]->data[type].nHeader,
					 nList, CompOpTypeHeaderTemp,
					 NULL, list[i]->name, count,
					 callBack, closure);

    dataOp.data = ";";

    (*callBack) (&dataOp, nList, closure);

    count = 0;

    for (i = 0; i < nList; i++)
	count = forEachHeaderOpWithType (list[i]->data[type].header,
					 list[i]->data[type].nHeader,
					 nList, CompOpTypeHeaderParam,
					 "PARAM ", list[i]->name, count,
					 callBack, closure);

    if (count)
    {
	dataOp.data = ";";

	(*callBack) (&dataOp, nList, closure);
    }

    count = 0;

    for (i = 0; i < nList; i++)
	count = forEachHeaderOpWithType (list[i]->data[type].header,
					 list[i]->data[type].nHeader,
					 nList, CompOpTypeHeaderAttrib,
					 "ATTRIB ", list[i]->name, count,
					 callBack, closure);

    if (count)
    {
	dataOp.data = ";";

	(*callBack) (&dataOp, nList, closure);
    }

    forEachDataOpInFunction (list, nList - 1, type, 0, NULL,
			     &colorDone, &blendDone,
			     callBack, closure);

    if (colorDone)
	dataOp.data = "MOV result.color, output;END";
    else
	dataOp.data = "MUL result.color, fragment.color, output;END";

    (*callBack) (&dataOp, nList, closure);

    return blendDone;
}

static void
addFetchOffsetVariables (CompDataOp *op,
			 int	    index,
			 void       *closure)
{
    if (op->type == CompOpTypeDataOffset)
    {
	FetchInfo *info = (FetchInfo *) closure;

	if (!info->indices[index])
	{
	    char *str;
	    int	 oldSize = strlen (info->data);
	    int	 newSize;
	    char data[256];

	    snprintf (data, 256, "TEMP __tmp_texcoord%d;", index);

	    newSize = oldSize + strlen (data);

	    str = realloc (info->data, newSize + 1);
	    if (str)
	    {
		strcpy (str + oldSize, data);
		info->data = str;
	    }

	    info->indices[index] = TRUE;
	}
    }
}

static void
addData (CompDataOp *op,
	 int	    index,
	 void       *closure)
{
    FetchInfo *info = (FetchInfo *) closure;
    char      *str;
    int	      oldSize = strlen (info->data);
    int	      newSize = oldSize + strlen (op->data);

    str = realloc (info->data, newSize + 1);
    if (str)
    {
	strcpy (str + oldSize, op->data);
	info->data = str;
    }
}

static CompProgram *
buildFragmentProgram (CompScreen     *s,
		      FragmentAttrib *attrib)
{
    CompProgram	 *program;
    CompFunction **functionList;
    int		 nFunctionList;
    int		 mask = COMP_FUNCTION_MASK;
    int		 type;
    GLint	 errorPos;
    FetchInfo    info;
    int		 i;

    program = malloc (sizeof (CompProgram));
    if (!program)
	return NULL;

    functionList = malloc ((attrib->nFunction + 1) * sizeof (void *));
    if (!functionList)
    {
	free (program);

	return NULL;
    }

    functionList[0] = &initialLoadFunction;
    nFunctionList   = 1;

    for (i = 0; i < attrib->nFunction; i++)
    {
	functionList[nFunctionList] =
	    findFragmentFunction (s, attrib->function[i]);
	if (functionList[nFunctionList])
	    nFunctionList++;
    }

    for (i = 0; i < nFunctionList; i++)
	mask &= functionList[i]->mask;

    if (!mask)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"fragment functions can't be linked together "
			"because a common type doesn't exist");
    }

    if (!mask || nFunctionList == 1)
    {
	free (program);
	free (functionList);

	return NULL;
    }

    program->signature = malloc (attrib->nFunction * sizeof (int));
    if (!program->signature)
    {
	free (program);
	free (functionList);

	return NULL;
    }

    for (i = 0; i < attrib->nFunction; i++)
	program->signature[i] = attrib->function[i];

    program->nSignature = attrib->nFunction;

    type = functionMaskToType (mask);

    info.data = strdup ("!!ARBfp1.0");

    memset (info.indices, 0, sizeof (info.indices));

    forEachDataOp (functionList, nFunctionList, type,
		   addFetchOffsetVariables, (void *) &info);

    program->blending = forEachDataOp (functionList, nFunctionList, type,
				       addData, (void *) &info);

    program->type = GL_FRAGMENT_PROGRAM_ARB;

    glGetError ();

    (*s->genPrograms) (1, &program->name);
    (*s->bindProgram) (GL_FRAGMENT_PROGRAM_ARB, program->name);
    (*s->programString) (GL_FRAGMENT_PROGRAM_ARB,
			 GL_PROGRAM_FORMAT_ASCII_ARB,
			 strlen (info.data), info.data);

    glGetIntegerv (GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
    if (glGetError () != GL_NO_ERROR || errorPos != -1)
    {
	compLogMessage ("core", CompLogLevelError,
			"failed to load fragment program");

	(*s->deletePrograms) (1, &program->name);

	program->name = 0;
	program->type = 0;
    }

    free (info.data);
    free (functionList);

    return program;
}

static GLuint
getFragmentProgram (CompScreen	   *s,
		    FragmentAttrib *attrib,
		    GLenum	   *type,
		    Bool	   *blending)
{
    CompProgram	*program;

    if (!attrib->nFunction)
	return 0;

    program = findFragmentProgram (s, attrib->function, attrib->nFunction);
    if (!program)
    {
	program = buildFragmentProgram (s, attrib);
	if (program)
	{
	    program->next = s->fragmentPrograms;
	    s->fragmentPrograms = program;
	}
    }

    if (program)
    {
	*type     = program->type;
	*blending = program->blending;

	return program->name;
    }

    return 0;
}

CompFunctionData *
createFunctionData (void)
{
    CompFunctionData *data;

    data = malloc (sizeof (CompFunctionData));
    if (!data)
	return NULL;

    data->header  = NULL;
    data->nHeader = 0;

    data->body  = NULL;
    data->nBody = 0;

    return data;
}

static void
finiFunctionData (CompFunctionData *data)
{
    int i;

    for (i = 0; i < data->nHeader; i++)
	free (data->header[i].name);

    if (data->header)
	free (data->header);

    for (i = 0; i < data->nBody; i++)
    {
	switch (data->body[i].type) {
	case CompOpTypeFetch:
	    free (data->body[i].fetch.dst);
	    if (data->body[i].fetch.offset)
		free (data->body[i].fetch.offset);
	    break;
	case CompOpTypeLoad:
	case CompOpTypeHeaderTemp:
	case CompOpTypeHeaderParam:
	case CompOpTypeHeaderAttrib:
	    break;
	case CompOpTypeData:
	case CompOpTypeDataBlend:
	case CompOpTypeDataStore:
	case CompOpTypeDataOffset:
	    free (data->body[i].data.data);
	    break;
	case CompOpTypeColor:
	    free (data->body[i].color.dst);
	    free (data->body[i].color.src);
	    break;
	}
    }

    if (data->body)
	free (data->body);
}

void
destroyFunctionData (CompFunctionData *data)
{
    finiFunctionData (data);
    free (data);
}

/* performs simple variable substitution */
static char *
copyData (CompHeaderOp *header,
	  int	       nHeader,
	  const char   *prefix,
	  char	       *data)
{
    char *copy, *needle, *haystack, *dst, *first;
    int  i, index, length, prefixLength, count = 0;

    prefixLength = strlen (prefix);

    for (i = 0; i < nHeader; i++)
    {
	length   = strlen (header[i].name);
	haystack = data;

	do {
	    needle = strstr (haystack, header[i].name);
	    if (needle)
	    {
		haystack = needle + length;
		count++;
	    }
	} while (needle);
    }

    /* allocate new memory that will fit all substitutions */
    copy = malloc (strlen (data) + count * (strlen (prefix) + 1) + 1);
    if (!copy)
	return NULL;

    haystack = data;
    dst      = copy;

    for (;;)
    {
	first = NULL;
	index = 0;

	for (i = 0; i < nHeader; i++)
	{
	    needle = strstr (haystack, header[i].name);
	    if (needle && (!first || needle < first))
	    {
		first = needle;
		index = i;
	    }
	}

	if (first)
	{
	    int length;

	    if (first > haystack)
	    {
		strncpy (dst, haystack, first - haystack);
		dst += first - haystack;
	    }

	    length = strlen (header[index].name);

	    strcpy (dst, prefix);
	    dst += prefixLength;
	    *dst++ = '_';
	    strcpy (dst, header[index].name);
	    dst += length;

	    haystack = first + length;
	}
	else
	{
	    strcpy (dst, haystack);
	    break;
	}
    }

    return copy;
}

static Bool
copyFunctionData (CompFunctionData	 *dst,
		  const CompFunctionData *src,
		  const char		 *dstPrefix)
{
    int i;

    dst->header = malloc (src->nHeader * sizeof (CompHeaderOp));
    if (!dst->header)
	return FALSE;

    dst->body = malloc (src->nBody * sizeof (CompBodyOp));
    if (!dst->body)
    {
	free (dst->header);
	return FALSE;
    }

    dst->nHeader = src->nHeader;

    for (i = 0; i < src->nHeader; i++)
    {
	dst->header[i].type = src->header[i].type;
	dst->header[i].name = strdup (src->header[i].name);
    }

    dst->nBody = src->nBody;

    for (i = 0; i < src->nBody; i++)
    {
	dst->body[i].type = src->body[i].type;

	switch (src->body[i].type) {
	case CompOpTypeFetch:
	    dst->body[i].fetch.dst = copyData (dst->header,
					       dst->nHeader,
					       dstPrefix,
					       src->body[i].fetch.dst);
	    if (src->body[i].fetch.offset)
		dst->body[i].fetch.offset =
		    copyData (dst->header,
			      dst->nHeader,
			      dstPrefix,
			      src->body[i].fetch.offset);
	    else
		dst->body[i].fetch.offset = NULL;

	    dst->body[i].fetch.target = src->body[i].fetch.target;
	    break;
	case CompOpTypeLoad:
	case CompOpTypeHeaderTemp:
	case CompOpTypeHeaderParam:
	case CompOpTypeHeaderAttrib:
	    break;
	case CompOpTypeData:
	case CompOpTypeDataBlend:
	case CompOpTypeDataStore:
	case CompOpTypeDataOffset:
	    dst->body[i].data.data = copyData (dst->header,
					       dst->nHeader,
					       dstPrefix,
					       src->body[i].data.data);
	    break;
	case CompOpTypeColor:
	    dst->body[i].color.dst = copyData (dst->header,
					       dst->nHeader,
					       dstPrefix,
					       src->body[i].color.dst);
	    dst->body[i].color.src = copyData (dst->header,
					       dst->nHeader,
					       dstPrefix,
					       src->body[i].color.src);
	    break;
	}
    }

    return TRUE;
}

static Bool
addHeaderOpToFunctionData (CompFunctionData *data,
			   const char	    *name,
			   CompOpType	    type)
{
    static char  *reserved[] = {
	"output",
	"__tmp_texcoord",
	"fragment",
	"program",
	"result",
	"state",
	"texture"
    };
    CompHeaderOp *header;
    int		 i;

    for (i = 0; i < sizeof (reserved) / sizeof (reserved[0]); i++)
    {
	if (strncmp (name, reserved[i], strlen (reserved[i])) == 0)
	{
	    compLogMessage ("core", CompLogLevelWarn,
			    "%s is a reserved word", name);
	    return FALSE;
	}
    }

    header = realloc (data->header,
		      (data->nHeader + 1) * sizeof (CompHeaderOp));
    if (!header)
	return FALSE;

    header[data->nHeader].type = type;
    header[data->nHeader].name = strdup (name);

    data->header = header;
    data->nHeader++;

    return TRUE;
}

Bool
addTempHeaderOpToFunctionData (CompFunctionData *data,
			       const char	*name)
{
    return addHeaderOpToFunctionData (data, name, CompOpTypeHeaderTemp);
}

Bool
addParamHeaderOpToFunctionData (CompFunctionData *data,
				const char	 *name)
{
    return addHeaderOpToFunctionData (data, name, CompOpTypeHeaderParam);
}

Bool
addAttribHeaderOpToFunctionData (CompFunctionData *data,
				 const char	  *name)
{
    return addHeaderOpToFunctionData (data, name, CompOpTypeHeaderAttrib);
}

static Bool
allocBodyOpInFunctionData (CompFunctionData *data)
{
    CompBodyOp *body;

    body = realloc (data->body, (data->nBody + 1) * sizeof (CompBodyOp));
    if (!body)
	return FALSE;

    data->body = body;
    data->nBody++;

    return TRUE;
}

Bool
addFetchOpToFunctionData (CompFunctionData *data,
			  const char	   *dst,
			  const char	   *offset,
			  int		   target)
{
    int index = data->nBody;

    if (!allocBodyOpInFunctionData (data))
	return FALSE;

    data->body[index].type	   = CompOpTypeFetch;
    data->body[index].fetch.dst    = strdup (dst);
    data->body[index].fetch.target = target;

    if (offset)
	data->body[index].fetch.offset = strdup (offset);
    else
	data->body[index].fetch.offset = NULL;

    return TRUE;
}

Bool
addColorOpToFunctionData (CompFunctionData *data,
			  const char	   *dst,
			  const char	   *src)
{
    int index = data->nBody;

    if (!allocBodyOpInFunctionData (data))
	return FALSE;

    data->body[index].type      = CompOpTypeColor;
    data->body[index].color.dst = strdup (dst);
    data->body[index].color.src = strdup (src);

    return TRUE;
}

Bool
addDataOpToFunctionData (CompFunctionData *data,
			 const char	  *str,
			 ...)
{
    int     index = data->nBody;
    int     size  = strlen (str) + 1;
    int     n;
    char    *fStr;
    char    *tmp;
    va_list ap;

    if (!allocBodyOpInFunctionData (data))
	return FALSE;

    fStr = malloc (size);
    if (!fStr)
	return FALSE;

    while (1)
    {
	/* Try to print in the allocated space. */
	va_start (ap, str);
	n = vsnprintf (fStr, size, str, ap);
	va_end (ap);

	/* If that worked, leave the loop. */
	if (n > -1 && n < size)
	    break;

	/* Else try again with more space. */
	if (n > -1)       /* glibc 2.1 */
	    size = n + 1; /* precisely what is needed */
	else              /* glibc 2.0 */
	    size++;       /* one more than the old size */

	tmp = realloc (fStr, size);
	if (!tmp)
	{
	    free (fStr);
	    return FALSE;
	}
	else
	{
	    fStr = tmp;
	}
    }

    data->body[index].type	= CompOpTypeData;
    data->body[index].data.data = fStr;

    return TRUE;
}

Bool
addBlendOpToFunctionData (CompFunctionData *data,
			  const char	   *str,
			  ...)
{
    int     index = data->nBody;
    int     size  = strlen (str) + 1;
    int     n;
    char    *fStr;
    char    *tmp;
    va_list ap;

    if (!allocBodyOpInFunctionData (data))
	return FALSE;

    fStr = malloc (size);
    if (!fStr)
	return FALSE;

    while (1)
    {
	/* Try to print in the allocated space. */
	va_start (ap, str);
	n = vsnprintf (fStr, size, str, ap);
	va_end (ap);

	/* If that worked, leave the loop. */
	if (n > -1 && n < size)
	    break;

	/* Else try again with more space. */
	if (n > -1)       /* glibc 2.1 */
	    size = n + 1; /* precisely what is needed */
	else              /* glibc 2.0 */
	    size++;       /* one more than the old size */

	tmp = realloc (fStr, size);
	if (!tmp)
	{
	    free (fStr);
	    return FALSE;
	}
	else
	{
	    fStr = tmp;
	}
    }

    data->body[index].type	= CompOpTypeDataBlend;
    data->body[index].data.data = fStr;

    return TRUE;
}

static int
allocFunctionId (CompScreen *s)
{
    return ++s->lastFunctionId;
}

int
createFragmentFunction (CompScreen	 *s,
			const char	 *name,
			CompFunctionData *data)
{
    CompFunction *function;
    const char	 *validName = name;
    char	 *nameBuffer = NULL;
    int		 i = 0;

    while (findFragmentFunctionWithName (s, validName))
    {
	if (!nameBuffer)
	{
	    nameBuffer = malloc (strlen (name) + 64);
	    if (!nameBuffer)
		return 0;

	    validName = nameBuffer;
	}

	sprintf (nameBuffer, "%s%d", name, i++);
    }

    function = malloc (sizeof (CompFunction));
    if (!function)
    {
	if (nameBuffer)
	    free (nameBuffer);

	return 0;
    }

    if (!copyFunctionData (&function->data[COMP_FUNCTION_TYPE_ARB], data,
			   validName))
    {
	free (function);
	if (nameBuffer)
	    free (nameBuffer);

	return 0;
    }

    function->name = strdup (validName);
    function->mask = COMP_FUNCTION_ARB_MASK;
    function->id   = allocFunctionId (s);

    function->next = s->fragmentFunctions;
    s->fragmentFunctions = function;

    if (nameBuffer)
	free (nameBuffer);

    return function->id;
}

void
destroyFragmentFunction (CompScreen *s,
			 int	    id)
{
    CompFunction *function, *prevFunction = NULL;
    CompProgram  *program, *prevProgram = NULL;
    int		 i;

    for (function = s->fragmentFunctions; function; function = function->next)
    {
	if (function->id == id)
	    break;

	prevFunction = function;
    }

    if (!function)
	return;

    program = s->fragmentPrograms;
    while (program)
    {
	for (i = 0; i < program->nSignature; i++)
	{
	    if (program->signature[i] == id)
		break;
	}

	if (i < program->nSignature)
	{
	    CompProgram *tmp = program;

	    if (prevProgram)
		prevProgram->next = program->next;
	    else
		s->fragmentPrograms = program->next;

	    program = program->next;

	    (*s->deletePrograms) (1, &tmp->name);

	    free (tmp->signature);
	    free (tmp);
	}
	else
	{
	    prevProgram = program;
	    program = program->next;
	}
    }

    if (prevFunction)
	prevFunction->next = function->next;
    else
	s->fragmentFunctions = function->next;

    finiFunctionData (&function->data[COMP_FUNCTION_TYPE_ARB]);
    free (function->name);
    free (function);
}

int
getSaturateFragmentFunction (CompScreen  *s,
			     CompTexture *texture,
			     int	 param)
{
    int target;

    if (param >= 64)
	return 0;

    if (texture->target == GL_TEXTURE_2D)
	target = COMP_FETCH_TARGET_2D;
    else
	target = COMP_FETCH_TARGET_RECT;

    if (!s->saturateFunction[target][param])
    {
	static const char *saturateData =
	    "MUL temp, output, { 1.0, 1.0, 1.0, 0.0 };"
	    "DP3 temp, temp, program.env[%d];"
	    "LRP output.xyz, program.env[%d].w, output, temp;";
	CompFunctionData  *data;

	data = createFunctionData ();
	if (data)
	{
	    char str[1024];

	    if (!addTempHeaderOpToFunctionData (data, "temp"))
	    {
		destroyFunctionData (data);
		return 0;
	    }

	    if (!addFetchOpToFunctionData (data, "output", NULL, target))
	    {
		destroyFunctionData (data);
		return 0;
	    }

	    if (!addColorOpToFunctionData (data, "output", "output"))
	    {
		destroyFunctionData (data);
		return 0;
	    }

	    snprintf (str, 1024, saturateData, param, param);

	    if (!addDataOpToFunctionData (data, str))
	    {
		destroyFunctionData (data);
		return 0;
	    }

	    s->saturateFunction[target][param] =
		createFragmentFunction (s, "__core_saturate", data);

	    destroyFunctionData (data);
	}
    }

    return s->saturateFunction[target][param];
}

int
allocFragmentTextureUnits (FragmentAttrib *attrib,
			   int		  nTexture)
{
    int first = attrib->nTexture;

    attrib->nTexture += nTexture;

    /* 0 is reserved for source texture */
    return 1 + first;
}

int
allocFragmentParameters (FragmentAttrib *attrib,
			 int		nParam)
{
    int first = attrib->nParam;

    attrib->nParam += nParam;

    return first;
}

void
addFragmentFunction (FragmentAttrib *attrib,
		     int	    function)
{
    if (attrib->nFunction < MAX_FRAGMENT_FUNCTIONS)
	attrib->function[attrib->nFunction++] = function;
}

void
initFragmentAttrib (FragmentAttrib	    *attrib,
		    const WindowPaintAttrib *paint)
{
    attrib->opacity    = paint->opacity;
    attrib->brightness = paint->brightness;
    attrib->saturation = paint->saturation;
    attrib->nTexture   = 0;
    attrib->nFunction  = 0;
    attrib->nParam     = 0;

    memset (attrib->function, 0, sizeof (attrib->function));
}

Bool
enableFragmentAttrib (CompScreen     *s,
		      FragmentAttrib *attrib,
		      Bool	     *blending)
{
    GLuint name;
    GLenum type;
    Bool   programBlending;

    if (!s->fragmentProgram)
	return FALSE;

    name = getFragmentProgram (s, attrib, &type, &programBlending);
    if (!name)
	return FALSE;

    *blending = !programBlending;

    glEnable (GL_FRAGMENT_PROGRAM_ARB);

    (*s->bindProgram) (type, name);

    return TRUE;
}

void
disableFragmentAttrib (CompScreen     *s,
		       FragmentAttrib *attrib)
{
    glDisable (GL_FRAGMENT_PROGRAM_ARB);
}
