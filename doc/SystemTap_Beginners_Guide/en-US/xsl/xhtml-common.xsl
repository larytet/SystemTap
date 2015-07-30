<?xml version='1.0'?>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml"
		xmlns:exsl="http://exslt.org/common"
		xmlns:xtext="xalan://com.nwalsh.xalan.Text"
		xmlns:xlink="http://www.w3.org/1999/xlink"
		xmlns:stext="http://nwalsh.com/xslt/ext/com.nwalsh.saxon.TextFactory"
		xmlns:simg="http://nwalsh.com/xslt/ext/com.nwalsh.saxon.ImageIntrinsics" 
		xmlns:ximg="xalan://com.nwalsh.xalan.ImageIntrinsics" 
                xmlns:stbl="http://nwalsh.com/xslt/ext/com.nwalsh.saxon.Table"
                xmlns:xtbl="com.nwalsh.xalan.Table"
                xmlns:ptbl="http://nwalsh.com/xslt/ext/xsltproc/python/Table"
		xmlns:perl="urn:perl"
		xmlns:sverb="http://nwalsh.com/xslt/ext/com.nwalsh.saxon.Verbatim"
		xmlns:xverb="xalan://com.nwalsh.xalan.Verbatim"
		xmlns:d="http://docbook.org/ns/docbook"
		version="1.0"
		exclude-result-prefixes="sverb xverb xlink exsl stext xtext simg ximg"
		extension-element-prefixes="stext xtext perl ptbl xtbl stbl"
>

<!-- Admonition Graphics -->
<xsl:param name="admon.graphics" select="1"/>
<xsl:param name="callouts.extension" select="1"/>
<xsl:param name="admon.style" select="''"/>
<xsl:param name="admon.graphics.path">
    <xsl:choose>
      <xsl:when test="$embedtoc != 0">
        <xsl:value-of select="concat($tocpath, '/../', $brand, '/', $langpath, '/images/')"/>
      </xsl:when>
      <xsl:otherwise>
		<xsl:text>Common_Content/images/</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
</xsl:param>
<xsl:param name="callout.graphics.path"><xsl:value-of select="$admon.graphics.path"/></xsl:param>

<xsl:param name="package" select="''"/>

<xsl:param name="ignore.image.scaling" select="0"/>

<xsl:param name="chunker.output.doctype-public" select="'-//W3C//DTD XHTML 1.0 Strict//EN'"/>
<xsl:param name="chunker.output.doctype-system" select="'http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd'"/>
<xsl:param name="chunker.output.encoding" select="'UTF-8'"/>
<xsl:param name="chunker.output.indent" select="'no'"/>
<xsl:param name="html.longdesc.link" select="0"/>
<xsl:param name="html.longdesc" select="0"/>
<xsl:param name="html.stylesheet"><xsl:if test="$embedtoc = 0 ">Common_Content/css/default.css</xsl:if></xsl:param>
<xsl:param name="html.stylesheet.type" select="'text/css'"/>
<xsl:param name="html.stylesheet.print"><xsl:if test="$embedtoc = 0 ">Common_Content/css/print.css</xsl:if></xsl:param>
<xsl:param name="html.cleanup" select="0"/>
<xsl:param name="html.ext" select="'.html'"/>
<xsl:output method="xml" indent="yes"/>
<xsl:param name="highlight.source" select="1"/>
<xsl:param name="use.extensions" select="1"/>
<xsl:param name="tablecolumns.extension" select="1"/>

<xsl:param name="qanda.in.toc" select="0"/>
<xsl:param name="segmentedlist.as.table" select="1"/>
<xsl:param name="othercredit.like.author.enabled" select="0"/>
<xsl:param name="email.delimiters.enabled">0</xsl:param>

<xsl:param name="generate.id.attributes" select="0"/>
<xsl:param name="make.graphic.viewport" select="0"/>
<xsl:param name="use.embed.for.svg" select="0"/>

<!-- TOC -->
<xsl:param name="section.autolabel" select="1"/>
<xsl:param name="section.label.includes.component.label" select="1"/>

<xsl:param name="generate.toc">
set toc
book toc
article nop
chapter toc
qandadiv toc
qandaset toc
sect1 nop
sect2 nop
sect3 nop
sect4 nop
sect5 nop
section toc
part toc
</xsl:param>

<xsl:param name="suppress.navigation" select="0"/>
<xsl:param name="suppress.header.navigation" select="0"/>

<xsl:param name="header.rule" select="0"/>
<xsl:param name="footer.rule" select="0"/>
<xsl:param name="css.decoration" select="0"/>
<xsl:param name="ulink.target"/>
<xsl:param name="table.cell.border.style"/>

<!-- BUGBUG 

	There is a bug where inserting elements in to the body level
	of xhtml will add xmlns="" to the tag. This is invalid xhtml.
	To overcome this I added:
		xmlns="http://www.w3.org/1999/xhtml"
	to the outer most tag. This gets stripped by the parser, resulting
	in valid xhtml ... go figure.
-->

<!--
From: xhtml/admon.xsl
Reason: remove tables
Version: 1.72.0
-->
<xsl:template name="graphical.admonition">
	<xsl:variable name="admon.type">
		<xsl:choose>
			<xsl:when test="local-name(.)='note'">Note</xsl:when>
			<xsl:when test="local-name(.)='warning'">Warning</xsl:when>
			<xsl:when test="local-name(.)='important'">Important</xsl:when>
			<xsl:otherwise>Note</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>

	<xsl:variable name="alt">
		<xsl:call-template name="gentext">
			<xsl:with-param name="key" select="$admon.type"/>
		</xsl:call-template>
	</xsl:variable>

	<div xmlns="http://www.w3.org/1999/xhtml">
		<xsl:apply-templates select="." mode="class.attribute"/>
		<xsl:if test="$admon.style != ''">
			<xsl:attribute name="style">
				<xsl:value-of select="$admon.style"/>
			</xsl:attribute>
		</xsl:if>
		<xsl:if test="@id or @xml:id">
			<xsl:attribute name="id">
				<xsl:value-of select="(@id|@xml:id)[1]"/>
			</xsl:attribute>
		</xsl:if>
                <xsl:call-template name="common.html.attributes"/>
		<xsl:if test="$admon.textlabel != 0 or title">
			<div class="admonition_header">
				<p>
					<strong><xsl:apply-templates select="." mode="object.title.markup"/></strong>
				</p>
			</div>
		</xsl:if>
		<div class="admonition">
			<xsl:apply-templates/>
		</div>
	</div>
</xsl:template>

<!--
From: xhtml/lists.xsl
Reason: Remove invalid type attribute from ol
Version: 1.72.0
-->
<xsl:template match="substeps">
	<xsl:variable name="numeration">
		<xsl:call-template name="procedure.step.numeration"/>
	</xsl:variable>
	<ol xmlns="http://www.w3.org/1999/xhtml" class="substeps {$numeration}">
		<xsl:call-template name="anchor"/>
		<xsl:apply-templates/>
	</ol>
</xsl:template>

<!--
From: xhtml/lists.xsl
Reason: Remove invalid type, start & compact attributes from ol, use role as class
Version: 1.72.0
-->
<xsl:template match="orderedlist">
	<div xmlns="http://www.w3.org/1999/xhtml">
		<xsl:call-template name="common.html.attributes"/>
		<xsl:apply-templates select="." mode="class.attribute"/>
		<xsl:call-template name="anchor"/>
		<xsl:if test="title">
			<xsl:call-template name="formal.object.heading"/>
		</xsl:if>
<!-- Preserve order of PIs and comments -->
		<xsl:apply-templates select="*[not(self::listitem or self::title or self::titleabbrev)] |
                                      comment()[not(preceding-sibling::listitem)] |
									  processing-instruction()[not(preceding-sibling::listitem)]"/>
		<ol>
          <xsl:if test="@role">
            <xsl:apply-templates select="." mode="class.attribute">
              <xsl:with-param name="class" select="@role"/>
            </xsl:apply-templates>
          </xsl:if>
          <xsl:if test="@numeration">
            <xsl:apply-templates select="." mode="class.attribute">
              <xsl:with-param name="class" select="@numeration"/>
            </xsl:apply-templates>
          </xsl:if>
          <xsl:apply-templates select="listitem | comment()[preceding-sibling::listitem] | processing-instruction()[preceding-sibling::listitem]"/>
		</ol>
	</div>
</xsl:template>

<!--
From: xhtml/lists.xsl
Reason: Remove invalid type, start & compact attributes from ol
Version: 1.72.0
-->
<xsl:template match="procedure">
	<xsl:variable name="param.placement" select="substring-after(normalize-space($formal.title.placement), concat(local-name(.), ' '))"/>

	<xsl:variable name="placement">
		<xsl:choose>
			<xsl:when test="contains($param.placement, ' ')">
				<xsl:value-of select="substring-before($param.placement, ' ')"/>
			</xsl:when>
			<xsl:when test="$param.placement = ''">before</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$param.placement"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>

<!-- Preserve order of PIs and comments -->
	<xsl:variable name="preamble" select="*[not(self::step or self::title or self::titleabbrev)] |comment()[not(preceding-sibling::step)]	|processing-instruction()[not(preceding-sibling::step)]"/>
	<div xmlns="http://www.w3.org/1999/xhtml">
		<xsl:apply-templates select="." mode="class.attribute"/>
		<xsl:call-template name="common.html.attributes"/>
		<xsl:call-template name="anchor">
			<xsl:with-param name="conditional">
				<xsl:choose>
					<xsl:when test="title">0</xsl:when>
					<xsl:otherwise>1</xsl:otherwise>
				</xsl:choose>
			</xsl:with-param>
		</xsl:call-template>
		<xsl:if test="title and $placement = 'before'">
			<xsl:call-template name="formal.object.heading"/>
		</xsl:if>
		<xsl:apply-templates select="$preamble"/>
		<xsl:choose>
			<xsl:when test="count(step) = 1">
				<ul>
					<xsl:apply-templates select="step |comment()[preceding-sibling::step] |processing-instruction()[preceding-sibling::step]"/>
				</ul>
			</xsl:when>
			<xsl:otherwise>
				<ol>
					<xsl:attribute name="class">
						<xsl:value-of select="substring($procedure.step.numeration.formats,1,1)"/>
					</xsl:attribute>
					<xsl:apply-templates select="step |comment()[preceding-sibling::step] |processing-instruction()[preceding-sibling::step]"/>
				</ol>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:if test="title and $placement != 'before'">
			<xsl:call-template name="formal.object.heading"/>
		</xsl:if>
	</div>
</xsl:template>

