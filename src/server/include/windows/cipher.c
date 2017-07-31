#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <windows.h>

#include "cipher.h"
#include "opt.h"

static char aux_log[250];

static int cipher_rand(unsigned int *seed) {
        long k;
        long s = (long)(*seed);
        if (s == 0) {
                s = 0x12345987;
        }
        k = s / 127773;
        s = 16807 * (s - k * 127773) - 2836 * k;
        if (s < 0) {
                s += 2147483647;
        }
        (*seed) = (unsigned int)s;
        return (int)(s & RAND_MAX);
}

int cipher(char *input_file, char *output_file, unsigned int seed) {
        // input file
        HANDLE input = CreateFile(input_file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (input == INVALID_HANDLE_VALUE) {
                sprintf(aux_log, "Input file \"%s\" cannot be read: %ld.", input_file, GetLastError());
                std_err(aux_log);
                return CMD_NOK;
        }
        DWORD inputFileSizeHigh = 0;
        DWORD inputFileSizeLow = 0;
        inputFileSizeLow = GetFileSize(input, &inputFileSizeHigh);
        uint64_t input_size = (((uint64_t) inputFileSizeHigh) << 32 ) | inputFileSizeLow;
        if(!LockFile(input, (DWORD) 0, (DWORD) input_size, (DWORD) 0, (DWORD) input_size)) {
                sprintf(aux_log, "Unable to lock input file \"%s\": %ld.", input_file, GetLastError());
                std_err(aux_log);
                CloseHandle(input);
                return CMD_TRNS_NOK;
        }
        HANDLE *input_map = CreateFileMapping(input, NULL, PAGE_READONLY, 0, 0, NULL);
        if (input_map == NULL) {
                sprintf(aux_log, "Unable to memory-map input file \"%s\": %ld.", input_file, GetLastError());
                std_err(aux_log);
                CloseHandle(input);
                return CMD_NOK;
        }

        // output file
        HANDLE output = CreateFile(output_file, GENERIC_WRITE | GENERIC_READ, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (output == INVALID_HANDLE_VALUE) {
                sprintf(aux_log, "Output file \"%s\" opened be read: %ld.", output_file, GetLastError());
                std_err(aux_log);
                return CMD_NOK;
        }
        HANDLE *output_map = CreateFileMapping(output, NULL, PAGE_READWRITE, inputFileSizeHigh, inputFileSizeLow, NULL);
        if (output_map == NULL) {
                sprintf(aux_log, "Unable to memory-map output file \"%s\": %ld.", output_file, GetLastError());
                std_err(aux_log);
                CloseHandle(input);
                CloseHandle(output);
                return CMD_NOK;
        }

        HANDLE key_map = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, inputFileSizeHigh, inputFileSizeLow, NULL);
        if (key_map == NULL) {
                sprintf(aux_log, "Unable to memory-map key map: %ld.", GetLastError());
                std_err(aux_log);
                CloseHandle(input_map);
                CloseHandle(input);
                CloseHandle(output_map);
                CloseHandle(output);
                return CMD_NOK;
        }
        int *key_mapview = MapViewOfFile(key_map, FILE_MAP_WRITE, 0, 0, input_size);
        for (uint64_t i = 0; i < input_size/4.; i++) {
                key_mapview[i] = cipher_rand(&seed);
        }
        UnmapViewOfFile(key_mapview);

        // XOR data and write to file
        uint64_t i,j;
        uint64_t from, len;
        uint64_t upperbound_out = ceil(input_size/4./(PAR_FOR_AT_K*1024));
        #pragma omp parallel for
        for(j = 0; j < upperbound_out; j++) {
                from = j * (PAR_FOR_AT_K*1024);
                len = (from + (PAR_FOR_AT_K*1024) > input_size) ? (input_size - from) : (PAR_FOR_AT_K*1024);
                int *input_mapview = MapViewOfFile(input_map, FILE_MAP_READ, (DWORD) (from >> 32), (DWORD) from, len);
                int *output_mapview = MapViewOfFile(output_map, FILE_MAP_WRITE, (DWORD) (from >> 32), (DWORD) from, len);
                int *key_mapview = MapViewOfFile(key_map, FILE_MAP_WRITE, (DWORD) (from >> 32), (DWORD) from, len);
                for(i = 0; i < floor(len/4.); i++) {
                        output_mapview[i] = input_mapview[i] ^ key_mapview[i];
                }

                int missing = len % 4;
                if (missing != 0) {
                        char *input_char = (char *) input_mapview;
                        char *output_char = (char *) output_mapview;
                        char *key_char = (char *) key_mapview;
                        for (int i = len - missing; i < len; i++) {
                                output_char[i] = input_char[i] ^ key_char[i];
                        }
                }

                UnmapViewOfFile(input_mapview);
                UnmapViewOfFile(output_mapview);
                UnmapViewOfFile(key_mapview);
        }

        // close maps
        CloseHandle(input_map);
        CloseHandle(output_map);
        CloseHandle(key_map);

        UnlockFile(input, (DWORD) 0, (DWORD) input_size, (DWORD) 0, (DWORD) input_size);

        CloseHandle(input);
        CloseHandle(output);

        return CMD_CPH_OK;
}
