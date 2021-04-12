NULL =

SPEC_XML_FILES =			\
	budgie-screensaver.xml		\
	dbus-interface.xml		\
	$(NULL)

if DOCBOOK_DOCS_ENABLED

htmldocdir = $(DOCDIR)/spec
htmldoc_DATA = budgie-screensaver.html

budgie-screensaver.html : dbus-interface.xml
	$(XMLTO) html-nochunks -m config.xsl budgie-screensaver.xml

endif # DOCBOOK_DOCS_ENABLED

EXTRA_DIST =				\
	budgie-screensaver.xml.in	\
	config.xsl			\
	docbook.css			\
	$(SPEC_XML_FILES)
	$(NULL)

clean-local:
	rm -f *~
	rm -f budgie-screensaver.html

-include $(top_srcdir)/git.mk