<!--
From: xhtml/graphics.xsl
Reason:  Remove html markup (align)
Version: 1.72.0
-->
<xsl:template name="longdesc.link">
	<xsl:param name="longdesc.uri" select="''"/>

	<xsl:variable name="this.uri">
	<xsl:call-template name="make-relative-filename">
		<xsl:with-param name="base.dir" select="$base.dir"/>
			<xsl:with-param name="base.name">
				<xsl:call-template name="href.target.uri"/>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:variable>
	<xsl:variable name="href.to">
		<xsl:call-template name="trim.common.uri.paths">
			<xsl:with-param name="uriA" select="$longdesc.uri"/>
			<xsl:with-param name="uriB" select="$this.uri"/>
			<xsl:with-param name="return" select="'A'"/>
		</xsl:call-template>
	</xsl:variable>
	<div xmlns="http://www.w3.org/1999/xhtml" class="longdesc-link">
		<br/>
		<span class="longdesc-link">
			<xsl:text>[</xsl:text>
			<a href="{$href.to}">D</a>
			<xsl:text>]</xsl:text>
		</span>
	</div>
</xsl:template>

<!--
From: xhtml/docbook.xsl
Reason: Remove inline style for draft mode
Version: 1.72.0
-->
<xsl:template name="head.content">
	<xsl:param name="node" select="."/>
	<xsl:param name="title">
		<xsl:apply-templates select="$node" mode="object.title.markup.textonly"/>
	</xsl:param>

	<title xmlns="http://www.w3.org/1999/xhtml" >
		<xsl:copy-of select="$title"/>
	</title>

	<xsl:if test="$html.stylesheet != ''">
		<xsl:call-template name="output.html.stylesheets">
			<xsl:with-param name="stylesheets" select="normalize-space($html.stylesheet)"/>
		</xsl:call-template>
	</xsl:if>
	<xsl:if test="$html.stylesheet.print != ''">
         <link rel="stylesheet" media="print">
            <xsl:attribute name="href">
              <xsl:value-of select="$html.stylesheet.print"/>
            </xsl:attribute>
            <xsl:if test="$html.stylesheet.type != ''">
              <xsl:attribute name="type">
                <xsl:value-of select="$html.stylesheet.type"/>
              </xsl:attribute>
           </xsl:if>
         </link>
        </xsl:if>

	<xsl:if test="$link.mailto.url != ''">
		<link rev="made" href="{$link.mailto.url}"/>
	</xsl:if>

	<xsl:if test="$html.base != ''">
		<base href="{$html.base}"/>
	</xsl:if>

	<meta xmlns="http://www.w3.org/1999/xhtml" name="generator">
		<xsl:attribute name="content">
			<xsl:text>not publican</xsl:text>
		</xsl:attribute>
	</meta>

	<meta xmlns="http://www.w3.org/1999/xhtml" name="package">
		<xsl:attribute name="content">
			<xsl:copy-of select="$package"/>
		</xsl:attribute>
	</meta>
	<xsl:if test="$generate.meta.abstract != 0">
		<xsl:variable name="info" select="(articleinfo |bookinfo |prefaceinfo |chapterinfo |appendixinfo |sectioninfo |sect1info |sect2info |sect3info |sect4info |sect5info |referenceinfo |refentryinfo |partinfo |info |docinfo)[1]"/>
		<xsl:if test="$info and $info/abstract">
			<meta xmlns="http://www.w3.org/1999/xhtml" name="description">
				<xsl:attribute name="content">
					<xsl:for-each select="$info/abstract[1]/*">
						<xsl:value-of select="normalize-space(.)"/>
						<xsl:if test="position() &lt; last()">
							<xsl:text> </xsl:text>
						</xsl:if>
					</xsl:for-each>
				</xsl:attribute>
			</meta>
		</xsl:if>
	</xsl:if>
      <xsl:if test="$embedtoc != 0 ">
          <link rel="stylesheet" type="text/css"><xsl:attribute name="href"><xsl:value-of select="$tocpath"/>/../chrome.css</xsl:attribute></link>
          <link rel="stylesheet" type="text/css"><xsl:attribute name="href"><xsl:value-of select="$tocpath"/>/../db4.css</xsl:attribute></link>
          <link rel="stylesheet" type="text/css"><xsl:attribute name="href"><xsl:value-of select="$tocpath"/>/../<xsl:value-of select="$brand"/>/<xsl:value-of select="$langpath"/>/css/brand.css</xsl:attribute></link>
          <link rel="stylesheet" type="text/css"><xsl:attribute name="href"><xsl:value-of select="$tocpath"/>/../print.css</xsl:attribute><xsl:attribute name="media">print</xsl:attribute></link>
          <script type="text/javascript"><xsl:attribute name="src"><xsl:value-of select="$tocpath"/>/../jquery-1.7.1.min.js</xsl:attribute></script>
          <script type="text/javascript"><xsl:attribute name="src"><xsl:value-of select="$tocpath"/>/labels.js</xsl:attribute></script>
          <script type="text/javascript"><xsl:attribute name="src"><xsl:value-of select="$tocpath"/>/../toc.js</xsl:attribute></script>
          <script type="text/javascript">
          <xsl:if test="$web.type = ''">
		current_book = '<xsl:copy-of select="$pop_name"/>';
		current_version = '<xsl:copy-of select="$pop_ver"/>';
		current_product = '<xsl:copy-of select="$pop_prod"/>';
	   </xsl:if>
          <xsl:if test="$web.type != ''">
		current_book = '';
		current_version = '';
	   </xsl:if>
                toc_path = '<xsl:value-of select="$tocpath"/>';
		loadMenu();
          </script>
      </xsl:if>

	<xsl:apply-templates select="." mode="head.keywords.content"/>
</xsl:template>

<!--
From: xhtml/docbook.xsl
Reason: Add css class for draft mode
Version: 1.72.0
-->
<xsl:template name="body.attributes">
  <!-- remove the if stmt checking writing.mode since it's not always defined, and RHEL5 doesn't like that. -->
	<xsl:variable name="class">
		<xsl:if test="($draft.mode = 'yes' or ($draft.mode = 'maybe' and (ancestor-or-self::set | ancestor-or-self::book | ancestor-or-self::article)[1]/@status = 'draft'))">
			<xsl:value-of select="ancestor-or-self::*[@status][1]/@status"/><xsl:text> </xsl:text>
		</xsl:if>
		<xsl:if test="$embedtoc != 0">
			<xsl:text>toc_embeded </xsl:text>
		</xsl:if>
       		<xsl:if test="$desktop != 0">
		  <xsl:text>desktop </xsl:text>
		</xsl:if>
	</xsl:variable>
        <xsl:if test="$class != ''">
	  <xsl:attribute name="class">
		<xsl:value-of select="$class"/>
	  </xsl:attribute>
	</xsl:if>
</xsl:template>

<!--
From: xhtml/docbook.xsl
Reason: Add confidential to footer
Version: 1.72.0
-->
<xsl:template name="user.footer.content">
	<xsl:param name="node" select="."/>
	<xsl:if test="$confidential = '1'">
		<div xmlns="http://www.w3.org/1999/xhtml" class="confidential">
			<xsl:value-of select="$confidential.text"/>
		</div>
	</xsl:if>
	<xsl:if test="$embedtoc != 0">
		<div id="site_footer"></div>
		<script type="text/javascript">
			$("#site_footer").load("<xsl:value-of select="$tocpath"/>/../footer.html");
		</script>
	</xsl:if>
</xsl:template>

<!--
From: xhtml/block.xsl
Reason:  default class (otherwise) to formalpara
Version: 1.72.0
-->
<xsl:template match="formalpara">
	<xsl:call-template name="paragraph">
		<xsl:with-param name="class">
			<xsl:choose>
				<xsl:when test="@role and $para.propagates.style != 0">
					<xsl:value-of select="@role"/>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>formalpara</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:with-param>
		<xsl:with-param name="content">
			<xsl:apply-templates/>
		</xsl:with-param>
	</xsl:call-template>
	<!--xsl:apply-templates/-->
</xsl:template>

<!--
From: xhtml/block.xsl
Reason:  h5 instead of <b>, remove default title end punctuation
Version: 1.72.0
-->
<xsl:template match="formalpara/title|formalpara/info/title">
	<xsl:variable name="titleStr">
			<xsl:apply-templates/>
	</xsl:variable>
	<div xmlns="http://www.w3.org/1999/xhtml" class="title">
		<xsl:copy-of select="$titleStr"/>
	</div>
</xsl:template>

<!--
From: xhtml/lists.xsl
Reason:  use role as class
Version: 1.72.0
-->
<xsl:template match="itemizedlist">
  <div xmlns="http://www.w3.org/1999/xhtml">
    <xsl:call-template name="common.html.attributes"/>
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:call-template name="anchor"/>
    <xsl:if test="title">
      <xsl:call-template name="formal.object.heading"/>
    </xsl:if>

    <!-- Preserve order of PIs and comments -->
    <xsl:apply-templates select="*[not(self::listitem or self::title or self::titleabbrev)] |
								  comment()[not(preceding-sibling::listitem)] |
								  processing-instruction()[not(preceding-sibling::listitem)]"/>
    <ul>
      <xsl:if test="@role">
        <xsl:apply-templates select="." mode="class.attribute">
          <xsl:with-param name="class" select="@role"/>
        </xsl:apply-templates>
      </xsl:if>
      <xsl:if test="$css.decoration != 0">
        <xsl:attribute name="type">
          <xsl:call-template name="list.itemsymbol"/>
        </xsl:attribute>
      </xsl:if>

      <xsl:apply-templates select="listitem | comment()[preceding-sibling::listitem] | processing-instruction()[preceding-sibling::listitem]"/>
    </ul>
  </div>
</xsl:template>

<!--
From: xhtml/xref.xsl
Reason:  use role as class
Version: 1.72.0
-->
<xsl:template match="ulink" name="ulink">
  <xsl:param name="url" select="@url"/>
  <xsl:variable name="link">
    <a xmlns="http://www.w3.org/1999/xhtml">
      <xsl:if test="@id or @xml:id">
        <xsl:attribute name="id">
          <xsl:value-of select="(@id|@xml:id)[1]"/>
        </xsl:attribute>
      </xsl:if>
      <xsl:attribute name="href"><xsl:value-of select="$url"/></xsl:attribute>
      <xsl:if test="$ulink.target != ''">
        <xsl:attribute name="target">
          <xsl:value-of select="$ulink.target"/>
        </xsl:attribute>
      </xsl:if>
      <xsl:if test="@role">
        <xsl:apply-templates select="." mode="class.attribute">
          <xsl:with-param name="class" select="@role"/>
        </xsl:apply-templates>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="count(child::node())=0">
          <xsl:value-of select="$url"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:apply-templates/>
        </xsl:otherwise>
      </xsl:choose>
    </a>
  </xsl:variable>
  <xsl:copy-of select="$link"/>
</xsl:template>

<xsl:template match="*" mode="class.attribute">
  <xsl:param name="class" select="local-name(.)"/>
  <!-- permit customization of class attributes -->
  <!-- Use element name by default -->
  <xsl:variable name="class.value">
    <xsl:apply-templates select="." mode="class.value">
      <xsl:with-param name="class" select="$class"/>
    </xsl:apply-templates>
    <xsl:if test="@role">
        <xsl:text> </xsl:text>
        <xsl:value-of select="@role"/>
    </xsl:if>
    <xsl:if test="@revisionflag and ($draft.mode = 'yes' or ($draft.mode = 'maybe' and (ancestor-or-self::set | ancestor-or-self::book | ancestor-or-self::article)[1]/@status = 'draft'))">
        <xsl:text> </xsl:text>
        <xsl:value-of select="@revisionflag"/>
    </xsl:if>
  </xsl:variable>

  <xsl:if test="string-length(normalize-space($class.value)) != 0">
    <xsl:attribute name="class">
      <xsl:value-of select="$class.value"/>
    </xsl:attribute>
  </xsl:if>
