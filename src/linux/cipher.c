#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cipher.h"

char aux_log[250];

int cipher(char *input_file, char *output_file, unsigned int key, char *logger) {

        FILE* input;
        FILE* output;

        //Open input and output files
        input = fopen(input_file, "r");
        output = fopen(output_file, "w");

        //Check input file
        if (input == NULL) {
                sprintf(logger, "Input file \"%s\" cannot be read.", input_file);
                return 1;
        }

        //Check output file
        if (output == NULL) {
                sprintf(logger, "Input file \"%s\" cannot be written.", output_file);
                return 1;
        }

        int par_for = 0;
        struct stat st;
        if (stat(input_file, &st) == 0) {
                if (st.st_size > 1024*PAR_FOR_AT_K) {
                        par_for = 1;
                }
        }


        //XOR data and write it to file
        char key_str[15];
        sprintf(key_str, "%d", key);
        int key_count = 0; //Used to restart key if strlen(key) < strlen(encrypt)
        char encrypt_bytes[5];
        int read_bytes;

        for(int byte_ctr = 0; byte_ctr < st.st_size; byte_ctr += 4) {
                read_bytes = fread(encrypt_bytes, 1, 4, input);
                for(int byte_ctr_sub = 0; byte_ctr_sub < read_bytes; byte_ctr_sub++) {
                        if (encrypt_bytes[byte_ctr_sub] == EOF) {
                                continue;
                        }

                        fputc(encrypt_bytes[byte_ctr_sub] ^ key_str[key_count], output);

                        //Increment key_count and start over if necessary
                        key_count++;
                        if (key_count == strlen(key_str)) {
                                key_count = 0;
                        }
                }
        }

        //Close files
        fclose(input);
        fclose(output);

        return 0;
}
