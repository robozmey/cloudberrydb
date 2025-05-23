#-------------------------------------------------------------------------
#
# Makefile for src/test
#
# Copyright (c) 1994, Regents of the University of California
#
# src/test/Makefile
#
#-------------------------------------------------------------------------

subdir = src/test
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

# GPDB_12_MERGE_FEATURE_NOT_SUPPORTED Since logical replication doesn't work for GPDB,
# removed "subscription" test from below list.
SUBDIRS = perl regress isolation modules authentication recovery

SUBDIRS += fsync walrep heap_checksum isolation2 fdw singlenode_regress singlenode_isolation2

# Test suites that are not safe by default but can be run if selected
# by the user via the whitespace-separated list in variable
# PG_TEST_EXTRA:
ifeq ($(with_openssl),yes)
SUBDIRS += crypto
endif
ifeq ($(with_gssapi),yes)
ifneq (,$(filter kerberos,$(PG_TEST_EXTRA)))
SUBDIRS += kerberos
endif
endif
ifeq ($(with_ldap),yes)
ifneq (,$(filter ldap,$(PG_TEST_EXTRA)))
SUBDIRS += ldap
endif
endif
ifeq ($(with_ssl),openssl)
ifneq (,$(filter ssl,$(PG_TEST_EXTRA)))
SUBDIRS += ssl
endif
endif

# We don't build or execute these by default, but we do want "make
# clean" etc to recurse into them.  (We must filter out those that we
# have conditionally included into SUBDIRS above, else there will be
# make confusion.)
ALWAYS_SUBDIRS = $(filter-out $(SUBDIRS),examples kerberos ldap ssl)

# We want to recurse to all subdirs for all standard targets, except that
# installcheck and install should not recurse into the subdirectory "modules".

recurse_alldirs_targets := $(filter-out installcheck install, $(standard_targets))
installable_dirs := $(filter-out modules, $(SUBDIRS))

$(call recurse,$(recurse_alldirs_targets))
$(call recurse,installcheck, $(installable_dirs))
$(call recurse,install, $(installable_dirs))

# New target installcheck-cbdb-parallel
check_dirs := regress isolation2

installcheck-cbdb-parallel: install
    $(call recurse,installcheck-cbdb-parallel,$(check_dirs))

singlenode_check_dirs := singlenode_regress isolation singlenode_isolation2
installcheck-singlenode: install
    $(call recurse,installcheck-singlenode,$(singlenode_check_dirs))

$(recurse_always)