</xsl:template>

<!--
From: xhtml/qandaset.xsl
Reason: No stinking tables
Version: 1.72.0
-->
<xsl:template name="qandaset">
  <div class="qandaset">
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:apply-templates />
  </div>
</xsl:template>

<xsl:template name="process.qandaset">
  <div class="qandaset">
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:apply-templates />
  </div>
</xsl:template>

<xsl:template match="qandadiv">
  <xsl:variable name="preamble" select="*[local-name(.) != 'title'                                           and local-name(.) != 'titleabbrev'                                           and local-name(.) != 'qandadiv'                                           and local-name(.) != 'qandaentry']"/>

  <xsl:if test="blockinfo/title|info/title|title">
    <div class="qandadiv">
        <xsl:apply-templates select="(blockinfo/title|info/title|title)[1]"/>
    </div>
  </xsl:if>

  <xsl:variable name="toc">
    <xsl:call-template name="dbhtml-attribute">
      <xsl:with-param name="pis" select="processing-instruction('dbhtml')"/>
      <xsl:with-param name="attribute" select="'toc'"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="toc.params">
    <xsl:call-template name="find.path.params">
      <xsl:with-param name="table" select="normalize-space($generate.toc)"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:if test="(contains($toc.params, 'toc') and $toc != '0') or $toc = '1'">
    <div class="toc">
        <xsl:call-template name="process.qanda.toc"/>
    </div>
  </xsl:if>
  <xsl:if test="$preamble">
    <div class="preamble">
        <xsl:apply-templates select="$preamble"/>
    </div>
  </xsl:if>
  <div>
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:apply-templates select="qandadiv|qandaentry"/>
  </div>
</xsl:template>

<xsl:template match="qandaentry">
  <div>
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:apply-templates/>
  </div>
</xsl:template>

<xsl:template match="question">
  <xsl:variable name="deflabel">
    <xsl:choose>
      <xsl:when test="ancestor-or-self::*[@defaultlabel]">
        <xsl:value-of select="(ancestor-or-self::*[@defaultlabel])[last()] /@defaultlabel"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$qanda.defaultlabel"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:variable name="label.content">
    <xsl:apply-templates select="." mode="label.markup"/>
    <xsl:if test="$deflabel = 'number' and not(label)">
      <xsl:apply-templates select="." mode="intralabel.punctuation"/>
    </xsl:if>
  </xsl:variable>
  <div>
    <xsl:apply-templates select="." mode="class.attribute"/>
      <xsl:call-template name="anchor">
        <xsl:with-param name="node" select=".."/>
        <xsl:with-param name="conditional" select="0"/>
      </xsl:call-template>
      <!--xsl:call-template name="anchor">
        <xsl:with-param name="conditional" select="0"/>
      </xsl:call-template-->
      <xsl:if test="string-length($label.content) &gt; 0">
        <div class="label">
          <xsl:copy-of select="$label.content"/>
        </div>
      </xsl:if>
    <div class="data">
      <xsl:apply-templates/>
    </div>
  </div>
</xsl:template>

<xsl:template match="answer">
  <xsl:variable name="deflabel">
    <xsl:choose>
      <xsl:when test="ancestor-or-self::*[@defaultlabel]">
        <xsl:value-of select="(ancestor-or-self::*[@defaultlabel])[last()] /@defaultlabel"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$qanda.defaultlabel"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <div>
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:variable name="answer.label">
      <xsl:apply-templates select="." mode="label.markup"/>
    </xsl:variable>
    <xsl:if test="string-length($answer.label) &gt; 0">
      <div class="label">
        <xsl:copy-of select="$answer.label"/>
      </div>
    </xsl:if>
     <div class="data">
       <xsl:apply-templates />
     </div>
   </div>
</xsl:template>

<!--

BUGBUG callout code blows up highlight if the span contains a newline
because it has to parse lines one by one to place the gfx

-->
<xsl:template match="perl_Alert | perl_BaseN | perl_BString | perl_Char | perl_Comment | perl_DataType | perl_DecVal | perl_Error | perl_Float | perl_Function | perl_IString | perl_Keyword | perl_Operator | perl_Others | perl_RegionMarker | perl_Reserved | perl_String | perl_Variable | perl_Warning ">
  <xsl:variable name="name">
    <xsl:value-of select="local-name(.)"/>
  </xsl:variable>
  <xsl:variable name="content">
    <xsl:apply-templates/>
  </xsl:variable>
  <xsl:choose>
    <xsl:when test="contains($content,'&#xA;')">
      <span><xsl:attribute name="class"><xsl:value-of select="$name"/></xsl:attribute><xsl:value-of select="substring-before($content,'&#xA;')"/></span><xsl:text>
</xsl:text>
      <span><xsl:attribute name="class"><xsl:value-of select="$name"/></xsl:attribute><xsl:value-of select="substring-after($content,'&#xA;')"/></span>
    </xsl:when>
    <xsl:otherwise>
      <span><xsl:attribute name="class"><xsl:value-of select="$name"/></xsl:attribute><xsl:value-of select="$content"/></span>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="productnumber" mode="book.titlepage.recto.auto.mode">
<xsl:apply-templates select="." mode="book.titlepage.recto.mode"/>
</xsl:template>

<xsl:template match="productname" mode="titlepage.mode">
  <span>
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:apply-templates mode="titlepage.mode"/>
  </span>
</xsl:template>

<xsl:template match="productnumber" mode="titlepage.mode">
  <span>
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:apply-templates mode="titlepage.mode"/>
  </span>
</xsl:template>

<xsl:template match="productname" mode="book.titlepage.recto.auto.mode">
<xsl:apply-templates select="." mode="book.titlepage.recto.mode"/>
</xsl:template>

<xsl:template match="orgdiv" mode="titlepage.mode">
  <xsl:if test="preceding-sibling::*[1][self::orgname]">
    <xsl:text> </xsl:text>
  </xsl:if>
  <span>
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:apply-templates mode="titlepage.mode"/>
  </span>
</xsl:template>

<xsl:template match="orgname" mode="titlepage.mode">
  <span>
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:apply-templates mode="titlepage.mode"/>
  </span>
</xsl:template>

<xsl:template name="book.titlepage.recto">
  <xsl:choose>
    <xsl:when test="bookinfo/productname">
	<div class="producttitle" xsl:use-attribute-sets="book.titlepage.recto.style">
	  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/productname"/>
	  <xsl:text> </xsl:text>
	  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/productnumber"/>
	</div>
    </xsl:when>
  </xsl:choose>
  <xsl:choose>
    <xsl:when test="bookinfo/title">
      <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/title"/>
    </xsl:when>
    <xsl:when test="info/title">
      <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/title"/>
    </xsl:when>
    <xsl:when test="title">
      <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="title"/>
    </xsl:when>
  </xsl:choose>

  <xsl:choose>
    <xsl:when test="bookinfo/subtitle">
      <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/subtitle"/>
    </xsl:when>
    <xsl:when test="info/subtitle">
      <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/subtitle"/>
    </xsl:when>
    <xsl:when test="subtitle">
      <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="subtitle"/>
    </xsl:when>
  </xsl:choose>

  <xsl:apply-templates mode="titlepage.mode" select="bookinfo/edition"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/corpauthor"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/corpauthor"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/authorgroup"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/authorgroup"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/author"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/author"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/othercredit"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/othercredit"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/releaseinfo"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/releaseinfo"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/copyright"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/copyright"/>
  <!--hr/-->
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/legalnotice"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/legalnotice"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/pubdate"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/pubdate"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/revision"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/revision"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/revhistory"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/revhistory"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="bookinfo/abstract"/>
  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="info/abstract"/>
</xsl:template>

<xsl:template match="legalnotice" mode="titlepage.mode">
  <xsl:variable name="id"><xsl:call-template name="object.id"/></xsl:variable>
  <xsl:choose>
    <xsl:when test="$generate.legalnotice.link != 0">
      <xsl:variable name="filename">
        <xsl:call-template name="make-relative-filename">
          <xsl:with-param name="base.dir" select="$base.dir"/>
	  <xsl:with-param name="base.name">
            <xsl:apply-templates mode="chunk-filename" select="."/>
	  </xsl:with-param>
        </xsl:call-template>
      </xsl:variable>

      <xsl:variable name="title">
        <xsl:apply-templates select="." mode="title.markup"/>
      </xsl:variable>

      <xsl:variable name="href">
        <xsl:apply-templates mode="chunk-filename" select="."/>
      </xsl:variable>

      <a href="{$href}"><xsl:copy-of select="$title"/></a>

      <xsl:call-template name="write.chunk">
        <xsl:with-param name="filename" select="$filename"/>
        <xsl:with-param name="quiet" select="$chunk.quietly"/>
        <xsl:with-param name="content">
        <xsl:call-template name="user.preroot"/>
          <html>
            <head>
              <xsl:call-template name="system.head.content"/>
              <xsl:call-template name="head.content"/>
              <xsl:call-template name="user.head.content"/>
            </head>
            <body>
              <xsl:call-template name="body.attributes"/>
              <div>
                <xsl:apply-templates select="." mode="class.attribute"/>
                <xsl:apply-templates mode="titlepage.mode"/>
              </div>
            </body>
          </html>
          <xsl:value-of select="$chunk.append"/>
        </xsl:with-param>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <div>
        <xsl:apply-templates select="." mode="class.attribute"/>
        <a id="{$id}"/>
	<h1 class="legalnotice">
    <xsl:call-template name="gentext">
      <xsl:with-param name="key">LegalNotice</xsl:with-param>
    </xsl:call-template>
	</h1>
        <xsl:apply-templates mode="titlepage.mode"/>
      </div>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="chapter/title" mode="titlepage.mode" priority="2">
  <xsl:call-template name="component.title">
    <xsl:with-param name="node" select="ancestor::chapter[1]"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="section/title|section/info/title|sectioninfo/title" mode="titlepage.mode" priority="2">
  <xsl:call-template name="section.title"/>
</xsl:template>

<xsl:template match="edition" mode="titlepage.mode">
  <p>
    <xsl:apply-templates select="." mode="class.attribute"/>
    <xsl:call-template name="gentext">
      <xsl:with-param name="key" select="'Edition'"/>
    </xsl:call-template>
    <xsl:call-template name="gentext.space"/>
    <xsl:apply-templates mode="titlepage.mode"/>
  </p>
</xsl:template>

