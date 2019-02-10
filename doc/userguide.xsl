<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<!--
 For note, tip, and warning: include little
 gifs of a hand pointing a finger, or a yield sign, and so on.
 Much prettier to look at than text.
-->
<xsl:param name="admon.graphics" select="1"/>
<xsl:param name="admon.graphics.extension">.gif</xsl:param>
<xsl:param name="admon.graphics.path">../images/</xsl:param>
<xsl:param name="graphic.default.extension">gif</xsl:param>

<!--
 In generating names of HTML pages, use the
 section's "id" attribute, if present, to form the name.
-->
<xsl:param name="use.id.as.filename" select="1"/>

<!-- The filename of the root HTML document (excluding the extension). -->
<xsl:param name="root.filename">index</xsl:param>

<!--
 Should the role attribute of emphasis be propagated to HTML as a
 class attribute value? Source Code
-->
<xsl:param name="emphasis.propagate.style" select="1"/>

<!--
 Generate links to the FreeTDS HTML stylesheet to control the
 browswer's rendering of certain classes of elements e.g. < userinput >.
-->
<xsl:param name="html.stylesheet" select="'userguide.css'"/>

<!-- Do not appent "TM" to every product -->
<xsl:template match="productname">
   <xsl:call-template name="inline.charseq"/>
</xsl:template>
</xsl:stylesheet>
