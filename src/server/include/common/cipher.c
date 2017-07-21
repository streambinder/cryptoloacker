#include <fcntl.h>
#include <math.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cipher.h"
#include "opt.h"

char aux_log[250];

int cipher(char *input_file, char *output_file, unsigned long seed) {
        // input file
        int input = open(input_file, O_RDONLY, 0);
        if (input == -1) {
                sprintf(aux_log, "Input file \"%s\" cannot be read.", input_file);
                std_err(aux_log);
                return CMD_NOK;
        }
        int input_lock = flock(input, LOCK_SH);
        if (input_lock != 0) {
                sprintf(aux_log, "Unable to get lock on input file \"%s\".", input_file);
                std_err(aux_log);
                close(input);
                return CMD_TRNS_NOK;
        }
        int input_size = lseek(input, 0, SEEK_END);
        if (input_size == -1) {
                sprintf(aux_log, "lseek error on input file \"%s\".", input_file);
                std_err(aux_log);
                close(input);
                return CMD_NOK;
        }
        int *input_map = mmap(NULL, input_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, input, 0);
        if (input_map == MAP_FAILED) {
                sprintf(aux_log, "Unable to memory-map input file \"%s\".", input_file);
                std_err(aux_log);
                close(input);
                return CMD_NOK;
        }

        // output file
        int output = open(output_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (output == -1) {
                sprintf(aux_log, "Output file \"%s\" cannot be written.", output_file);
                std_err(aux_log);
                close(input);
                return CMD_NOK;
        }
        int output_size = lseek(output, input_size-1, SEEK_SET);
        if (output_size == -1) {
                sprintf(aux_log, "lseek error on output file \"%s\".", output_file);
                std_err(aux_log);
                close(output);
                close(input);
                return CMD_NOK;
        }
        int result = write(output, "", 1);
        if (result < 0) {
                sprintf(aux_log, "Unable to write last byte of output file \"%s\".", output_file);
                std_err(aux_log);
                close(output);
                close(input);
                return CMD_NOK;
        }
        int *output_map = mmap(NULL, input_size-1, PROT_READ | PROT_WRITE, MAP_SHARED, output, 0);
        if (output_map == MAP_FAILED) {
                sprintf(aux_log, "Unable to memory-map output file \"%s\".", output_file);
                std_err(aux_log);
                close(output);
                close(input);
                return CMD_NOK;
        }

        int *key_map = mmap(NULL, input_size-1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (key_map == MAP_FAILED) {
                std_err("Unable to memory-map key map.");
                close(output);
                close(input);
                return CMD_NOK;
        } else {
                for (int i = 0; i < input_size/4.; i++) {
                        key_map[i] = rand_r(&seed);
                }
        }

        // XOR data and write to file
        uint64_t i,j;
        uint64_t upperbound_in;
        uint64_t upperbound_out = ceil(input_size/4./(PAR_FOR_AT_K*1024));
        #pragma omp parallel for
        for(j = 0; j < upperbound_out; j++) {
                upperbound_in = ceil((PAR_FOR_AT_K*1024));
                for(i = 0; i < upperbound_in; i++) {
                        uint64_t index = PAR_FOR_AT_K*1024*j + i;
                        if (index < input_size/4.) {
                                output_map[index] = input_map[index] ^ key_map[index];
                        }
                }
        }

        // close fds
        int input_unmap = munmap(input_map, input_size);
        if (input_unmap < 0) {
                sprintf(aux_log, "Something wrong while memory-unmapping input file \"%s\".", input_file);
                std_err(aux_log);
        }

        int output_unmap = munmap(output_map, input_size-1);
        if (output_unmap < 0) {
                sprintf(aux_log, "Something wrong while memory-unmapping output file \"%s\".", output_file);
                std_err(aux_log);
                close(output);
                return CMD_NOK;
        }

        int key_unmap = munmap(key_map, input_size-1);
        if (key_unmap < 0) {
                std_err("Something wrong while memory-unmapping key map.");
        }

        return CMD_CPH_OK;
}
