<!--
    Copyright © 2007 Novell, Inc.

    Permission to use, copy, modify, distribute, and sell this software
    and its documentation for any purpose is hereby granted without
    fee, provided that the above copyright notice appear in all copies
    and that both that copyright notice and this permission notice
    appear in supporting documentation, and that the name of
    Novell, Inc. not be used in advertising or publicity pertaining to
    distribution of the software without specific, written prior permission.
    Novell, Inc. makes no representations about the suitability of this
    software for any purpose. It is provided "as is" without express or
    implied warranty.

    NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
    INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
    NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
    CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
    OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
    NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
    WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

    Author: David Reveman <davidr@novell.com>
  -->

<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

<xsl:output method="text"/>

<xsl:template match="/kcfg">
  <xsl:for-each select="/kcfg/group">
    <xsl:variable name="prefix">
      <xsl:value-of select="substring(@name,1,string-length(@name) - 9)"/>
    </xsl:variable>
    <xsl:variable name="suffix">
      <xsl:value-of select="substring(@name,string-length(@name) - 8,string-length(@name))"/>
    </xsl:variable>
    <xsl:text>[</xsl:text>
    <xsl:choose>
      <xsl:when test="$suffix = '$(screen)'">
	<xsl:value-of select="$prefix"/>
	<xsl:value-of select="$screen"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="@name"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>]&#10;</xsl:text>
    <xsl:for-each select="entry">
      <xsl:value-of select="@key"/>
      <xsl:text>=</xsl:text>
      <xsl:value-of select="default/text()"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:for-each>
    <xsl:text>&#10;</xsl:text>
  </xsl:for-each>
</xsl:template>

</xsl:stylesheet>