<xsl:template name="article.titlepage.recto">
  <xsl:choose>
    <xsl:when test="articleinfo/productname">
	<div class="producttitle" xsl:use-attribute-sets="book.titlepage.recto.style">
	  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="articleinfo/productname"/>
	  <xsl:text> </xsl:text>
	  <xsl:apply-templates mode="book.titlepage.recto.auto.mode" select="articleinfo/productnumber"/>
	</div>
    </xsl:when>
  </xsl:choose>
  <xsl:choose>
    <xsl:when test="articleinfo/title">
      <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/title"/>
    </xsl:when>
    <xsl:when test="artheader/title">
      <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/title"/>
    </xsl:when>
    <xsl:when test="info/title">
      <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/title"/>
    </xsl:when>
    <xsl:when test="title">
      <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="title"/>
    </xsl:when>
  </xsl:choose>

  <xsl:choose>
    <xsl:when test="articleinfo/subtitle">
      <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/subtitle"/>
    </xsl:when>
    <xsl:when test="artheader/subtitle">
      <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/subtitle"/>
    </xsl:when>
    <xsl:when test="info/subtitle">
      <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/subtitle"/>
    </xsl:when>
    <xsl:when test="subtitle">
      <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="subtitle"/>
    </xsl:when>
  </xsl:choose>

  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/corpauthor"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/corpauthor"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/corpauthor"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/authorgroup"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/authorgroup"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/authorgroup"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/author"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/author"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/author"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/othercredit"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/othercredit"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/othercredit"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/releaseinfo"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/releaseinfo"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/releaseinfo"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/copyright"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/copyright"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/copyright"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/legalnotice"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/legalnotice"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/legalnotice"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/pubdate"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/pubdate"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/pubdate"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/revision"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/revision"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/revision"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/revhistory"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/revhistory"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/revhistory"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="articleinfo/abstract"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="artheader/abstract"/>
  <xsl:apply-templates mode="article.titlepage.recto.auto.mode" select="info/abstract"/>
</xsl:template>

<!--
From: xhtml/block.xsl
Reason:  make para use a div since using P makes a bunch of nested block tags output invalid XHTML
Version: 1.72.0
-->

<xsl:template name="paragraph">
  <xsl:param name="class" select="''"/>
  <xsl:param name="content"/>

  <xsl:variable name="p">
    <div>
      <xsl:call-template name="dir"/>
        <xsl:apply-templates select="." mode="class.attribute">
          <xsl:with-param name="class" select="'para'"/>
        </xsl:apply-templates>
      <!--xsl:if test="@id or @xml:id">
        <xsl:attribute name="id">
          <xsl:value-of select="(@id|@xml:id)[1]"/>
        </xsl:attribute>
      </xsl:if-->
      <xsl:copy-of select="$content"/>
    </div>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="$html.cleanup != 0">
      <xsl:call-template name="unwrap.p">
        <xsl:with-param name="p" select="$p"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:copy-of select="$p"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!--
From: xhtml/block.xsl
Reason:  make para use a div since using P makes a bunch of nested block tags output invalid XHTML
Version: 1.72.0
-->
<xsl:template match="simpara">
  <!-- see also listitem/simpara in lists.xsl -->
  <div class="para">
    <xsl:if test="@role and $para.propagates.style != 0">
      <xsl:apply-templates select="." mode="class.attribute">
        <xsl:with-param name="class" select="@role"/>
      </xsl:apply-templates>
    </xsl:if>

    <xsl:call-template name="anchor"/>
    <p/>
    <xsl:apply-templates/>
  </div>
</xsl:template>

<xsl:template match="collab" mode="titlepage.mode">
</xsl:template>

<xsl:template match="funcprototype">
  <xsl:variable name="html-style">
    <xsl:call-template name="dbhtml-attribute">
      <xsl:with-param name="pis" select="ancestor::funcsynopsis//processing-instruction('dbhtml')"/>
      <xsl:with-param name="attribute" select="'funcsynopsis-style'"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="style">
    <xsl:choose>
      <xsl:when test="$html-style != ''">
        <xsl:value-of select="$html-style"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$funcsynopsis.style"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

<!--
  <xsl:variable name="tabular-p"
                select="$funcsynopsis.tabular.threshold &gt; 0
                        and string-length(.) &gt; $funcsynopsis.tabular.threshold"/>
