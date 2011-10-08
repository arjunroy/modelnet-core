#
# modelnet  doc/Makefile.am
#
#     Distribute documentation
#
# author David Becker 
# Copyright (c) 2003 Duke University  All rights reserved.
# See COPYING for license statement.
#

## Process this file with automake to produce Makefile.in


DOCSRC_DIR = modelnet-doc
OUTPUT_NAME = modelnet
OUTPUT = modelnet.txt modelnet.html modelnet.pdf

MAIN_SGML = $(OUTPUT_NAME).sgml

build: $(OUTPUT)

$(OUTPUT_NAME).txt: $(DOCSRC_DIR)/*.sgml
	debiandoc2text $(MAIN_SGML)

$(OUTPUT_NAME).html: $(DOCSRC_DIR)/*.sgml
	debiandoc2html -1 $(MAIN_SGML)
	mv $(OUTPUT_NAME).html/index.html tmp.html
	-rm -rf $(OUTPUT_NAME).html
	mv tmp.html $(OUTPUT_NAME).html

$(OUTPUT_NAME).pdf: $(DOCSRC_DIR)/*.sgml
	debiandoc2pdf $(MAIN_SGML)
	-rm -f $(OUTPUT_NAME).tpt

clean:
	-rm -f $(OUTPUT)
	

release : build
	mv $(OUTPUT) ../
