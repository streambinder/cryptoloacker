#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

        //XOR data and write it to file
        char key_str[15];
        sprintf(key_str, "%d", key);
        int key_count = 0; //Used to restart key if strlen(key) < strlen(encrypt)
        int encrypt_byte;

        while ((encrypt_byte = fgetc(input)) != EOF) { //Loop through each byte of file until EOF
                // printf("Char: %c\n", encrypt_byte);
                //XOR the data and write it to a file
                fputc(encrypt_byte ^ key_str[key_count], output);

                //Increment key_count and start over if necessary
                key_count++;

                if (key_count == strlen(key_str)) {
                        key_count = 0;
                }
        }

        //Close files
        fclose(input);
        fclose(output);

        return 0;
}