-->

  <xsl:variable name="tabular-p" select="false()"/>

  <xsl:choose>
    <xsl:when test="$style = 'kr' and $tabular-p">
      <xsl:apply-templates select="." mode="kr-tabular"/>
    </xsl:when>
    <xsl:when test="$style = 'kr'">
      <xsl:apply-templates select="." mode="kr-nontabular"/>
    </xsl:when>
    <xsl:when test="$style = 'ansi' and $tabular-p">
      <xsl:apply-templates select="." mode="ansi-tabular"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:apply-templates select="." mode="ansi-nontabular"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="process.image">
  <!-- When this template is called, the current node should be  -->
  <!-- a graphic, inlinegraphic, imagedata, or videodata. All    -->
  <!-- those elements have the same set of attributes, so we can -->
  <!-- handle them all in one place.                             -->
  <xsl:param name="tag" select="'img'"/>
  <xsl:param name="alt"/>
  <xsl:param name="longdesc"/>

  <!-- The HTML img element only supports the notion of content-area
       scaling; it doesn't support the distinction between a
       content-area and a viewport-area, so we have to make some
       compromises.

       1. If only the content-area is specified, everything is fine.
          (If you ask for a three inch image, that's what you'll get.)

       2. If only the viewport-area is provided:
          - If scalefit=1, treat it as both the content-area and
            the viewport-area. (If you ask for an image in a five inch
            area, we'll make the image five inches to fill that area.)
          - If scalefit=0, ignore the viewport-area specification.

          Note: this is not quite the right semantic and has the additional
          problem that it can result in anamorphic scaling, which scalefit
          should never cause.

       3. If both the content-area and the viewport-area is specified
          on a graphic element, ignore the viewport-area.
          (If you ask for a three inch image in a five inch area, we'll assume
           it's better to give you a three inch image in an unspecified area
           than a five inch image in a five inch area.

       Relative units also cause problems. As a general rule, the stylesheets
       are operating too early and too loosely coupled with the rendering engine
       to know things like the current font size or the actual dimensions of
       an image. Therefore:

       1. We use a fixed size for pixels, $pixels.per.inch

       2. We use a fixed size for "em"s, $points.per.em

       Percentages are problematic. In the following discussion, we speak
       of width and contentwidth, but the same issues apply to depth and
       contentdepth

       1. A width of 50% means "half of the available space for the image."
          That's fine. But note that in HTML, this is a dynamic property and
          the image size will vary if the browser window is resized.

       2. A contentwidth of 50% means "half of the actual image width". But
          the stylesheets have no way to assess the image's actual size. Treating
          this as a width of 50% is one possibility, but it produces behavior
          (dynamic scaling) that seems entirely out of character with the
          meaning.

          Instead, the stylesheets define a $nominal.image.width
          and convert percentages to actual values based on that nominal size.

       Scale can be problematic. Scale applies to the contentwidth, so
       a scale of 50 when a contentwidth is not specified is analagous to a
       width of 50%. (If a contentwidth is specified, the scaling factor can
       be applied to that value and no problem exists.)

       If scale is specified but contentwidth is not supplied, the
       nominal.image.width is used to calculate a base size
       for scaling.

       Warning: as a consequence of these decisions, unless the aspect ratio
       of your image happens to be exactly the same as (nominal width / nominal height),
       specifying contentwidth="50%" and contentdepth="50%" is NOT going to
       scale the way you expect (or really, the way it should).

       Don't do that. In fact, a percentage value is not recommended for content
       size at all. Use scale instead.

       Finally, align and valign are troublesome. Horizontal alignment is now
       supported by wrapping the image in a <div align="{@align}"> (in block
       contexts!). I can't think of anything (practical) to do about vertical
       alignment.
  -->

  <xsl:variable name="width-units">
    <xsl:choose>
      <xsl:when test="$ignore.image.scaling != 0"/>
      <xsl:when test="@width">
        <xsl:call-template name="length-units">
          <xsl:with-param name="length" select="@width"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:when test="not(@depth) and $default.image.width != ''">
        <xsl:call-template name="length-units">
          <xsl:with-param name="length" select="$default.image.width"/>
        </xsl:call-template>
      </xsl:when>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="width">
    <xsl:choose>
      <xsl:when test="$ignore.image.scaling != 0"/>
      <xsl:when test="@width">
        <xsl:choose>
          <xsl:when test="$width-units = '%'">
            <xsl:value-of select="@width"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:call-template name="length-spec">
              <xsl:with-param name="length" select="@width"/>
            </xsl:call-template>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:when test="not(@depth) and $default.image.width != ''">
        <xsl:value-of select="$default.image.width"/>
      </xsl:when>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="scalefit">
    <xsl:choose>
      <xsl:when test="$ignore.image.scaling != 0">0</xsl:when>
      <xsl:when test="@contentwidth or @contentdepth">0</xsl:when>
      <xsl:when test="@scale">0</xsl:when>
      <xsl:when test="@scalefit"><xsl:value-of select="@scalefit"/></xsl:when>
      <xsl:when test="$width != '' or @depth">1</xsl:when>
      <xsl:otherwise>0</xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="scale">
    <xsl:choose>
      <xsl:when test="$ignore.image.scaling != 0">1.0</xsl:when>
      <xsl:when test="@contentwidth or @contentdepth">1.0</xsl:when>
      <xsl:when test="@scale">
        <xsl:value-of select="@scale div 100.0"/>
      </xsl:when>
      <xsl:otherwise>1.0</xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="filename">
    <xsl:choose>
      <xsl:when test="local-name(.) = 'graphic'                       or local-name(.) = 'inlinegraphic'">
        <!-- handle legacy graphic and inlinegraphic by new template --> 
        <xsl:call-template name="mediaobject.filename">
          <xsl:with-param name="object" select="."/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <!-- imagedata, videodata, audiodata -->
        <xsl:call-template name="mediaobject.filename">
          <xsl:with-param name="object" select=".."/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="output_filename">
    <xsl:choose>
      <xsl:when test="$embedtoc != 0 and contains($filename, 'Common_Content')">
        <xsl:value-of select=" concat($tocpath, '/../', $brand, '/',$langpath)"/>
        <xsl:value-of select="substring-after($filename, 'Common_Content')"/>
      </xsl:when>
      <xsl:when test="@entityref">
        <xsl:value-of select="$filename"/>
      </xsl:when>
      <!--
        Moved test for $keep.relative.image.uris to template below:
            <xsl:template match="@fileref">
      -->
      <xsl:otherwise>
        <xsl:value-of select="$filename"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="img.src.path.pi">
    <xsl:call-template name="dbhtml-attribute">
      <xsl:with-param name="pis" select="../processing-instruction('dbhtml')"/>
      <xsl:with-param name="attribute" select="'img.src.path'"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="filename.for.graphicsize">
    <xsl:choose>
      <xsl:when test="$img.src.path.pi != ''">
        <xsl:value-of select="concat($img.src.path.pi, $filename)"/>
      </xsl:when>
      <xsl:when test="$img.src.path != '' and                       $graphicsize.use.img.src.path != 0 and                       $tag = 'img' and                       not(starts-with($filename, '/')) and                       not(contains($filename, '://'))">
        <xsl:value-of select="concat($img.src.path, $filename)"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$filename"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="realintrinsicwidth">
    <!-- This funny compound test works around a bug in XSLTC -->
    <xsl:choose>
      <xsl:when test="$use.extensions != 0 and $graphicsize.extension != 0">
        <xsl:choose>
          <xsl:when test="function-available('simg:getWidth')">
            <xsl:value-of select="simg:getWidth(simg:new($filename.for.graphicsize),                                                 $nominal.image.width)"/>
          </xsl:when>
          <xsl:when test="function-available('ximg:getWidth')">
            <xsl:value-of select="ximg:getWidth(ximg:new($filename.for.graphicsize),                                                 $nominal.image.width)"/>
          </xsl:when>
          <xsl:otherwise>
           <xsl:value-of select="0"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="0"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="intrinsicwidth">
    <xsl:choose>
      <xsl:when test="$realintrinsicwidth = 0">
       <xsl:value-of select="$nominal.image.width"/>
      </xsl:when>
      <xsl:otherwise>
       <xsl:value-of select="$realintrinsicwidth"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="intrinsicdepth">
    <!-- This funny compound test works around a bug in XSLTC -->
    <xsl:choose>
      <xsl:when test="$use.extensions != 0 and $graphicsize.extension != 0">
        <xsl:choose>
          <xsl:when test="function-available('simg:getDepth')">
            <xsl:value-of select="simg:getDepth(simg:new($filename.for.graphicsize),                                                 $nominal.image.depth)"/>
          </xsl:when>
          <xsl:when test="function-available('ximg:getDepth')">
            <xsl:value-of select="ximg:getDepth(ximg:new($filename.for.graphicsize),                                                 $nominal.image.depth)"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$nominal.image.depth"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$nominal.image.depth"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="contentwidth">
    <xsl:choose>
      <xsl:when test="$ignore.image.scaling != 0"/>
      <xsl:when test="@contentwidth">
        <xsl:variable name="units">
          <xsl:call-template name="length-units">
            <xsl:with-param name="length" select="@contentwidth"/>
          </xsl:call-template>
        </xsl:variable>

        <xsl:choose>
          <xsl:when test="$units = '%'">
            <xsl:variable name="cmagnitude">
              <xsl:call-template name="length-magnitude">
                <xsl:with-param name="length" select="@contentwidth"/>
              </xsl:call-template>
            </xsl:variable>
            <xsl:value-of select="$intrinsicwidth * $cmagnitude div 100.0"/>
            <xsl:text>px</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:call-template name="length-spec">
              <xsl:with-param name="length" select="@contentwidth"/>
            </xsl:call-template>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$intrinsicwidth"/>
        <xsl:text>px</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="scaled.contentwidth">
    <xsl:if test="$contentwidth != ''">
      <xsl:variable name="cwidth.in.points">
        <xsl:call-template name="length-in-points">
          <xsl:with-param name="length" select="$contentwidth"/>
          <xsl:with-param name="pixels.per.inch" select="$pixels.per.inch"/>
          <xsl:with-param name="em.size" select="$points.per.em"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:value-of select="$cwidth.in.points div 72.0 * $pixels.per.inch * $scale"/>
    </xsl:if>
  </xsl:variable>

  <xsl:variable name="html.width">
    <xsl:choose>
      <xsl:when test="$ignore.image.scaling != 0"/>
      <xsl:when test="$width-units = '%'">
        <xsl:value-of select="$width"/>
      </xsl:when>
      <xsl:when test="$width != ''">
        <xsl:variable name="width.in.points">
          <xsl:call-template name="length-in-points">
            <xsl:with-param name="length" select="$width"/>
            <xsl:with-param name="pixels.per.inch" select="$pixels.per.inch"/>
            <xsl:with-param name="em.size" select="$points.per.em"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="round($width.in.points div 72.0 * $pixels.per.inch)"/>
      </xsl:when>
      <xsl:otherwise/>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="contentdepth">
    <xsl:choose>
      <xsl:when test="$ignore.image.scaling != 0"/>
      <xsl:when test="@contentdepth">
        <xsl:variable name="units">
          <xsl:call-template name="length-units">
            <xsl:with-param name="length" select="@contentdepth"/>
          </xsl:call-template>
        </xsl:variable>

        <xsl:choose>
          <xsl:when test="$units = '%'">
            <xsl:variable name="cmagnitude">
              <xsl:call-template name="length-magnitude">
                <xsl:with-param name="length" select="@contentdepth"/>
              </xsl:call-template>
            </xsl:variable>
            <xsl:value-of select="$intrinsicdepth * $cmagnitude div 100.0"/>
            <xsl:text>px</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <xsl:call-template name="length-spec">
              <xsl:with-param name="length" select="@contentdepth"/>
            </xsl:call-template>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$intrinsicdepth"/>
        <xsl:text>px</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="scaled.contentdepth">
    <xsl:if test="$contentdepth != ''">
      <xsl:variable name="cdepth.in.points">
        <xsl:call-template name="length-in-points">
          <xsl:with-param name="length" select="$contentdepth"/>
          <xsl:with-param name="pixels.per.inch" select="$pixels.per.inch"/>
          <xsl:with-param name="em.size" select="$points.per.em"/>
        </xsl:call-template>
      </xsl:variable>
      <xsl:value-of select="$cdepth.in.points div 72.0 * $pixels.per.inch * $scale"/>
    </xsl:if>
  </xsl:variable>

  <xsl:variable name="depth-units">
    <xsl:if test="@depth">
      <xsl:call-template name="length-units">
        <xsl:with-param name="length" select="@depth"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:variable>

  <xsl:variable name="depth">
    <xsl:if test="@depth">
      <xsl:choose>
        <xsl:when test="$depth-units = '%'">
          <xsl:value-of select="@depth"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:call-template name="length-spec">
            <xsl:with-param name="length" select="@depth"/>
          </xsl:call-template>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:variable>

  <xsl:variable name="html.depth">
    <xsl:choose>
      <xsl:when test="$ignore.image.scaling != 0"/>
      <xsl:when test="$depth-units = '%'">
        <xsl:value-of select="$depth"/>
      </xsl:when>
      <xsl:when test="@depth and @depth != ''">
        <xsl:variable name="depth.in.points">
          <xsl:call-template name="length-in-points">
            <xsl:with-param name="length" select="$depth"/>
            <xsl:with-param name="pixels.per.inch" select="$pixels.per.inch"/>
            <xsl:with-param name="em.size" select="$points.per.em"/>
          </xsl:call-template>
        </xsl:variable>
        <xsl:value-of select="round($depth.in.points div 72.0 * $pixels.per.inch)"/>
      </xsl:when>
      <xsl:otherwise/>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="viewport">
    <xsl:choose>
      <xsl:when test="$ignore.image.scaling != 0">0</xsl:when>
      <xsl:when test="local-name(.) = 'inlinegraphic'                       or ancestor::inlinemediaobject                       or ancestor::inlineequation">0</xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$make.graphic.viewport"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

<!--
  <xsl:message>=====================================
scale: <xsl:value-of select="$scale"/>, <xsl:value-of select="$scalefit"/>
@contentwidth <xsl:value-of select="@contentwidth"/>
$contentwidth <xsl:value-of select="$contentwidth"/>
scaled.contentwidth: <xsl:value-of select="$scaled.contentwidth"/>
@width: <xsl:value-of select="@width"/>
width: <xsl:value-of select="$width"/>
html.width: <xsl:value-of select="$html.width"/>
@contentdepth <xsl:value-of select="@contentdepth"/>
$contentdepth <xsl:value-of select="$contentdepth"/>
scaled.contentdepth: <xsl:value-of select="$scaled.contentdepth"/>
@depth: <xsl:value-of select="@depth"/>
depth: <xsl:value-of select="$depth"/>
html.depth: <xsl:value-of select="$html.depth"/>
align: <xsl:value-of select="@align"/>
valign: <xsl:value-of select="@valign"/></xsl:message>
-->

  <xsl:variable name="scaled" select="@width|@depth|@contentwidth|@contentdepth                         |@scale|@scalefit"/>

  <xsl:variable name="img">
    <xsl:choose>
      <xsl:when test="@format = 'SVG' and $svg.object != 0">
        <object data="{$output_filename}" type="image/svg+xml">
          <xsl:call-template name="process.image.attributes">
            <!--xsl:with-param name="alt" select="$alt"/ there's no alt here-->
            <xsl:with-param name="html.depth" select="$html.depth"/>
            <xsl:with-param name="html.width" select="$html.width"/>
            <xsl:with-param name="longdesc" select="$longdesc"/>
            <xsl:with-param name="scale" select="$scale"/>
            <xsl:with-param name="scalefit" select="$scalefit"/>
            <xsl:with-param name="scaled.contentdepth" select="$scaled.contentdepth"/>
            <xsl:with-param name="scaled.contentwidth" select="$scaled.contentwidth"/>
            <xsl:with-param name="viewport" select="$viewport"/>
          </xsl:call-template>
          <xsl:if test="@align">
            <xsl:attribute name="align">
                <xsl:choose>
                  <xsl:when test="@align = 'center'">middle</xsl:when>
                  <xsl:otherwise>
                    <xsl:value-of select="@align"/>
                  </xsl:otherwise>
                </xsl:choose>
            </xsl:attribute>
          </xsl:if>
          <xsl:if test="$use.embed.for.svg != 0">
            <embed src="{$output_filename}" type="image/svg+xml">
              <xsl:call-template name="process.image.attributes">
                <!--xsl:with-param name="alt" select="$alt"/ there's no alt here -->
                <xsl:with-param name="html.depth" select="$html.depth"/>
                <xsl:with-param name="html.width" select="$html.width"/>
                <xsl:with-param name="longdesc" select="$longdesc"/>
                <xsl:with-param name="scale" select="$scale"/>
                <xsl:with-param name="scalefit" select="$scalefit"/>
                <xsl:with-param name="scaled.contentdepth" select="$scaled.contentdepth"/>
                <xsl:with-param name="scaled.contentwidth" select="$scaled.contentwidth"/>
                <xsl:with-param name="viewport" select="$viewport"/>
              </xsl:call-template>
            </embed>
          </xsl:if>
          <img src="{substring-before($output_filename,'.svg')}.png" alt="{$alt}"/>
        </object>
      </xsl:when>
      <xsl:otherwise>
        <xsl:element name="{$tag}" namespace="http://www.w3.org/1999/xhtml">
         <xsl:if test="$tag = 'img' and ../../self::imageobjectco">
           <xsl:variable name="mapname">
             <xsl:call-template name="object.id">
               <xsl:with-param name="object" select="../../areaspec"/>
             </xsl:call-template>
           </xsl:variable>
           <xsl:choose>
             <xsl:when test="$scaled">
              <!-- It might be possible to handle some scaling; needs -->
              <!-- more investigation -->
              <xsl:message>
                <xsl:text>Warning: imagemaps not supported </xsl:text>
                <xsl:text>on scaled images</xsl:text>
              </xsl:message>
             </xsl:when>
             <xsl:otherwise>
              <xsl:attribute name="border">0</xsl:attribute>
              <xsl:attribute name="usemap">
                <xsl:value-of select="concat('#', $mapname)"/>
              </xsl:attribute>
             </xsl:otherwise>
           </xsl:choose>
         </xsl:if>

          <xsl:attribute name="src">
           <xsl:choose>
             <xsl:when test="$img.src.path != '' and                            $tag = 'img' and                              not(starts-with($output_filename, '/')) and                            not(contains($output_filename, '://'))">
               <xsl:value-of select="$img.src.path"/>
             </xsl:when>
           </xsl:choose>
            <xsl:value-of select="$output_filename"/>
          </xsl:attribute>

          <xsl:if test="@align">
            <xsl:attribute name="align">
              <xsl:choose>
                <xsl:when test="@align = 'center'">middle</xsl:when>
                <xsl:otherwise>
                  <xsl:value-of select="@align"/>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:attribute>
          </xsl:if>

          <xsl:call-template name="process.image.attributes">
            <xsl:with-param name="alt">
              <xsl:choose>
                <xsl:when test="$alt != ''">
                  <xsl:copy-of select="$alt"/>
                </xsl:when>
                <xsl:when test="ancestor::figure">
                  <xsl:value-of select="normalize-space(ancestor::figure/title)"/>
                </xsl:when>
              </xsl:choose>
            </xsl:with-param>
            <xsl:with-param name="html.depth" select="$html.depth"/>
            <xsl:with-param name="html.width" select="$html.width"/>
            <xsl:with-param name="longdesc" select="$longdesc"/>
            <xsl:with-param name="scale" select="$scale"/>
            <xsl:with-param name="scalefit" select="$scalefit"/>
            <xsl:with-param name="scaled.contentdepth" select="$scaled.contentdepth"/>
            <xsl:with-param name="scaled.contentwidth" select="$scaled.contentwidth"/>
            <xsl:with-param name="viewport" select="$viewport"/>
          </xsl:call-template>
        </xsl:element>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="bgcolor">
    <xsl:call-template name="dbhtml-attribute">
      <xsl:with-param name="pis" select="../processing-instruction('dbhtml')"/>
      <xsl:with-param name="attribute" select="'background-color'"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="use.viewport" select="$viewport != 0                         and ($html.width != ''                              or ($html.depth != '' and $depth-units != '%')                              or $bgcolor != ''                              or @valign)"/>

  <xsl:choose>
    <xsl:when test="$use.viewport">
      <table border="0" summary="manufactured viewport for HTML img" cellspacing="0" cellpadding="0">
        <xsl:if test="$html.width != ''">
          <xsl:attribute name="width">
            <xsl:value-of select="$html.width"/>
          </xsl:attribute>
        </xsl:if>
        <tr>
          <xsl:if test="$html.depth != '' and $depth-units != '%'">
            <!-- don't do this for percentages because browsers get confused -->
            <xsl:choose>
              <xsl:when test="$css.decoration != 0">
                <xsl:attribute name="style">
                  <xsl:text>height: </xsl:text>
                  <xsl:value-of select="$html.depth"/>
                  <xsl:text>px</xsl:text>
                </xsl:attribute>
              </xsl:when>
              <xsl:otherwise>
                <xsl:attribute name="height">
                  <xsl:value-of select="$html.depth"/>
                </xsl:attribute>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:if>
          <td>
            <xsl:if test="$bgcolor != ''">
              <xsl:choose>
                <xsl:when test="$css.decoration != 0">
                  <xsl:attribute name="style">
                    <xsl:text>background-color: </xsl:text>
                    <xsl:value-of select="$bgcolor"/>
                  </xsl:attribute>
                </xsl:when>
                <xsl:otherwise>
                  <xsl:attribute name="bgcolor">
                    <xsl:value-of select="$bgcolor"/>
                  </xsl:attribute>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:if>
            <xsl:if test="@align">
              <xsl:attribute name="align">
                <xsl:value-of select="@align"/>
              </xsl:attribute>
            </xsl:if>
            <xsl:if test="@valign">
              <xsl:attribute name="valign">
                <xsl:value-of select="@valign"/>
              </xsl:attribute>
            </xsl:if>
            <xsl:copy-of select="$img"/>
          </td>
        </tr>
      </table>
    </xsl:when>
    <xsl:otherwise>
      <xsl:copy-of select="$img"/>
    </xsl:otherwise>
  </xsl:choose>

  <xsl:if test="$tag = 'img' and ../../self::imageobjectco and not($scaled)">
    <xsl:variable name="mapname">
      <xsl:call-template name="object.id">
        <xsl:with-param name="object" select="../../areaspec"/>
      </xsl:call-template>
    </xsl:variable>

    <map name="{$mapname}">
      <xsl:for-each select="../../areaspec//area">
        <xsl:variable name="units">
          <xsl:choose>
            <xsl:when test="@units = 'other' and @otherunits">
              <xsl:value-of select="@otherunits"/>
            </xsl:when>
            <xsl:when test="@units">
              <xsl:value-of select="@units"/>
            </xsl:when>
            <!-- areaspec|areaset/area -->
            <xsl:when test="../@units = 'other' and ../@otherunits">
              <xsl:value-of select="../@otherunits"/>
            </xsl:when>
            <xsl:when test="../@units">
              <xsl:value-of select="../@units"/>
            </xsl:when>
            <!-- areaspec/areaset/area -->
            <xsl:when test="../../@units = 'other' and ../../@otherunits">
              <xsl:value-of select="../@otherunits"/>
            </xsl:when>
            <xsl:when test="../../@units">
              <xsl:value-of select="../../@units"/>
            </xsl:when>
            <xsl:otherwise>calspair</xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
 
        <xsl:choose>
          <xsl:when test="$units = 'calspair' or                           $units = 'imagemap'">
            <xsl:variable name="coords" select="normalize-space(@coords)"/>

            <area shape="rect">
              <xsl:variable name="linkends">
                <xsl:choose>
                  <xsl:when test="@linkends">
                    <xsl:value-of select="normalize-space(@linkends)"/>
                  </xsl:when>
                  <xsl:otherwise>
                    <xsl:value-of select="normalize-space(../@linkends)"/>
                  </xsl:otherwise>
                </xsl:choose>
              </xsl:variable>
 
              <xsl:variable name="href">
                <xsl:choose>
                  <xsl:when test="@xlink:href">
                    <xsl:value-of select="@xlink:href"/>
                  </xsl:when>
                  <xsl:otherwise>
                    <xsl:value-of select="../@xlink:href"/>
                  </xsl:otherwise>
                </xsl:choose>
              </xsl:variable>
 
              <xsl:choose>
                <xsl:when test="$linkends != ''">
                  <xsl:variable name="linkend">
                    <xsl:choose>
                      <xsl:when test="contains($linkends, ' ')">
                        <xsl:value-of select="substring-before($linkends, ' ')"/>
                      </xsl:when>
                      <xsl:otherwise>
                        <xsl:value-of select="$linkends"/>
                      </xsl:otherwise>
                    </xsl:choose>
                  </xsl:variable>
                  <xsl:variable name="target" select="key('id', $linkend)[1]"/>
                 
                  <xsl:if test="$target">
                    <xsl:attribute name="href">
                      <xsl:call-template name="href.target">
                        <xsl:with-param name="object" select="$target"/>
                      </xsl:call-template>
                    </xsl:attribute>
                  </xsl:if>
                </xsl:when>
                <xsl:when test="$href != ''">
                  <xsl:attribute name="href">
                    <xsl:value-of select="$href"/>
                  </xsl:attribute>
                </xsl:when>
              </xsl:choose>
 
              <xsl:if test="alt">
                <xsl:attribute name="alt">
                  <xsl:value-of select="alt[1]"/>
                </xsl:attribute>
              </xsl:if>
 
              <xsl:attribute name="coords">
                <xsl:choose>
                  <xsl:when test="$units = 'calspair'">

                    <xsl:variable name="p1" select="substring-before($coords, ' ')"/>
                    <xsl:variable name="p2" select="substring-after($coords, ' ')"/>
         
                    <xsl:variable name="x1" select="substring-before($p1,',')"/>
                    <xsl:variable name="y1" select="substring-after($p1,',')"/>
                    <xsl:variable name="x2" select="substring-before($p2,',')"/>
                    <xsl:variable name="y2" select="substring-after($p2,',')"/>
         
                    <xsl:variable name="x1p" select="$x1 div 100.0"/>
                    <xsl:variable name="y1p" select="$y1 div 100.0"/>
                    <xsl:variable name="x2p" select="$x2 div 100.0"/>
                    <xsl:variable name="y2p" select="$y2 div 100.0"/>
         
         <!--
                    <xsl:message>
                      <xsl:text>units: </xsl:text>
                      <xsl:value-of select="$units"/>
                      <xsl:text> </xsl:text>
                      <xsl:value-of select="$x1p"/><xsl:text>, </xsl:text>
                      <xsl:value-of select="$y1p"/><xsl:text>, </xsl:text>
                      <xsl:value-of select="$x2p"/><xsl:text>, </xsl:text>
                      <xsl:value-of select="$y2p"/><xsl:text>, </xsl:text>
                    </xsl:message>
         
                    <xsl:message>
                      <xsl:text>      </xsl:text>
                      <xsl:value-of select="$intrinsicwidth"/>
                      <xsl:text>, </xsl:text>
                      <xsl:value-of select="$intrinsicdepth"/>
                    </xsl:message>
         
                    <xsl:message>
                      <xsl:text>      </xsl:text>
                      <xsl:value-of select="$units"/>
                      <xsl:text> </xsl:text>
                      <xsl:value-of 
                            select="round($x1p * $intrinsicwidth div 100.0)"/>
                      <xsl:text>,</xsl:text>
                      <xsl:value-of select="round($intrinsicdepth
                                       - ($y2p * $intrinsicdepth div 100.0))"/>
                      <xsl:text>,</xsl:text>
                      <xsl:value-of select="round($x2p * 
                                            $intrinsicwidth div 100.0)"/>
                      <xsl:text>,</xsl:text>
                      <xsl:value-of select="round($intrinsicdepth
                                       - ($y1p * $intrinsicdepth div 100.0))"/>
                    </xsl:message>
         -->
                    <xsl:value-of select="round($x1p * $intrinsicwidth div 100.0)"/>
                    <xsl:text>,</xsl:text>
                    <xsl:value-of select="round($intrinsicdepth                                         - ($y2p * $intrinsicdepth div 100.0))"/>
                    <xsl:text>,</xsl:text>
                    <xsl:value-of select="round($x2p * $intrinsicwidth div 100.0)"/>
                    <xsl:text>,</xsl:text>
                    <xsl:value-of select="round($intrinsicdepth                                       - ($y1p * $intrinsicdepth div 100.0))"/>
                  </xsl:when>
                  <xsl:otherwise>
                    <xsl:copy-of select="$coords"/>
                  </xsl:otherwise>
                </xsl:choose>
              </xsl:attribute>
            </area>
          </xsl:when>
          <xsl:otherwise>
            <xsl:message>
              <xsl:text>Warning: only calspair or </xsl:text>
              <xsl:text>otherunits='imagemap' supported </xsl:text>
              <xsl:text>in imageobjectco</xsl:text>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:for-each>
    </map>
  </xsl:if>
