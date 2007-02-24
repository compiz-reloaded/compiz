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

#include <compiz.h>

#include <stdlib.h>
#include <string.h>

static void
matchResetOps (CompDisplay *display,
	       CompMatchOp *op,
	       int	   nOp)
{
    while (nOp--)
    {
	switch (op->type) {
	case CompMatchOpTypeGroup:
	    matchResetOps (display, op->group.op, op->group.nOp);
	    break;
	case CompMatchOpTypeExp:
	    if (op->exp.e.fini)
	    {
		(*op->exp.e.fini) (display, op->exp.e.private);
		op->exp.e.fini = NULL;
	    }

	    op->exp.e.eval	  = NULL;
	    op->exp.e.private.val = 0;
	    break;
	}

	op++;
    }
}

static void
matchReset (CompMatch *match)
{
    if (match->display)
	matchResetOps (match->display, match->op, match->nOp);

    match->display = NULL;
}

void
matchInit (CompMatch *match)
{
    match->display = NULL;
    match->op	   = NULL;
    match->nOp	    = 0;
}

static void
matchFiniOps (CompMatchOp *op,
	      int	  nOp)
{
    while (nOp--)
    {
	switch (op->type) {
	case CompMatchOpTypeGroup:
	    matchFiniOps (op->group.op, op->group.nOp);
	    free (op->group.op);
	    break;
	case CompMatchOpTypeExp:
	    free (op->exp.value);
	    break;
	}

	op++;
    }
}

void
matchFini (CompMatch *match)
{
    matchReset (match);
    matchFiniOps (match->op, match->nOp);
    free (match->op);
}

static CompMatchOp *
matchAddOp (CompMatch	    *match,
	    CompMatchOpType type,
	    int		    flags)
{
    CompMatchOp *op;

    /* remove AND prefix if this is the first op in this group */
    if (!match->nOp)
	flags &= ~MATCH_EXP_AND_MASK;

    op = realloc (match->op, sizeof (CompMatchOp) * (match->nOp + 1));
    if (!op)
	return FALSE;

    op[match->nOp].any.type  = type;
    op[match->nOp].any.flags = flags;

    match->op = op;
    match->nOp++;

    return &match->op[match->nOp - 1];
}

static Bool
matchCopyOps (CompMatchOp *opSrc,
	      int	   nOpSrc,
	      CompMatchOp *opDst)
{
    CompMatchOp *op, *first = opDst;
    int		count = 0;

    while (nOpSrc--)
    {
	opDst->any.type  = opSrc->any.type;
	opDst->any.flags = opSrc->any.flags;

	switch (opSrc->type) {
	case CompMatchOpTypeGroup:
	    op = malloc (sizeof (CompMatchOp) * opSrc->group.nOp);
	    if (!op)
	    {
		matchFiniOps (first, count);
		return FALSE;
	    }

	    if (!matchCopyOps (opSrc->group.op, opSrc->group.nOp, op))
	    {
		free (op);
		matchFiniOps (first, count);
		return FALSE;
	    }

	    opDst->group.op  = op;
	    opDst->group.nOp = opSrc->group.nOp;
	    break;
	case CompMatchOpTypeExp:
	    opDst->exp.value = strdup (opSrc->exp.value);
	    if (!opDst->exp.value)
	    {
		matchFiniOps (first, count);
		return FALSE;
	    }

	    opDst->exp.e.fini	     = NULL;
	    opDst->exp.e.eval	     = NULL;
	    opDst->exp.e.private.val = 0;
	    break;
	}

	count++;
	opDst++;
	opSrc++;
    }

    return TRUE;
}

Bool
matchAddGroup (CompMatch *match,
	       int	 flags,
	       CompMatch *group)
{
    CompMatchOp *op, *opDst;

    opDst = malloc (sizeof (CompMatchOp) * group->nOp);
    if (!opDst)
	return FALSE;

    if (!matchCopyOps (group->op, group->nOp, opDst))
    {
	free (opDst);
	return FALSE;
    }

    op = matchAddOp (match, CompMatchOpTypeGroup, flags);
    if (!op)
    {
	matchFiniOps (opDst, group->nOp);
	free (opDst);
	return FALSE;
    }

    op->group.op  = opDst;
    op->group.nOp = group->nOp;

    return TRUE;
}

Bool
matchAddExp (CompMatch *match,
	     int       flags,
	     char      *str)
{
    CompMatchOp *op;
    char	*value;

    value = strdup (str);
    if (!value)
	return FALSE;

    op = matchAddOp (match, CompMatchOpTypeExp, flags);
    if (!op)
    {
	free (value);
	return FALSE;
    }

    op->exp.value	  = value;
    op->exp.e.fini	  = NULL;
    op->exp.e.eval	  = NULL;
    op->exp.e.private.val = 0;

    return TRUE;
}

static int
nextIndex (char *str,
	   int  i)
{
    while (str[i] == '\\')
	if (str[++i] != '\0')
	    i++;

    return i;
}

