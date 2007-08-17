<!--
    Copyright © 2007 Dennis Kasprzyk
    Copyright © 2007 Novell, Inc.

    Permission to use, copy, modify, distribute, and sell this software
    and its documentation for any purpose is hereby granted without
    fee, provided that the above copyright notice appear in all copies
    and that both that copyright notice and this permission notice
    appear in supporting documentation, and that the name of
    Dennis Kasprzyk not be used in advertising or publicity pertaining to
    distribution of the software without specific, written prior permission.
    Dennis Kasprzyk makes no representations about the suitability of this
    software for any purpose. It is provided "as is" without express or
    implied warranty.

    DENNIS KASPRZYK DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
    INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
    NO EVENT SHALL DENNIS KASPRZYK BE LIABLE FOR ANY SPECIAL, INDIRECT OR
    CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
    OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
    NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
    WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

    Authors: Dennis Kasprzyk <onestone@deltatauchi.de>
             David Reveman <davidr@novell.com>
  -->

<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

  <xsl:output method="xml" indent="yes"/>

  <xsl:template match="/compiz">
    <kcfg>
      <kcfgfile name="compizrc">
	<parameter name="screen"/>
      </kcfgfile>
      <xsl:for-each select="/compiz/*/display | /compiz/*/screen">
	<group>
	  <xsl:variable name="group">
	    <xsl:choose>
	      <xsl:when test="ancestor::plugin">
		<xsl:value-of select="ancestor::plugin/@name"/>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:text>core</xsl:text>
	      </xsl:otherwise>
	    </xsl:choose>
	    <xsl:text>_</xsl:text>
	    <xsl:value-of select="name()"/>
	  </xsl:variable>
	  <xsl:attribute name='name'>
	    <xsl:value-of select="$group"/>
	    <xsl:if test="name() = 'screen'">
	      <xsl:text>$(screen)</xsl:text>
	    </xsl:if>
	  </xsl:attribute>
	  <xsl:for-each select="option[not(@read_only='true') and not(@type='action')]">
	    <xsl:call-template name="print_option">
              <xsl:with-param name="group" select="$group"/>
	    </xsl:call-template>
	  </xsl:for-each>
	</group>
      </xsl:for-each>
    </kcfg>
  </xsl:template>

  <xsl:template name="print_option">
    <xsl:param name="group"/>
    <entry>
      <xsl:variable name="ktype">
	<xsl:call-template name="print_type"/>
      </xsl:variable>
      <xsl:attribute name='name'>
	<xsl:value-of select="$group"/>
	<xsl:text>_</xsl:text>
	<xsl:value-of select="@name"/>
      </xsl:attribute>
      <xsl:attribute name='key'>
	<xsl:value-of select="@name"/>
      </xsl:attribute>
      <xsl:attribute name='type'>
	<xsl:value-of select="$ktype"/>
      </xsl:attribute>
      <label><xsl:value-of select="short[not(@xml:lang)]/text()"/></label>
      <whatsthis>
	<xsl:value-of select="long[not(@xml:lang)]/text()"/>
	<xsl:if test="$ktype = 'Int'">
          <xsl:call-template name="print_info"/>
        </xsl:if>
      </whatsthis>
      <xsl:choose>
	<xsl:when test="@type = 'color'">
          <default>
            <xsl:choose>
              <xsl:when test="default">
                <xsl:for-each select="default[1]">
                  <xsl:call-template name="print_color"/>
                </xsl:for-each>
              </xsl:when>
              <xsl:otherwise>
                <xsl:text>#000000ff</xsl:text>
              </xsl:otherwise>
            </xsl:choose>
	  </default>
        </xsl:when>
        <xsl:when test="@type = 'edge'">
          <default>
	    <xsl:for-each select="default[1]">
	      <xsl:call-template name="print_edge_list"/>
	    </xsl:for-each>
          </default>
        </xsl:when>
	<xsl:when test="$ktype = 'IntList' or $ktype = 'StringList'">
          <default>
            <xsl:choose>
              <xsl:when test="@type = 'color'">
		<xsl:for-each select="default[1]">
                  <xsl:call-template name="print_color_list"/>
		</xsl:for-each>
              </xsl:when>
              <xsl:otherwise>
		<xsl:for-each select="default[1]">
		  <xsl:call-template name="print_value_list"/>
		</xsl:for-each>
              </xsl:otherwise>
            </xsl:choose>
	  </default>
        </xsl:when>
	<xsl:otherwise>
          <default>
            <xsl:if test="default/text()">
              <xsl:value-of select="default/text()"/>
            </xsl:if>
          </default>
	</xsl:otherwise>
      </xsl:choose>
      <xsl:if test="contains('Int,Double', $ktype)">
	<xsl:if test="min/text()">
          <min><xsl:value-of select="min/text()"/></min>
	</xsl:if>
	<xsl:if test="max/text()">
          <max><xsl:value-of select="max/text()"/></max>
	</xsl:if>
      </xsl:if>
    </entry>
  </xsl:template>

  <xsl:template name="print_type">
    <xsl:param name="type">
      <xsl:value-of select="@type"/>
    </xsl:param>
    <xsl:choose>
      <xsl:when test="$type = 'bool'">
        <xsl:text>Bool</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'int'">
        <xsl:text>Int</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'float'">
        <xsl:text>Double</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'bell'">
        <xsl:text>Bool</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'list'">
	<xsl:choose>
	  <xsl:when test="type/text() = 'int'">
            <xsl:text>Int</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
            <xsl:text>String</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>List</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>String</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="print_info">
    <xsl:variable name="info">
      <xsl:text> (</xsl:text>
      <xsl:choose>
        <xsl:when test="contains('int,float',@type) and not(desc/value/text())">
          <xsl:value-of select="min/text()"/> - <xsl:value-of select="max/text()"/>
        </xsl:when>
	<xsl:when test="@type='int' and desc/value/text()">
          <xsl:call-template name="print_int_desc_list"/>
        </xsl:when>
        <xsl:when test="@type = 'match'">
          <xsl:text>match</xsl:text>
        </xsl:when>
      </xsl:choose>
      <xsl:text>)</xsl:text>
    </xsl:variable>
    <xsl:if test="not(contains($info,' ()'))">
      <xsl:value-of select="$info"/>
    </xsl:if>
  </xsl:template>

  <xsl:template name="print_int_desc_list">
    <xsl:variable name="list">
      <xsl:for-each select="desc">
          <xsl:value-of select="value/text()"/>
	  <xsl:text> = </xsl:text>
	  <xsl:value-of select="name/text()"/>
          <xsl:text>, </xsl:text>
      </xsl:for-each>
    </xsl:variable>
    <xsl:value-of select="substring($list,1,string-length($list) - 2)"/>
  </xsl:template>

  <xsl:template name="print_value_list">
    <xsl:variable name="list">
      <xsl:for-each select="value">
        <xsl:value-of select="text()"/>
        <xsl:text>,</xsl:text>
      </xsl:for-each>
    </xsl:variable>
    <xsl:value-of select="substring($list,1,string-length($list) - 1)"/>
  </xsl:template>

  <xsl:template name="print_color_list">
    <xsl:variable name="list">
      <xsl:for-each select="value">
        <xsl:call-template name="print_color"/>
        <xsl:text>,</xsl:text>
      </xsl:for-each>
    </xsl:variable>
    <xsl:value-of select="substring($list,1,string-length($list) - 1)"/>
  </xsl:template>

  <!--
    generates the #00aabbcc color value out of the compiz
    metadata color description
  -->
  <xsl:template name="print_color">
    <xsl:variable name="red">
      <xsl:call-template name="get_hex_num">
        <xsl:with-param name="value" select="red/text()"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="green">
      <xsl:call-template name="get_hex_num">
        <xsl:with-param name="value" select="green/text()"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="blue">
      <xsl:call-template name="get_hex_num">
        <xsl:with-param name="value" select="blue/text()"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="alpha">
      <xsl:choose>
        <xsl:when test="alpha/text()">
	  <xsl:call-template name="get_hex_num">
            <xsl:with-param name="value" select="alpha/text()"/>
	  </xsl:call-template>
        </xsl:when>
	<xsl:otherwise>
          <xsl:text>ff</xsl:text>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:value-of select="concat('#',concat($red,concat($green,concat($blue,$alpha))))"/>
  </xsl:template>

  <!--
    converts a decimal number in the range of 0-65535 or
    a hex number in the range of 0x0000 - 0xffff to a hex number in the
    range of 00 - ff
  -->
  <xsl:template name="get_hex_num">
    <xsl:param name="value"/>
    <xsl:choose>
      <xsl:when test="starts-with($value,'0x')">
        <xsl:variable name="number">
          <xsl:text>0000</xsl:text>
          <xsl:value-of select="substring-after($value,'0x')"/>
        </xsl:variable>
        <xsl:value-of select="substring(concat('000',$number),string-length($number),2)"/>
      </xsl:when>
      <xsl:when test="string-length($value)">
        <xsl:variable name="number">
          <xsl:call-template name="to_hex">
            <xsl:with-param name="decimal_number" select="$value"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="substring(concat('000',$number),string-length($number),2)"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>00</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- converts a decimal number to a hex number -->
  <xsl:variable name="hex_digits" select="'0123456789abcdef'"/>

  <xsl:template name="to_hex">
    <xsl:param name="decimal_number"/>
    <xsl:if test="$decimal_number >= 16">
      <xsl:call-template name="to_hex">
        <xsl:with-param name="decimal_number" select="floor($decimal_number div 16)" />
      </xsl:call-template>
    </xsl:if>
    <xsl:value-of select="substring($hex_digits, ($decimal_number mod 16) + 1, 1)" />
  </xsl:template>

  <xsl:template name="print_edge_list">
    <xsl:variable name="list">
      <xsl:for-each select="edge">
        <xsl:value-of select="@name"/>
        <xsl:text> | </xsl:text>
      </xsl:for-each>
    </xsl:variable>
    <xsl:value-of select="substring($list,1,string-length($list) - 3)"/>
  </xsl:template>

</xsl:stylesheet>
