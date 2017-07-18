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

int cipher(char *input_file, char *output_file, unsigned int key, char *logger) {

        // input file
        int input = open(input_file, O_RDONLY, 0);
        if (input == -1) {
                sprintf(logger, "Input file \"%s\" cannot be read.", input_file);
                return CMD_NOK;
        }
        int input_lock = flock(input, LOCK_SH);
        if (input_lock != 0) {
                sprintf(logger, "Unable to get lock on input file \"%s\".", input_file);
                close(input);
                return CMD_LST_OK;
        }
        int input_size = lseek(input, 0, SEEK_END);
        if (input_size == -1) {
                sprintf(logger, "lseek error on input file \"%s\".", input_file);
                close(input);
                return CMD_NOK;
        }
        int *input_map = mmap(NULL, input_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, input, 0);
        if (input_map == MAP_FAILED) {
                sprintf(logger, "Unable to memory-map input file \"%s\".", input_file);
                close(input);
                return CMD_NOK;
        }

        // output file
        int output = open(output_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (output == -1) {
                sprintf(logger, "Output file \"%s\" cannot be written.", output_file);
                close(input);
                return CMD_NOK;
        }
        int output_size = lseek(output, input_size-1, SEEK_SET);
        if (output_size == -1) {
                sprintf(logger, "lseek error on output file \"%s\".", output_file);
                close(output);
                close(input);
                return CMD_NOK;
        }
        int result = write(output, "", 1);
        if (result < 0) {
                sprintf(logger, "Unable to write last byte of output file \"%s\".", output_file);
                close(output);
                close(input);
                return CMD_NOK;
        }

        int *output_map = mmap(NULL, input_size-1, PROT_READ | PROT_WRITE, MAP_SHARED, output, 0);
        if (output_map == MAP_FAILED) {
                sprintf(logger, "Unable to memory-map output file \"%s\".", output_file);
                close(output);
                close(input);
                return CMD_NOK;
        }

        //XOR data and write it to file
        uint64_t i,j;
        uint64_t upperbound_in;
        uint64_t upperbound_out = ceil(input_size/4./(PAR_FOR_AT_K*1024));
        #pragma omp parallel for
        for(j = 0; j < upperbound_out; j++) {
                upperbound_in = ceil((PAR_FOR_AT_K*1024));
                for(i = 0; i < upperbound_in; i++) {
                        uint64_t index = PAR_FOR_AT_K*1024*j + i;
                        if (index < input_size/4.) {
                                output_map[index] = input_map[index] ^ key;
                        }
                }
        }

        //Close files
        int input_unmap = munmap(input_map, input_size);
        if (input_unmap < 0) {
                sprintf(logger, "Something wrong while memory-unmapping input file \"%s\".", input_file);
        }
        close(input);
        close(input_lock);

        int output_unmap = munmap(output_map, input_size-1);
        if (output_unmap < 0) {
                sprintf(logger, "Something wrong while memory-unmapping output file \"%s\".", output_file);
                close(output);
                return CMD_NOK;
        }
        close(output);

        return CMD_CPH_OK;
}
