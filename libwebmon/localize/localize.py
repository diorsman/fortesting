#! /usr/bin/env python
# coding=utf-8

import os
from sgmllib import SGMLParser
import htmlentitydefs

class Image():
    def __init__(self):
        self.upLimit = -1
        self.labels = []
        self.lines = []

    def setYRange(self, upLimit):
        self.upLimit = upLimit

    def setLables(self, labels):
        self.labels = labels.split(',')

    def addLine(self, datas):
        l = []
        for p in datas.split(','):
            if p.strip() != '':
                l.append(p)
        self.lines.append(l)

    def drawTo(self, outFile, tmpDataFile='./.data.tmp', \
               tmpScriptFile='./.script.tmp'):
        # save data to a tmp file
        phyLines = [] 
        for i in range(0, len(self.lines[0])):
            phyLine = ['%d' % i]
            for j in range(0, len(self.lines)):
                phyLine.append(self.lines[j][i])
            phyLines.append(phyLine)
        # write
        fp = open(tmpDataFile, 'w')
        for phyLine in phyLines:
            fp.write('%s\n' % '\t'.join(phyLine))
        fp.close()

        # write script for gnuplot
        fp = open(tmpScriptFile, 'w')
        fp.write('set terminal png\n')
        fp.write('set output "%s"\n' % outFile)
        fp.write('set grid\n')
        if self.upLimit != -1:
            fp.write('set yrange [0:%d]\n' % self.upLimit)
        # plot
        plots = ['plot "%s" using 1:2 title "%s" with lines' % \
                 (tmpDataFile, self.labels[0])]
        if len(self.lines) > 1:
            for i in range(2, len(self.lines) + 1):
                plots.append('"%s" using 1:%d title "%s" with lines' % \
                             (tmpDataFile, i + 1, self.labels[i - 1]))
        fp.write(','.join(plots) + '\n')
        fp.close()

        # run gnuplot
        os.system('gnuplot %s' % tmpScriptFile)


# BaseHTMLProcessor: from <Dive Into Python>
#
#   __author__ = "Mark Pilgrim (mark@diveintopython.org)"
#   __version__ = "$Revision: 1.2 $"
#   __date__ = "$Date: 2004/05/05 21:57:19 $"
#   __copyright__ = "Copyright (c) 2001 Mark Pilgrim"
#   __license__ = "Python"
class BaseHTMLProcessor(SGMLParser):
    def reset(self):
        # extend (called by SGMLParser.__init__)
        self.pieces = []
        self.outputPath = ''
        self.imageNamePrefix = ''
        self.inApplet = False
        self.imageIndex = 0
        SGMLParser.reset(self)

    def setOutputPath(self, outputPath):
        self.outputPath = outputPath 

    def setImageNamePrefix(self, imageNamePrefix):
        self.imageNamePrefix = imageNamePrefix
        
    def unknown_starttag(self, tag, attrs):
        if self.inApplet:
            return
        # called for each start tag
        # attrs is a list of (attr, value) tuples
        # e.g. for <pre class="screen">, tag="pre", attrs=[("class", "screen")]
        # Ideally we would like to reconstruct original tag and attributes, but
        # we may end up quoting attribute values that weren't quoted in the source
        # document, or we may change the type of quotes around the attribute value
        # (single to double quotes).
        # Note that improperly embedded non-HTML code (like client-side Javascript)
        # may be parsed incorrectly by the ancestor, causing runtime script errors.
        # All non-HTML code must be enclosed in HTML comment tags (<!-- code -->)
        # to ensure that it will pass through this parser unaltered (in handle_comment).
        strattrs = "".join([' %s="%s"' % (key, value) for key, value in attrs])
        self.pieces.append("<%(tag)s%(strattrs)s>" % locals())
        
    def unknown_endtag(self, tag):
        if self.inApplet:
            return
        # called for each end tag, e.g. for </pre>, tag will be "pre"
        # Reconstruct the original end tag.
        self.pieces.append("</%(tag)s>" % locals())

    def handle_charref(self, ref):
        if self.inApplet:
            return
        # called for each character reference, e.g. for "&#160;", ref will be "160"
        # Reconstruct the original character reference.
        self.pieces.append("&#%(ref)s;" % locals())
        
    def handle_entityref(self, ref):
        if self.inApplet:
            return
        # called for each entity reference, e.g. for "&copy;", ref will be "copy"
        # Reconstruct the original entity reference.
        self.pieces.append("&%(ref)s" % locals())
        # standard HTML entities are closed with a semicolon; other entities are not
        if htmlentitydefs.entitydefs.has_key(ref):
            self.pieces.append(";")

    def handle_data(self, text):
        if self.inApplet:
            return
        # called for each block of plain text, i.e. outside of any tag and
        # not containing any character or entity references
        # Store the original text verbatim.
        self.pieces.append(text)
        
    def handle_comment(self, text):
        if self.inApplet:
            return
        # called for each HTML comment, e.g. <!-- insert Javascript code here -->
        # Reconstruct the original comment.
        # It is especially important that the source document enclose client-side
        # code (like Javascript) within comments so it can pass through this
        # processor undisturbed; see comments in unknown_starttag for details.
        self.pieces.append("<!--%(text)s-->" % locals())
        
    def handle_pi(self, text):
        if self.inApplet:
            return
        # called for each processing instruction, e.g. <?instruction>
        # Reconstruct original processing instruction.
        self.pieces.append("<?%(text)s>" % locals())

    def handle_decl(self, text):
        if self.inApplet:
            return
        # called for the DOCTYPE, if present, e.g.
        # <!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
        #     "http://www.w3.org/TR/html4/loose.dtd">
        # Reconstruct original DOCTYPE
        self.pieces.append("<!%(text)s>" % locals())

    def start_applet(self, attrs):
        self.inApplet = True
        # new Image
        self.curImage = Image()

    def end_applet(self):
        self.inApplet = False
        # create img file
        p = '%s%d.png' % (self.imageNamePrefix, self.imageIndex)
        self.curImage.drawTo(self.outputPath + '/' + p)
        # replace
        self.pieces.append('<img src="%s">' % p)
        self.imageIndex += 1

    def start_param(self, attrs):
        if self.inApplet:
            name = ''
            value = ''
            for (n, v) in attrs:
                if n == 'name':
                    if v.startswith('sampleValues_'):
                        name = 'data'
                    elif v.startswith('range'):
                        name = 'range'
                    elif v.startswith('legendLabels'):
                        name = 'label'
                elif n == 'value':
                    value = v
            if name == 'data':
                self.curImage.addLine(value)
            elif name == 'range':
                self.curImage.setYRange(int(float(value)))
            elif name == 'label':
                self.curImage.setLables(value)

    def output(self):
        """Return processed HTML as a single string"""
        return "".join(self.pieces)


if __name__ == '__main__':
    for f in os.listdir('./src'):
        if f.lower().endswith('.html') or f.lower().endswith('.htm'):
            # parse
            fp = open('./src/' + f, 'r')
            processor = BaseHTMLProcessor()
            processor.setOutputPath('./dest/')
            processor.setImageNamePrefix(f + '.')
            processor.feed(fp.read())
            processor.close()
            fp.close()

            # write
            # New images and new html file must be in the same directory.
            fp = open('./dest/' + f, 'w')
            fp.write(processor.output())
            fp.close()
