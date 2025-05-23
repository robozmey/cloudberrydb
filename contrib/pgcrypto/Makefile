# contrib/pgcrypto/Makefile

INT_SRCS = internal.c internal-sha2.c blf.c rijndael.c \
		pgp-mpi-internal.c imath.c sm4.c
INT_TESTS = sha2

OSSL_SRCS = openssl.c pgp-mpi-openssl.c sm4.c openssl_redirect.c
OSSL_TESTS = sha2 des 3des cast5

ZLIB_TST = pgp-compression
ZLIB_OFF_TST = pgp-zlib-DISABLED

CF_SRCS = $(if $(subst openssl,,$(with_ssl)), $(INT_SRCS), $(OSSL_SRCS))
CF_TESTS = $(if $(subst openssl,,$(with_ssl)), $(INT_TESTS), $(OSSL_TESTS))
CF_PGP_TESTS = $(if $(subst no,,$(with_zlib)), $(ZLIB_TST), $(ZLIB_OFF_TST))

SRCS = \
	$(CF_SRCS) \
	crypt-blowfish.c \
	crypt-des.c \
	crypt-gensalt.c \
	crypt-md5.c \
	mbuf.c \
	pgcrypto.c \
	pgp-armor.c \
	pgp-cfb.c \
	pgp-compress.c \
	pgp-decrypt.c \
	pgp-encrypt.c \
	pgp-info.c \
	pgp-mpi.c \
	pgp-pgsql.c \
	pgp-pubdec.c \
	pgp-pubenc.c \
	pgp-pubkey.c \
	pgp-s2k.c \
	pgp.c \
	px-crypt.c \
	px-hmac.c \
	px.c

MODULE_big	= pgcrypto
OBJS = \
	$(SRCS:.c=.o) \
	$(WIN32RES)

EXTENSION = pgcrypto
DATA = pgcrypto--1.3.sql pgcrypto--1.2--1.3.sql pgcrypto--1.1--1.2.sql \
	pgcrypto--1.0--1.1.sql
PGFILEDESC = "pgcrypto - cryptographic functions"

REGRESS_OPTS = --dbname=$(CONTRIB_TESTDB) --init-file=$(top_builddir)/src/test/regress/init_file

REGRESS = init md5 sha1 hmac-md5 hmac-sha1 blowfish rijndael sm4 \
	$(CF_TESTS) \
	crypt-des crypt-md5 crypt-blowfish crypt-xdes \
	pgp-armor pgp-decrypt pgp-encrypt $(CF_PGP_TESTS) \
	pgp-pubkey-decrypt pgp-pubkey-encrypt pgp-info \
	setup_fips fips

EXTRA_CLEAN = gen-rtab

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pgcrypto
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

# Add libraries that pgcrypto depends (or might depend) on into the
# shared library link.  (The order in which you list them here doesn't
# matter.)
SHLIB_LINK += $(filter -lcrypto -lz, $(LIBS))
ifeq ($(PORTNAME), win32)
SHLIB_LINK += $(filter -leay32, $(LIBS))
# those must be at the end
SHLIB_LINK += -lws2_32
endif

# Upstream uses a larger subset of C99.
imath.o: CFLAGS+=$(PERMIT_DECLARATION_AFTER_STATEMENT)

rijndael.o: rijndael.tbl

rijndael.tbl:
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPRINT_TABS rijndael.c -o gen-rtab
	./gen-rtab > rijndael.tbl