static char *
strndupValue (char *str,
	      int  n)
{
    char *value;

    value = malloc (sizeof (char) * (n + 1));
    if (value)
    {
	int i, j;

	/* count trialing white spaces */
	i = j = 0;
	while (i < n)
	{
	    if (str[i] != ' ')
	    {
		j = 0;
		if (str[i] == '\\')
		    i++;
	    }
	    else
	    {
		j++;
	    }

	    i++;
	}

	/* remove trialing white spaces */
	n -= j;

	i = j = 0;
	for (;;)
	{
	    if (str[i] == '\\')
		i++;

	    value[j++] = str[i++];

	    if (i >= n)
	    {
		value[j] = '\0';
		return value;
	    }
	}
    }

    return NULL;
}

/*
  Add match expressions from string. Special characters are
  '(', ')', '!', '&', '|'. Escape character is '\'.

  Example:

  "type=desktop | !type=dock"
  "!type=dock & (state=fullscreen | state=shaded)"
*/
void
matchAddFromString (CompMatch *match,
		    char      *str)
{
    char *value;
    int	 j, i = 0;
    int	 flags = 0;

    while (str[i] != '\0')
    {
	while (str[i] == ' ')
	    i++;

	if (str[i] == '!')
	{
	    flags |= MATCH_EXP_NOT_MASK;

	    i++;
	    while (str[i] == ' ')
		i++;
	}

	if (str[i] == '(')
	{
	    int	level = 1;
	    int length;

	    j = i++;

	    while (str[j] != '\0')
	    {
		if (str[j] == '(')
		{
		    level++;
		}
		else if (str[j] == ')')
		{
		    level--;
		    if (level == 0)
			break;
		}

		j = nextIndex (str, ++j);
	    }

	    length = j - i;

	    value = malloc (sizeof (char) * (length + 1));
	    if (value)
	    {
		CompMatch group;

		strncpy (value, &str[i], length);
		value[length] = '\0';

		matchInit (&group);
		matchAddFromString (&group, value);
		matchAddGroup (match, flags, &group);
		matchFini (&group);

		free (value);
	    }

	    while (str[j] != '\0' && str[j] != '|' && str[j] != '&')
		j++;
	}
	else
	{
	    j = i;

	    while (str[j] != '\0' && str[j] != '|' && str[j] != '&')
		j = nextIndex (str, ++j);

	    value = strndupValue (&str[i], j - i);
	    if (value)
	    {
		matchAddExp (match, flags, value);

		free (value);
	    }
	}

	i = j;

	if (str[i] != '\0')
	{
	    if (str[i] == '&')
		flags = MATCH_EXP_AND_MASK;

	    i++;
	}
    }
}

static void
matchUpdateOps (CompDisplay *display,
		CompMatchOp *op,
		int	    nOp)
{
    while (nOp--)
    {
	switch (op->type) {
	case CompMatchOpTypeGroup:
	    matchUpdateOps (display, op->group.op, op->group.nOp);
	    break;
	case CompMatchOpTypeExp:
	    (*display->matchInitExp) (display, &op->exp.e, op->exp.value);
	    break;
	}

	op++;
    }
}

void
matchUpdate (CompDisplay *display,
	     CompMatch   *match)
{
    matchReset (match);
    matchUpdateOps (display, match->op, match->nOp);
    match->display = display;
}

static Bool
matchEvalOps (CompDisplay *display,
	      CompMatchOp *op,
	      int	  nOp,
	      CompWindow  *window)
{
    Bool value, result = FALSE;

    while (nOp--)
    {
	/* fast evaluation */
	if (op->any.flags & MATCH_EXP_AND_MASK)
	{
	    /* result will never be true */
	    if (!result)
		return FALSE;
	}
	else
	{
	    /* result will always be true */
	    if (result)
		return TRUE;
	}

	switch (op->type) {
	case CompMatchOpTypeGroup:
	    value = matchEvalOps (display, op->group.op, op->group.nOp, window);
	    break;
	case CompMatchOpTypeExp:
	default:
	    value = (*op->exp.e.eval) (display, window, op->exp.e.private);
	    break;
	}

	if (op->any.flags & MATCH_EXP_NOT_MASK)
	    value = !value;

	if (op->any.flags & MATCH_EXP_AND_MASK)
	    result = (result && value);
	else
	    result = (result || value);

	op++;
    }

    return result;
}

Bool
matchEval (CompMatch  *match,
	   CompWindow *window)
{
    if (match->display)
	return matchEvalOps (match->display, match->op, match->nOp, window);

    return FALSE;
}

static Bool
matchEvalTypeExp (CompDisplay *display,
		  CompWindow  *window,
		  CompPrivate private)
{
    return (private.uval & window->wmType);
}

void
matchInitExp (CompDisplay  *display,
	      CompMatchExp *exp,
	      char	   *value)
{
    exp->eval = matchEvalTypeExp;

    if (strncmp (value, "type=", 5) == 0)
	value += 5;

    exp->private.uval = compWindowTypeFromString (value);
}

void
matchExpHandlerChanged (CompDisplay *display)
{
}

void
matchPropertyChanged (CompDisplay *display,
		      CompWindow  *window)
{
}