</xsl:template>

<xsl:template match="tgroup" name="tgroup">
  <xsl:if test="not(@cols) or @cols = '' or string(number(@cols)) = 'NaN'">
    <xsl:message terminate="yes">
      <xsl:text>Error: CALS tables must specify the number of columns.</xsl:text>
    </xsl:message>
  </xsl:if>

  <xsl:variable name="summary">
    <xsl:call-template name="dbhtml-attribute">
      <xsl:with-param name="pis" select="processing-instruction('dbhtml')"/>
      <xsl:with-param name="attribute" select="'table-summary'"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="cellspacing">
    <xsl:call-template name="dbhtml-attribute">
      <xsl:with-param name="pis" select="processing-instruction('dbhtml')"/>
      <xsl:with-param name="attribute" select="'cellspacing'"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="cellpadding">
    <xsl:call-template name="dbhtml-attribute">
      <xsl:with-param name="pis" select="processing-instruction('dbhtml')[1]"/>
      <xsl:with-param name="attribute" select="'cellpadding'"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="colclass">
    <xsl:choose>
      <xsl:when test="@cols &lt; 4"><xsl:text>lt-4-cols</xsl:text></xsl:when>
      <xsl:when test="@cols &lt; 9"><xsl:text>gt-4-cols</xsl:text></xsl:when>
      <xsl:otherwise><xsl:text>gt-8-cols</xsl:text></xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="rowclass">
    <xsl:choose>
      <xsl:when test="count(tbody/row) &lt; 7"><xsl:text>lt-7-rows</xsl:text></xsl:when>
      <xsl:when test="count(tbody/row) &lt; 15"><xsl:text>gt-7-rows</xsl:text></xsl:when>
      <xsl:otherwise><xsl:text>gt-14-rows</xsl:text></xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <table>
    <xsl:attribute name="class">
      <xsl:value-of select="$colclass"/><xsl:text> </xsl:text><xsl:value-of select="$rowclass"/>
    </xsl:attribute>

    <xsl:choose>
      <!-- If there's a textobject/phrase for the table summary, use it -->
      <xsl:when test="../textobject/phrase">
        <xsl:attribute name="summary">
          <xsl:value-of select="../textobject/phrase"/>
        </xsl:attribute>
      </xsl:when>

      <!-- If there's a <?dbhtml table-summary="foo"?> PI, use it for
           the HTML table summary attribute -->
      <xsl:when test="$summary != ''">
        <xsl:attribute name="summary">
          <xsl:value-of select="$summary"/>
        </xsl:attribute>
      </xsl:when>

      <!-- Otherwise, if there's a title, use that -->
      <xsl:when test="../title">
        <xsl:attribute name="summary">
          <xsl:value-of select="string(../title)"/>
        </xsl:attribute>
      </xsl:when>

      <!-- Otherwise, forget the whole idea -->
      <xsl:otherwise><!-- nevermind --></xsl:otherwise>
    </xsl:choose>

    <xsl:if test="$cellspacing != '' or $html.cellspacing != ''">
      <xsl:attribute name="cellspacing">
        <xsl:choose>
          <xsl:when test="$cellspacing != ''">
            <xsl:value-of select="$cellspacing"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$html.cellspacing"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:attribute>
    </xsl:if>

    <xsl:if test="$cellpadding != '' or $html.cellpadding != ''">
      <xsl:attribute name="cellpadding">
        <xsl:choose>
          <xsl:when test="$cellpadding != ''">
            <xsl:value-of select="$cellpadding"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$html.cellpadding"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:attribute>
    </xsl:if>

    <xsl:if test="../@pgwide=1 or local-name(.) = 'entrytbl'">
      <xsl:attribute name="width">100%</xsl:attribute>
    </xsl:if>
