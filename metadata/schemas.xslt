<!--
  Copyright © 2007 Dennis Kasprzyk

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
-->

<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform' >
  <xsl:output method="xml" indent="yes"/>

  <xsl:param name="appName">/apps/compiz</xsl:param>

  <xsl:template  match="/compiz">
    <gconfschemafile>
      <schemalist>
        <xsl:for-each select="/compiz//option[not(@read_only='true') and not(@type='action')]">
          <xsl:call-template name="dumpOption"/>
        </xsl:for-each>
      </schemalist>
    </gconfschemafile>
  </xsl:template>

  <!-- generates the schema for an option -->
  <xsl:template name="dumpOption">
    <schema>
      <key>/schemas<xsl:call-template name="printKey"/></key>
      <applyto><xsl:call-template name="printKey"/></applyto>
      <owner>compiz</owner>
      <type><xsl:call-template name="printType"/></type>
      <xsl:choose>
        <!-- color values need a special handling -->
        <xsl:when test="@type = 'color'">
          <default>
            <xsl:choose>
              <xsl:when test="default">
                <xsl:for-each select="default[1]">
                  <xsl:call-template name="printColor"/>
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
            <xsl:call-template name="printEdgeList"/>
          </default>
        </xsl:when>
        <xsl:when test="@type = 'list'">
          <list_type>
            <xsl:call-template name="printType">
              <xsl:with-param name="type" select="type/text()"/>
            </xsl:call-template>
          </list_type>
          <xsl:choose>
            <xsl:when test="type/text() = 'color'">
              <default>[<xsl:call-template name="printColorList"/>]</default>
            </xsl:when>
            <xsl:when test="@name = 'active_plugins'">
              <default>[<xsl:value-of select="$defaultPlugins"/>]</default>
	    </xsl:when>
            <xsl:otherwise>
              <default>[<xsl:call-template name="printValueList"/>]</default>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:when>
        <!-- for most option types we can use the default value directly -->
        <xsl:otherwise>
          <default>
            <xsl:choose>
              <xsl:when test="default/text()">
                <xsl:value-of select="default/text()"/>
              </xsl:when>
              <xsl:otherwise>
                <!-- if no default value was specified we need to generate one -->
                <xsl:choose>
		  <xsl:when test="contains('bool,bell',@type)">
                    <xsl:text>false</xsl:text>
                  </xsl:when>
                  <xsl:when test="@type = 'int'">
                    <xsl:variable name="num">
                      <xsl:call-template name="printNumFallback"/>
                    </xsl:variable>
                    <xsl:value-of select="floor($num)"/>
                  </xsl:when>
                  <xsl:when test="@type = 'float'">
                    <xsl:call-template name="printNumFallback"/>
                  </xsl:when>
		  <xsl:when test="contains('key,button',@type)">
		    <xsl:text>Disabled</xsl:text>
                  </xsl:when>
                </xsl:choose>
              </xsl:otherwise>
            </xsl:choose>
          </default>
	</xsl:otherwise>
      </xsl:choose>
      <!-- add the short and long descriptions -->
      <xsl:call-template name="printDescription"/>
    </schema>
  </xsl:template>

  <!-- converts a compiz type to a gconf type -->
  <xsl:template name="printType">
    <xsl:param name="type">
      <xsl:value-of select="@type"/>
    </xsl:param>
    <xsl:choose>
      <xsl:when test="$type = 'int'">
        <xsl:text>int</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'float'">
        <xsl:text>float</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'bool'">
        <xsl:text>bool</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'list'">
        <xsl:text>list</xsl:text>
      </xsl:when>
      <xsl:when test="$type = 'bell'">
        <xsl:text>bool</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>string</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- prints the option long and short descriptions of an option -->
  <xsl:template name="printDescription">
    <xsl:param name="info">
      <xsl:call-template name="printInfo"/>
    </xsl:param>
    <locale name="C">
      <short><xsl:value-of select="short[not(@xml:lang)]/text()"/></short>
      <long>
        <xsl:value-of select="long[not(@xml:lang)]/text()"/>
        <xsl:value-of select="$info"/>
      </long>
    </locale>
    <xsl:for-each select="short[@xml:lang]">
      <xsl:variable name="language" select="@xml:lang"/>
      <xsl:variable name="infoTrans">
        <xsl:call-template name="printInfoTrans">
          <xsl:with-param name="language" select="$language"/>
        </xsl:call-template>
      </xsl:variable>
      <locale>
        <xsl:attribute name='name'>
          <xsl:value-of select="@xml:lang"/>
        </xsl:attribute>
        <short><xsl:value-of select="text()"/></short>
        <long>
          <xsl:choose>
            <xsl:when test="parent::option/long[lang($language)]">
              <xsl:value-of select="parent::option/long[lang($language)]/text()"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="parent::option/long[not(@xml:lang)]/text()"/>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:value-of select="$infoTrans"/>
        </long>
      </locale>
    </xsl:for-each>
  </xsl:template>

  <!-- generates the additional info for the long option description -->
  <xsl:template name="printInfo">
    <xsl:variable name="info">
      <xsl:text> (</xsl:text>
      <xsl:choose>
        <xsl:when test="contains('int,float',@type) and not(desc/value/text())">
          <xsl:value-of select="min/text()"/> - <xsl:value-of select="max/text()"/>
        </xsl:when>
	<xsl:when test="@type='int' and desc/value/text()">
          <xsl:call-template name="printIntDescList"/>
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

  <!-- generates a list of int descriptions -->
  <xsl:template name="printIntDescList">
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

   <!-- generates the additional info for the long option description -->
  <xsl:template name="printInfoTrans">
    <xsl:param name="language"/>
    <xsl:variable name="info">
      <xsl:text> (</xsl:text>
      <xsl:choose>
        <xsl:when test="contains('int,float',parent::option/@type) and not(parent::option/desc/value/text())">
          <xsl:value-of select="parent::option/min/text()"/> - <xsl:value-of select="parent::option/max/text()"/>
        </xsl:when>
	<xsl:when test="parent::option/@type='int' and parent::option/desc/value/text()">
          <xsl:call-template name="printIntDescListTrans">
            <xsl:with-param name="language" select="$language"/>
          </xsl:call-template>
        </xsl:when>
        <xsl:when test="parent::option/@type = 'match'">
          <xsl:text>match</xsl:text>
        </xsl:when>
      </xsl:choose>
      <xsl:text>)</xsl:text>
    </xsl:variable>
    <xsl:if test="not(contains($info,' ()'))">
      <xsl:value-of select="$info"/>
    </xsl:if>
  </xsl:template>
  
  <!-- generates a list of int descriptions -->
  <xsl:template name="printIntDescListTrans">
    <xsl:param name="language"/>
    <xsl:variable name="list">
      <xsl:for-each select="parent::option/desc">
          <xsl:value-of select="value/text()"/>
          <xsl:text> = </xsl:text>
          <xsl:choose>
            <xsl:when test="name[lang($language)]/text()">
              <xsl:value-of select="name[lang($language)]/text()"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="name[not(@xml:lang)]/text()"/>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:text>, </xsl:text>
      </xsl:for-each>
    </xsl:variable>
    <xsl:value-of select="substring($list,1,string-length($list) - 2)"/>
  </xsl:template>

  <!-- generates a default number out of the min and max values -->
  <xsl:template name="printNumFallback">
    <xsl:choose>
      <xsl:when test="max/text() and min/text()">
        <xsl:value-of select="(max/text() + min/text()) div 2"/>
      </xsl:when>
      <xsl:when test="max/text() and not(min/text())">
        <xsl:value-of select="max/text()"/>
      </xsl:when>
      <xsl:when test="not(max/text()) and min/text()">
        <xsl:value-of select="min/text()"/>
      </xsl:when>
      <xsl:otherwise>0</xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- generates a list of values -->
  <xsl:template name="printValueList">
    <xsl:variable name="list">
      <xsl:for-each select="default/value">
        <xsl:value-of select="text()"/>
        <xsl:text>,</xsl:text>
      </xsl:for-each>
    </xsl:variable>
    <xsl:value-of select="substring($list,1,string-length($list) - 1)"/>
  </xsl:template>

  <!-- generates a list of color string values -->
  <xsl:template name="printColorList">
    <xsl:variable name="list">
      <xsl:for-each select="default/value">
        <xsl:call-template name="printColor"/>
        <xsl:text>,</xsl:text>
      </xsl:for-each>
    </xsl:variable>
    <xsl:value-of select="substring($list,1,string-length($list) - 1)"/>
  </xsl:template>

  <!--
    generates the #00aabbcc color value out of the compiz
    metadata color description
  -->
  <xsl:template name="printColor">
    <xsl:variable name="red">
      <xsl:call-template name="getHexNum">
        <xsl:with-param name="value" select="red/text()"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="green">
      <xsl:call-template name="getHexNum">
        <xsl:with-param name="value" select="green/text()"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="blue">
      <xsl:call-template name="getHexNum">
        <xsl:with-param name="value" select="blue/text()"/>
      </xsl:call-template>
    </xsl:variable>
    <xsl:variable name="alpha">
      <xsl:choose>
        <xsl:when test="alpha/text()">
	  <xsl:call-template name="getHexNum">
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
  <xsl:template name="getHexNum">
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
          <xsl:call-template name="toHex">
            <xsl:with-param name="decimalNumber" select="$value"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="substring(concat('000',$number),string-length($number),2)"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>00</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- generates a list of selected edges -->
  <xsl:template name="printEdgeList">
    <xsl:variable name="list">
      <xsl:for-each select="default/edge">
        <xsl:value-of select="@name"/>
        <xsl:text> | </xsl:text>
      </xsl:for-each>
    </xsl:variable>
    <xsl:value-of select="substring($list,1,string-length($list) - 3)"/>
  </xsl:template>

  <!-- prints the key path for an option -->
  <xsl:template name="printKey">
    <xsl:value-of select="$appName"/>
    <xsl:choose>
      <xsl:when test="ancestor::plugin">
        <xsl:text>/plugins/</xsl:text>
        <xsl:value-of select="ancestor::plugin/@name"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>/general</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:choose>
      <xsl:when test="ancestor::screen">
        <xsl:text>/screen0/options/</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>/allscreens/options/</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:value-of select="@name"/>
  </xsl:template>

  <!-- converts a decimal number to a hex number -->
  <xsl:variable name="hexDigits" select="'0123456789abcdef'" />

  <xsl:template name="toHex">
    <xsl:param name="decimalNumber" />
    <xsl:if test="$decimalNumber >= 16">
      <xsl:call-template name="toHex">
        <xsl:with-param name="decimalNumber" select="floor($decimalNumber div 16)" />
      </xsl:call-template>
    </xsl:if>
    <xsl:value-of select="substring($hexDigits, ($decimalNumber mod 16) + 1, 1)" />
  </xsl:template>

</xsl:stylesheet>
