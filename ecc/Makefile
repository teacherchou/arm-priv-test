# ecc extension - Build file

TARGET   = ecc_test.elf

EXT_OBJS = \
    main.o \
    ecc_inject.o \
    tests/test_register.o

include ../common/Makefile.common

# ecc_inject.h is in ecc/ root, tests are in ecc/tests/
CFLAGS += -I.