<!--
snip border rubbish. BZ #875967
-->
    <xsl:variable name="colgroup">
      <colgroup>
        <xsl:call-template name="generate.colgroup">
          <xsl:with-param name="cols" select="@cols"/>
        </xsl:call-template>
      </colgroup>
    </xsl:variable>

    <xsl:variable name="explicit.table.width">
      <xsl:call-template name="dbhtml-attribute">
        <xsl:with-param name="pis" select="../processing-instruction('dbhtml')[1]"/>
        <xsl:with-param name="attribute" select="'table-width'"/>
      </xsl:call-template>
    </xsl:variable>

    <xsl:variable name="table.width">
      <xsl:choose>
        <xsl:when test="$explicit.table.width != ''">
          <xsl:value-of select="$explicit.table.width"/>
        </xsl:when>
        <xsl:when test="$default.table.width = ''">
          <xsl:text>100%</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$default.table.width"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:if test="$default.table.width != ''                   or $explicit.table.width != ''">
      <xsl:attribute name="width">
        <xsl:choose>
          <xsl:when test="contains($table.width, '%')">
            <xsl:value-of select="$table.width"/>
          </xsl:when>
          <xsl:when test="$use.extensions != 0                           and $tablecolumns.extension != 0">
            <xsl:choose>
              <xsl:when test="function-available('stbl:convertLength')">
                <xsl:value-of select="stbl:convertLength($table.width)"/>
              </xsl:when>
              <xsl:when test="function-available('xtbl:convertLength')">
                <xsl:value-of select="xtbl:convertLength($table.width)"/>
              </xsl:when>
              <xsl:otherwise>
                <xsl:message terminate="yes">
                  <xsl:text>No convertLength function available.</xsl:text>
                </xsl:message>
              </xsl:otherwise>
            </xsl:choose>
          </xsl:when>
          <xsl:otherwise>
            <xsl:value-of select="$table.width"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:attribute>
    </xsl:if>

    <xsl:choose>
      <xsl:when test="$use.extensions != 0                       and $tablecolumns.extension != 0">
        <xsl:choose>
          <xsl:when test="function-available('stbl:adjustColumnWidths')">
            <xsl:copy-of select="stbl:adjustColumnWidths($colgroup)"/>
          </xsl:when>
          <xsl:when test="function-available('xtbl:adjustColumnWidths')">
            <xsl:copy-of select="xtbl:adjustColumnWidths($colgroup)"/>
          </xsl:when>
          <xsl:when test="function-available('ptbl:adjustColumnWidths')">
            <xsl:copy-of select="ptbl:adjustColumnWidths($colgroup)"/>
          </xsl:when>
        <xsl:when test="function-available('perl:adjustColumnWidths')">
          <xsl:copy-of select="perl:adjustColumnWidths($table.width, $colgroup)"/>
        </xsl:when>
          <xsl:otherwise>
            <xsl:message terminate="yes">
              <xsl:text>No adjustColumnWidths function available.</xsl:text>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>
        <xsl:copy-of select="$colgroup"/>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:apply-templates select="thead"/>
    <xsl:apply-templates select="tfoot"/>
    <xsl:apply-templates select="tbody"/>

    <xsl:if test=".//footnote">
      <tbody class="footnotes">
        <tr>
          <td colspan="{@cols}">
            <xsl:apply-templates select=".//footnote" mode="table.footnote.mode"/>
          </td>
        </tr>
      </tbody>
    </xsl:if>
  </table>
</xsl:template>

<xsl:template match="programlistingco|screenco">
  <xsl:variable name="verbatim" select="programlisting|screen"/>

  <xsl:choose>
    <xsl:when test="$use.extensions != '0' and $callouts.extension != '0'">
      <xsl:variable name="rtf">
        <xsl:apply-templates select="$verbatim">
          <xsl:with-param name="suppress-numbers" select="'1'"/>
        </xsl:apply-templates>
      </xsl:variable>

      <xsl:variable name="rtf-with-callouts">
        <xsl:choose>
          <xsl:when test="function-available('sverb:insertCallouts')">
            <xsl:copy-of select="sverb:insertCallouts(areaspec,$rtf)"/>
          </xsl:when>
          <xsl:when test="function-available('xverb:insertCallouts')">
            <xsl:copy-of select="xverb:insertCallouts(areaspec,$rtf)"/>
          </xsl:when>
          <xsl:when test="function-available('perl:insertCallouts')">
            <xsl:copy-of select="perl:insertCallouts(areaspec,exsl:node-set($rtf))"/>
          </xsl:when>
          <xsl:otherwise>
            <xsl:message terminate="yes">
              <xsl:text>No insertCallouts function is available.</xsl:text>
            </xsl:message>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:variable>

      <xsl:choose>
        <xsl:when test="$verbatim/@linenumbering = 'numbered'                         and $linenumbering.extension != '0'">
          <div>
            <xsl:call-template name="common.html.attributes"/>
            <xsl:call-template name="number.rtf.lines">
              <xsl:with-param name="rtf" select="$rtf-with-callouts"/>
              <xsl:with-param name="pi.context" select="programlisting|screen"/>
            </xsl:call-template>
            <xsl:apply-templates select="calloutlist"/>
          </div>
        </xsl:when>
        <xsl:otherwise>
          <div>
            <xsl:call-template name="common.html.attributes"/>
            <xsl:copy-of select="$rtf-with-callouts"/>
            <xsl:apply-templates select="calloutlist"/>
          </div>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:otherwise>
      <div>
        <xsl:apply-templates select="." mode="common.html.attributes"/>
        <xsl:apply-templates/>
      </div>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="co" name="co">
  <!-- Support a single linkend in HTML -->
  <xsl:variable name="targets" select="key('id', @linkends)"/>
  <xsl:variable name="target" select="$targets[1]"/>
  <xsl:choose>
    <xsl:when test="$target">
      <a>
        <xsl:apply-templates select="." mode="common.html.attributes"/>
        <xsl:if test="@id or @xml:id">
          <xsl:attribute name="id">
            <xsl:value-of select="(@id|@xml:id)[1]"/>
          </xsl:attribute>
        </xsl:if>
        <xsl:attribute name="href">
          <xsl:call-template name="href.target">
            <xsl:with-param name="object" select="$target"/>
          </xsl:call-template>
        </xsl:attribute>
        <xsl:apply-templates select="." mode="callout-bug"/>
      </a>
    </xsl:when>
    <xsl:otherwise>
      <span>
        <xsl:if test="@id or @xml:id">
          <xsl:attribute name="id">
            <xsl:value-of select="(@id|@xml:id)[1]"/>
          </xsl:attribute>
        </xsl:if>
        <xsl:apply-templates select="." mode="callout-bug"/>
      </span>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="co" mode="callout-bug">
  <xsl:call-template name="callout-bug">
    <xsl:with-param name="conum">
      <xsl:number count="co" level="any" from="programlisting|screen|literallayout|synopsis" format="1"/>
    </xsl:with-param>
  </xsl:call-template>
</xsl:template>

