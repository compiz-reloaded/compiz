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

#include <stdlib.h>
#include <string.h>

#include <compiz-core.h>

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
		(*op->exp.e.fini) (display, op->exp.e.priv);
		op->exp.e.fini = NULL;
	    }

	    op->exp.e.eval     = NULL;
	    op->exp.e.priv.val = 0;
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

static Bool
matchOpsEqual (CompMatchOp *op1,
	       CompMatchOp *op2,
	       int	   nOp)
{
    while (nOp--)
    {
	if (op1->type != op2->type)
	    return FALSE;

	switch (op1->type) {
	case CompMatchOpTypeGroup:
	    if (op1->group.nOp != op2->group.nOp)
		return FALSE;

	    if (!matchOpsEqual (op1->group.op, op2->group.op, op1->group.nOp))
		return FALSE;

	    break;
	case CompMatchOpTypeExp:
	    if (op1->exp.flags != op2->exp.flags)
		return FALSE;

	    if (strcmp (op1->exp.value, op2->exp.value))
		return FALSE;

	    break;
	}

	op1++;
	op2++;
    }

    return TRUE;
}

Bool
matchEqual (CompMatch *m1,
	    CompMatch *m2)
{
    if (m1->nOp != m2->nOp)
	return FALSE;

    return matchOpsEqual (m1->op, m2->op, m1->nOp);
}

static CompMatchOp *
matchAddOp (CompMatch	    *match,
	    CompMatchOpType type,
	    int		    flags)
{
    CompMatchOp *op;

    /* remove AND prefix if this is the first op in this group */
    if (!match->nOp)
	flags &= ~MATCH_OP_AND_MASK;

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
matchCopyOps (CompMatchOp *opDst,
	      CompMatchOp *opSrc,
	      int	   nOpSrc)
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

	    if (!matchCopyOps (op, opSrc->group.op, opSrc->group.nOp))
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

	    opDst->exp.e.fini	  = NULL;
	    opDst->exp.e.eval	  = NULL;
	    opDst->exp.e.priv.val = 0;
	    break;
	}

	count++;
	opDst++;
	opSrc++;
    }

    return TRUE;
}

Bool
matchCopy (CompMatch *dst,
	   CompMatch *src)
{
    CompMatchOp *opDst;

    opDst = malloc (sizeof (CompMatchOp) * src->nOp);
    if (!opDst)
	return FALSE;

    if (!matchCopyOps (opDst, src->op, src->nOp))
    {
	free (opDst);
	return FALSE;
    }

    dst->op  = opDst;
    dst->nOp = src->nOp;

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

    if (!matchCopyOps (opDst, group->op, group->nOp))
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
matchAddExp (CompMatch  *match,
	     int        flags,
	     const char *str)
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

    op->exp.value      = value;
    op->exp.e.fini     = NULL;
    op->exp.e.eval     = NULL;
    op->exp.e.priv.val = 0;

    return TRUE;
}

static int
nextIndex (const char *str,
	   int	      i)
{
    while (str[i] == '\\')
	if (str[++i] != '\0')
	    i++;

    return i;
}

static char *
strndupValue (const char *str,
	      int	 n)
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
matchAddFromString (CompMatch  *match,
		    const char *str)
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
	    flags |= MATCH_OP_NOT_MASK;

	    i++;
	    while (str[i] == ' ')
		i++;
	}

	if (str[i] == '(')
	{
	    int	level = 1;
	    int length;

	    j = ++i;

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
		flags = MATCH_OP_AND_MASK;

	    i++;
	}
    }
}

static char *
matchOpsToString (CompMatchOp *op,
		  int	      nOp)
{
    char *value, *group;
    char *str = NULL;
    int  length = 0;

    while (nOp--)
    {
	value = NULL;

	switch (op->type) {
	case CompMatchOpTypeGroup:
	    group = matchOpsToString (op->group.op, op->group.nOp);
	    if (group)
	    {
		value = malloc (sizeof (char) * (strlen (group) + 7));
		if (value)
		    sprintf (value, "%s%s(%s)%s", !str ? "" :
			     ((op->any.flags & MATCH_OP_AND_MASK) ?
			      "& " : "| "),
			     (op->any.flags & MATCH_OP_NOT_MASK) ? "!" : "",
			     group, nOp ? " " : "");

		free (group);
	    }
	    break;
	case CompMatchOpTypeExp:
	    value = malloc (sizeof (char) * (strlen (op->exp.value) + 5));
	    if (value)
		sprintf (value, "%s%s%s%s", !str ? "" :
			 ((op->any.flags & MATCH_OP_AND_MASK) ? "& " : "| "),
			 (op->any.flags & MATCH_OP_NOT_MASK) ? "!" : "",
			 op->exp.value, nOp ? " " : "");
	    break;
	}

	if (value)
	{
	    char *s;
	    int  valueLength = strlen (value);

	    s = malloc (sizeof (char) * (length + valueLength + 1));
	    if (s)
	    {
		if (str)
		    memcpy (s, str, sizeof (char) * length);

		memcpy (s + length, value, sizeof (char) * valueLength);

		length += valueLength;

		s[length] = '\0';

		if (str)
		    free (str);

		str = s;
	    }

	    free (value);
	}

	op++;
    }

    return str;
}

