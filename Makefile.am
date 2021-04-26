# This file will be processed with automake-1.7 to create Makefile.in

AUTOMAKE_OPTIONS = 1.7

NULL =

SUBDIRS =			\
	src			\
	po 			\
	data			\
	doc			\
	$(NULL)

EXTRA_DIST =			\
	COPYING			\
	COPYING.LIB		\
	ChangeLog		\
	NEWS			\
	$(NULL)

DISTCLEANFILES =		\
	./po/.intltool-merge-cache	\
	$(NULL)

MAINTAINERCLEANFILES =		\
	*~			\
	intltool-*.in		\
	compile			\
	configure		\
	install-sh		\
	missing			\
	mkinstalldirs		\
	config.guess		\
	ltmain.sh		\
	config.sub		\
	depcomp			\
	Makefile.in		\
	config.h.*		\
	aclocal.m4		\
	acinclude.m4		\
	$(NULL)


-include $(top_srcdir)/git.mk
