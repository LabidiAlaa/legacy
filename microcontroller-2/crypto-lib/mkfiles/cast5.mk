# Makefile for CAST5
ALGO_NAME := CAST5

# comment out the following line for removement of CAST5 from the build process
BLOCK_CIPHERS += $(ALGO_NAME)

$(ALGO_NAME)_DIR      := cast5/
$(ALGO_NAME)_OBJ      := cast5.o
$(ALGO_NAME)_TEST_BIN := main-cast5-test.o $(CLI_STD) \
                         nessie_bc_test.o nessie_common.o performance_test.o
$(ALGO_NAME)_NESSIE_TEST      := "nessie"
$(ALGO_NAME)_PERFORMANCE_TEST := "performance"

