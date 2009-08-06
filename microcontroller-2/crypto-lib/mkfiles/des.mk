# Makefile for DES
ALGO_NAME := DES

# comment out the following line for removement of DES from the build process
BLOCK_CIPHERS += $(ALGO_NAME)

$(ALGO_NAME)_DIR      := des/
$(ALGO_NAME)_OBJ      := des.o
$(ALGO_NAME)_TEST_BIN := main-des-test.o $(CLI_STD)  \
                         nessie_bc_test.o nessie_common.o performance_test.o
$(ALGO_NAME)_NESSIE_TEST      := "nessie"
$(ALGO_NAME)_PERFORMANCE_TEST := "performance"