<xsl:template name="callout-bug">
  <xsl:param name="conum" select="1"/>

  <xsl:choose>
    <xsl:when test="$callout.graphics != 0                     and $conum &lt;= $callout.graphics.number.limit">
      <img class="callout" src="{$callout.graphics.path}{$conum}{$callout.graphics.extension}" alt="{$conum}">
      </img>
    </xsl:when>
    <xsl:when test="$callout.unicode != 0                     and $conum &lt;= $callout.unicode.number.limit">
      <xsl:choose>
        <xsl:when test="$callout.unicode.start.character = 10102">
          <xsl:choose>
            <xsl:when test="$conum = 1">&#10102;</xsl:when>
            <xsl:when test="$conum = 2">&#10103;</xsl:when>
            <xsl:when test="$conum = 3">&#10104;</xsl:when>
            <xsl:when test="$conum = 4">&#10105;</xsl:when>
            <xsl:when test="$conum = 5">&#10106;</xsl:when>
            <xsl:when test="$conum = 6">&#10107;</xsl:when>
            <xsl:when test="$conum = 7">&#10108;</xsl:when>
            <xsl:when test="$conum = 8">&#10109;</xsl:when>
            <xsl:when test="$conum = 9">&#10110;</xsl:when>
            <xsl:when test="$conum = 10">&#10111;</xsl:when>
          </xsl:choose>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message>
            <xsl:text>Don't know how to generate Unicode callouts </xsl:text>
            <xsl:text>when $callout.unicode.start.character is </xsl:text>
            <xsl:value-of select="$callout.unicode.start.character"/>
          </xsl:message>
          <xsl:text>(</xsl:text>
          <xsl:value-of select="$conum"/>
          <xsl:text>)</xsl:text>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>(</xsl:text>
      <xsl:value-of select="$conum"/>
      <xsl:text>)</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="number.rtf.lines">
  <xsl:param name="rtf" select="''"/>
  <xsl:param name="pi.context" select="."/>

  <!-- Save the global values -->
  <xsl:variable name="global.linenumbering.everyNth" select="$linenumbering.everyNth"/>

  <xsl:variable name="global.linenumbering.separator" select="$linenumbering.separator"/>

  <xsl:variable name="global.linenumbering.width" select="$linenumbering.width"/>

  <!-- Extract the <?dbhtml linenumbering.*?> PI values -->
  <xsl:variable name="pi.linenumbering.everyNth">
    <xsl:call-template name="pi.dbhtml_linenumbering.everyNth">
      <xsl:with-param name="node" select="$pi.context"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="pi.linenumbering.separator">
    <xsl:call-template name="pi.dbhtml_linenumbering.separator">
      <xsl:with-param name="node" select="$pi.context"/>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="pi.linenumbering.width">
    <xsl:call-template name="pi.dbhtml_linenumbering.width">
      <xsl:with-param name="node" select="$pi.context"/>
    </xsl:call-template>
  </xsl:variable>

  <!-- Construct the 'in-context' values -->
  <xsl:variable name="linenumbering.everyNth">
    <xsl:choose>
      <xsl:when test="$pi.linenumbering.everyNth != ''">
        <xsl:value-of select="$pi.linenumbering.everyNth"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$global.linenumbering.everyNth"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="linenumbering.separator">
    <xsl:choose>
      <xsl:when test="$pi.linenumbering.separator != ''">
        <xsl:value-of select="$pi.linenumbering.separator"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$global.linenumbering.separator"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="linenumbering.width">
    <xsl:choose>
      <xsl:when test="$pi.linenumbering.width != ''">
        <xsl:value-of select="$pi.linenumbering.width"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$global.linenumbering.width"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:variable name="linenumbering.startinglinenumber">
    <xsl:choose>
      <xsl:when test="$pi.context/@startinglinenumber">
        <xsl:value-of select="$pi.context/@startinglinenumber"/>
      </xsl:when>
      <xsl:when test="$pi.context/@continuation='continues'">
        <xsl:variable name="lastLine">
          <xsl:choose>
            <xsl:when test="$pi.context/self::programlisting">
              <xsl:call-template name="lastLineNumber">
                <xsl:with-param name="listings" select="preceding::programlisting[@linenumbering='numbered']"/>
              </xsl:call-template>
            </xsl:when>
            <xsl:when test="$pi.context/self::screen">
              <xsl:call-template name="lastLineNumber">
                <xsl:with-param name="listings" select="preceding::screen[@linenumbering='numbered']"/>
              </xsl:call-template>
            </xsl:when>
            <xsl:when test="$pi.context/self::literallayout">
              <xsl:call-template name="lastLineNumber">
                <xsl:with-param name="listings" select="preceding::literallayout[@linenumbering='numbered']"/>
              </xsl:call-template>
            </xsl:when>
            <xsl:when test="$pi.context/self::address">
              <xsl:call-template name="lastLineNumber">
                <xsl:with-param name="listings" select="preceding::address[@linenumbering='numbered']"/>
              </xsl:call-template>
            </xsl:when>
            <xsl:when test="$pi.context/self::synopsis">
              <xsl:call-template name="lastLineNumber">
                <xsl:with-param name="listings" select="preceding::synopsis[@linenumbering='numbered']"/>
              </xsl:call-template>
            </xsl:when>
            <xsl:otherwise>
              <xsl:message>
                <xsl:text>Unexpected verbatim environment: </xsl:text>
                <xsl:value-of select="local-name($pi.context)"/>
              </xsl:message>
              <xsl:value-of select="0"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>

        <xsl:value-of select="$lastLine + 1"/>
      </xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:choose>
    <xsl:when test="function-available('sverb:numberLines')">
      <xsl:copy-of select="sverb:numberLines($rtf)"/>
    </xsl:when>
    <xsl:when test="function-available('xverb:numberLines')">
      <xsl:copy-of select="xverb:numberLines($rtf)"/>
    </xsl:when>
    <xsl:when test="function-available('perl:numberLines')">
      <xsl:copy-of select="perl:numberLines($linenumbering.startinglinenumber, $rtf)"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:message terminate="yes">
        <xsl:text>No numberLines function available.</xsl:text>
      </xsl:message>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!-- ONLY add title attribute if there is an alt text -->
<!-- Generate a title attribute for the context node -->
<xsl:template match="*" mode="html.title.attribute">
  <xsl:variable name="is.title">
    <xsl:call-template name="gentext.template.exists">
      <xsl:with-param name="context" select="'title'"/>
      <xsl:with-param name="name" select="local-name(.)"/>
      <xsl:with-param name="lang">
        <xsl:call-template name="l10n.language"/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="is.title-numbered">
    <xsl:call-template name="gentext.template.exists">
      <xsl:with-param name="context" select="'title-numbered'"/>
      <xsl:with-param name="name" select="local-name(.)"/>
      <xsl:with-param name="lang">
        <xsl:call-template name="l10n.language"/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="is.title-unnumbered">
    <xsl:call-template name="gentext.template.exists">
      <xsl:with-param name="context" select="'title-unnumbered'"/>
      <xsl:with-param name="name" select="local-name(.)"/>
      <xsl:with-param name="lang">
        <xsl:call-template name="l10n.language"/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:variable>

  <xsl:variable name="has.title.markup">
    <xsl:apply-templates select="." mode="title.markup">
      <xsl:with-param name="verbose" select="0"/>
    </xsl:apply-templates>
  </xsl:variable>

  <xsl:variable name="gentext.title">
    <xsl:if test="$has.title.markup != '???TITLE???' and                   ($is.title != 0 or                   $is.title-numbered != 0 or                   $is.title-unnumbered != 0)">
      <xsl:apply-templates select="." mode="object.title.markup.textonly"/>
    </xsl:if>
  </xsl:variable>

  <xsl:choose>
    <!--xsl:when test="string-length($gentext.title) != 0">
      <xsl:attribute name="title">
        <xsl:value-of select="$gentext.title"/>
      </xsl:attribute>
    </xsl:when-->
    <!-- Fall back to alt if available -->
    <xsl:when test="alt">
      <xsl:attribute name="title">
        <xsl:value-of select="normalize-space(alt)"/>
      </xsl:attribute>
    </xsl:when>
  </xsl:choose>
</xsl:template>

<xsl:template name="book.titlepage.separator">
</xsl:template>

<xsl:template name="reference.titlepage.separator">
</xsl:template>

<xsl:template name="article.titlepage.separator">
</xsl:template>

<xsl:template name="set.titlepage.separator">
</xsl:template>

<xsl:template match="footnote">
  <xsl:variable name="href">
    <xsl:text>#ftn.</xsl:text>
    <xsl:call-template name="object.id">
      <xsl:with-param name="conditional" select="0"/>
    </xsl:call-template>
  </xsl:variable>

      <xsl:call-template name="anchor">
        <xsl:with-param name="conditional" select="0"/>
      </xsl:call-template>
  <a href="{$href}">
    <xsl:apply-templates select="." mode="class.attribute"/>
    <sup>
      <xsl:apply-templates select="." mode="class.attribute"/>
      <xsl:call-template name="id.attribute">
        <xsl:with-param name="conditional" select="0"/>
      </xsl:call-template>
<!-- MOVED UP BZ #
      <xsl:call-template name="anchor">
        <xsl:with-param name="conditional" select="0"/>
      </xsl:call-template>
-->
      <xsl:text>[</xsl:text>
      <xsl:apply-templates select="." mode="footnote.number"/>
      <xsl:text>]</xsl:text>
    </sup>
  </a>
</xsl:template>

<!--

biblio.xsl

Fix double footnote in bibliography. BZ #653447

-->
<xsl:template match="bibliography">
  <xsl:call-template name="id.warning"/>

  <div>
    <xsl:call-template name="common.html.attributes">
      <xsl:with-param name="inherit" select="1"/>
    </xsl:call-template>
    <xsl:call-template name="id.attribute">
      <xsl:with-param name="conditional" select="0"/>
    </xsl:call-template>

    <xsl:call-template name="bibliography.titlepage"/>

    <xsl:apply-templates/>

    <xsl:if test="parent::book|parent::part">
      <xsl:call-template name="process.footnotes"/>
    </xsl:if>
  </div>
</xsl:template>

<xsl:template match="replaceable" priority="1">
  <xsl:call-template name="inline.italicseq"/>
</xsl:template>

<xsl:template name="anchor">
  <xsl:param name="node" select="."/>
  <xsl:param name="conditional" select="1"/>

  <xsl:choose>
    <xsl:when test="$generate.id.attributes != 0">
      <!-- No named anchors output when this param is set -->
    </xsl:when>
    <xsl:when test="$conditional = 0 or $node/@id or $node/@xml:id">
      <a>
        <xsl:attribute name="id">
          <xsl:call-template name="object.id">
            <xsl:with-param name="object" select="$node"/>
          </xsl:call-template>
        </xsl:attribute>
        <!-- need a zero-width non-breaking space because webkit doesn't render empty anchors -->
      &#8288;</a>
    </xsl:when>
  </xsl:choose>
</xsl:template>

</xsl:stylesheet>
