<?xml version='1.0'?>
 
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		xmlns:exsl="http://exslt.org/common"
		xmlns:perl="urn:perl"
                xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0"
		version="1.0"
		exclude-result-prefixes="l exsl perl">

<!-- titles after all elements -->
<xsl:param name="formal.title.placement">
figure after
example before
equation before
table before
procedure before 
</xsl:param>

<xsl:param name="prod.url" select="'https://sourceware.org/systemtap/'"/>
<xsl:param name="doc.url" select="'https://sourceware.org/systemtap/'"/>

<xsl:param name="generate.section.toc.level" select="0"/>
<xsl:param name="qanda.defaultlabel">qanda</xsl:param>
<xsl:param name="glossary.sort" select="1"/>
<xsl:param name="formal.object.break.after">0</xsl:param>

<xsl:template name="user.preroot">
  <!-- Pre-root output, can be used to output comments and PIs. -->
  <!-- This must not output any element content! -->
</xsl:template>
<!--
Copied from fo/params.xsl
-->
<xsl:param name="l10n.gentext.default.language" select="'en'"/>
<xsl:param name="book.type" select="'book'"/>
<xsl:param name="web.type" select="''"/>
<xsl:param name="show.comments">0</xsl:param>
<xsl:param name="confidential" select="0"/>
<xsl:param name="confidential.text">CONFIDENTIAL</xsl:param>

<!-- This sets the filename based on the ID. -->
<xsl:param name="use.id.as.filename" select="'1'"/>

<xsl:param name="embedtoc"  select="'0'"/>
<xsl:param name="tocpath"   select="''"/>
<xsl:param name="pop_prod"  select="''"/>
<xsl:param name="pop_ver"   select="''"/>
<xsl:param name="pop_name"  select="''"/>
<xsl:param name="brand"     select="''"/>
<xsl:param name="langpath"  select="''"/>

<xsl:param name="desktop" select="0"/>
<xsl:param name="draft.mode">maybe</xsl:param>

<xsl:param name="funcsynopsis.style">ansi</xsl:param>
<xsl:param name="refentry.pagebreak">0</xsl:param>

<xsl:param name="svg.object">1</xsl:param>

<xsl:template match="command">
	<xsl:call-template name="inline.monoseq"/>
</xsl:template>

<xsl:template match="application">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guibutton">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guiicon">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guilabel">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guimenu">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guimenuitem">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guisubmenu">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="filename">
	<xsl:call-template name="inline.monoseq"/>
</xsl:template>

<xsl:template name="language.to.xslthl">
  <xsl:param name="context"/>

  <xsl:choose>
    <xsl:when test="$context/@language != ''">
      <xsl:value-of select="$context/@language"/>
    </xsl:when>
    <xsl:when test="$highlight.default.language != ''">
      <xsl:value-of select="$highlight.default.language"/>
    </xsl:when>
  </xsl:choose>
</xsl:template>

<xsl:template name="apply-highlighting">
  <xsl:choose>
    <!-- Do we want syntax highlighting -->
    <xsl:when test="$highlight.source != 0 and function-available('perl:highlight')">
      <xsl:variable name="language">
	<xsl:call-template name="language.to.xslthl">
	  <xsl:with-param name="context" select="."/>
	</xsl:call-template>
      </xsl:variable>
      <xsl:choose>
	<xsl:when test="$language != ''">
	  <xsl:variable name="content">
	    <xsl:apply-templates/>
	  </xsl:variable>
	  <xsl:apply-templates select="perl:highlight($language, exsl:node-set($content))"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <!-- No syntax highlighting -->
    <xsl:otherwise>
      <xsl:apply-templates/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="l10n.language">
  <xsl:param name="target" select="."/>
  <xsl:param name="xref-context" select="false()"/>

  <xsl:variable name="mc-language">
    <xsl:choose>
      <xsl:when test="$l10n.gentext.language != ''">
        <xsl:value-of select="$l10n.gentext.language"/>
      </xsl:when>

      <xsl:when test="$xref-context or $l10n.gentext.use.xref.language != 0">
        <!-- can't do this one step: attributes are unordered! -->
        <xsl:variable name="lang-scope"
                      select="$target/ancestor-or-self::*
                              [@lang or @xml:lang][1]"/>
        <xsl:variable name="lang-attr"
                      select="($lang-scope/@lang | $lang-scope/@xml:lang)[1]"/>
        <xsl:choose>
          <xsl:when test="string($lang-attr) = ''">
            <xsl:value-of select="$l10n.gentext.default.language"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$lang-attr"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>

      <xsl:otherwise>
        <!-- can't do this one step: attributes are unordered! -->
        <xsl:variable name="lang-scope"
                      select="$target/ancestor-or-self::*
                              [@lang or @xml:lang][1]"/>
        <xsl:variable name="lang-attr"
                      select="($lang-scope/@lang | $lang-scope/@xml:lang)[1]"/>

        <xsl:choose>
          <xsl:when test="string($lang-attr) = ''">
            <xsl:value-of select="$l10n.gentext.default.language"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$lang-attr"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="language" select="translate($mc-language,
                                        'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
                                        'abcdefghijklmnopqrstuvwxyz')"/>

<!-- sr-Latn-SR Need to remove contry code to get match -->
  <xsl:variable name="adjusted.language">
    <xsl:choose>
      <xsl:when test="contains($language,'-')">
        <xsl:variable name="start"><xsl:value-of select="substring-before($language,'-')"/></xsl:variable>
        <xsl:variable name="end"><xsl:value-of select="substring-after($language,'-')"/></xsl:variable>
        <xsl:choose>
           <xsl:when test="contains($end,'-')">
              <xsl:value-of select="$start"/>
              <xsl:text>_</xsl:text>
              <xsl:value-of select="substring-before($end,'-')"/>
           </xsl:when>
           <xsl:otherwise>
              <xsl:value-of select="$start"/>
              <xsl:text>_</xsl:text>
              <xsl:value-of select="$end"/>
           </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$language"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="$l10n.xml/l:i18n/l:l10n[@language=$adjusted.language]">
      <xsl:value-of select="$adjusted.language"/>
    </xsl:when>
    <!-- try just the lang code without country -->
    <xsl:when test="$l10n.xml/l:i18n/l:l10n[@language=substring-before($adjusted.language,'_')]">
      <xsl:value-of select="substring-before($adjusted.language,'_')"/>
    </xsl:when>
    <!-- or use the default -->
    <xsl:otherwise>
      <xsl:message>
        <xsl:text>No DocBook localization exists for "</xsl:text>
        <xsl:value-of select="$adjusted.language"/>
        <xsl:text>" or "</xsl:text>
        <xsl:value-of select="substring-before($adjusted.language,'_')"/>
        <xsl:text>". Using default "</xsl:text>
        <xsl:value-of select="$l10n.gentext.default.language"/>
        <xsl:text>".</xsl:text>
      </xsl:message>
      <xsl:value-of select="$l10n.gentext.default.language"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>


</xsl:stylesheet>


