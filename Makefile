subdir = contrib/pg_sendmail
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

MODULES = sendmail
DOCS = README.pg_sendmail

include $(top_srcdir)/contrib/contrib-global.mk