char *
matchToString (CompMatch *match)
{
    char *str;

    str = matchOpsToString (match->op, match->nOp);
    if (!str)
	str = strdup ("");

    return str;
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
	if (op->any.flags & MATCH_OP_AND_MASK)
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
	    value = (*op->exp.e.eval) (display, window, op->exp.e.priv);
	    break;
	}

	if (op->any.flags & MATCH_OP_NOT_MASK)
	    value = !value;

	if (op->any.flags & MATCH_OP_AND_MASK)
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

static Bool
matchEvalStateExp (CompDisplay *display,
		   CompWindow  *window,
		   CompPrivate private)
{
    return (private.uval & window->state);
}

static Bool
matchEvalIdExp (CompDisplay *display,
		CompWindow  *window,
		CompPrivate private)
{
    return (private.val == window->id);
}

static Bool
matchEvalOverrideRedirectExp (CompDisplay *display,
			      CompWindow  *window,
			      CompPrivate private)
{
    Bool overrideRedirect = window->attrib.override_redirect;
    return ((private.val == 1 && overrideRedirect) ||
	    (private.val == 0 && !overrideRedirect));
}

static Bool
matchEvalAlphaExp (CompDisplay *display,
		   CompWindow  *window,
		   CompPrivate private)
{
    return ((private.val && window->alpha) ||
	    (!private.val && !window->alpha));
}

void
matchInitExp (CompDisplay  *display,
	      CompMatchExp *exp,
	      const char   *value)
{
    if (strncmp (value, "xid=", 4) == 0)
    {
	exp->eval     = matchEvalIdExp;
	exp->priv.val = strtol (value + 4, NULL, 0);
    }
    else if (strncmp (value, "state=", 6) == 0)
    {
	exp->eval      = matchEvalStateExp;
	exp->priv.uval = windowStateFromString (value + 6);
    }
    else if (strncmp (value, "override_redirect=", 18) == 0)
    {
	exp->eval     = matchEvalOverrideRedirectExp;
	exp->priv.val = strtol (value + 18, NULL, 0);
    }
    else if (strncmp (value, "rgba=", 5) == 0)
    {
	exp->eval     = matchEvalAlphaExp;
	exp->priv.val = strtol (value + 5, NULL, 0);
    }
    else
    {
	if (strncmp (value, "type=", 5) == 0)
	    value += 5;

	exp->eval      = matchEvalTypeExp;
	exp->priv.uval = windowTypeFromString (value);
    }
}

static void
matchUpdateMatchOptions (CompOption *option,
			 int	    nOption)
{
    while (nOption--)
    {
	switch (option->type) {
	case CompOptionTypeMatch:
	    if (option->value.match.display)
		matchUpdate (option->value.match.display, &option->value.match);
	    break;
	case CompOptionTypeList:
	    if (option->value.list.type == CompOptionTypeMatch)
	    {
		int i;

		for (i = 0; i < option->value.list.nValue; i++)
		    if (option->value.list.value[i].match.display)
			matchUpdate (option->value.list.value[i].match.display,
				     &option->value.list.value[i].match);
	    }
	default:
	    break;
	}

	option++;
    }
}

void
matchExpHandlerChanged (CompDisplay *display)
{
    CompOption *option;
    int	       nOption;
    CompPlugin *p;
    CompScreen *s;

    for (p = getPlugins (); p; p = p->next)
    {
	if (!p->vTable->getObjectOptions)
	    continue;

	option = (*p->vTable->getObjectOptions) (p, &display->base, &nOption);
	matchUpdateMatchOptions (option, nOption);
    }

    for (s = display->screens; s; s = s->next)
    {
	for (p = getPlugins (); p; p = p->next)
	{
	    if (!p->vTable->getObjectOptions)
		continue;

	    option = (*p->vTable->getObjectOptions) (p, &s->base, &nOption);
	    matchUpdateMatchOptions (option, nOption);
	}
    }
}

void
matchPropertyChanged (CompDisplay *display,
		      CompWindow  *w)
{
}
