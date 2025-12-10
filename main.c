#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


typedef struct {
    char *header;
    char *seq;
} RawSequence;

typedef struct {
    char    *header;
    uint8_t  *seq;
    size_t   seq_len;
    size_t   real_len;
} CompressedSequence;

void clean_sequence(char* seq) {
    char* d = seq;
    do {
        while (*d == '\n' || *d == '\r') {
            ++d;
        }
    } while ((*seq++ = *d++));
}

uint8_t encode_base(char b) {
    switch(b) {
        case 'A': return 0b00;

        case 'C': return 0b01;
        case 'G': return 0b10;
        case 'T': return 0b11;
        default:  return 0b00; 
    }
}

char decodebase(uint8_t val) {
    char bases[] = {
        'A',
        'C',
        'G',
        'T'
    };

    if (val > 3) {
        perror("Invalid base detected");
        return 1;
    }

    return bases[val];
}


/*
    A = 00
    C = 01
    G = 10
    T = 11
*/



int count_char(const char* string, char c) {
    int count = 0;
    while (*string) {
        if (*string == c) {
            count++;
        }
        string++;
    }
    return count;
}

char **split_string(const char* string, char delimiter, int* count) {
    *count = count_char(string, delimiter) + 1;
    char **result = malloc(*count * sizeof(char*));
    
    int index = 0;
    const char *start = string;
    const char *end = string;
    
    while (*end) {
        if (*end == delimiter) {
            int length = end - start;
            result[index] = malloc(length + 1);
            strncpy(result[index], start, length);
            result[index][length] = '\0';
            index++;
            start = end + 1;
        }
        end++;
    }
    
    
    int length = end - start;
    result[index] = malloc(length + 1);
    strncpy(result[index], start, length);
    result[index][length] = '\0';
    
    return result;
}

RawSequence* split_seq(const char* string) {
    int index = 0;
    int len = strlen(string);

    if (len == 0) {
        return NULL;
    }

    while (index < len && string[index] != '\n') {
        index++;
    }

    RawSequence* sequence = malloc(sizeof(RawSequence));
    if (!sequence) return NULL;

    if (index == len) {
        sequence->header = malloc(len + 1);
        sequence->seq = malloc(1); 

        if (!sequence->header || !sequence->seq) {
            free(sequence->header);
            free(sequence->seq);
            free(sequence);
            return NULL;
        }

        memcpy(sequence->header, string, len);
        sequence->header[len] = '\0';

        sequence->seq[0] = '\0';
        return sequence;
    }

    sequence->header = malloc(index + 1);
    int seq_len = len - index - 1;
    sequence->seq = malloc(seq_len + 1);

    if (!sequence->header || !sequence->seq) {
        free(sequence->header);
        free(sequence->seq);
        free(sequence);
        return NULL;
    }


    memcpy(sequence->header, string, index);
    sequence->header[index] = '\0';


    memcpy(sequence->seq, string + index + 1, seq_len);
    sequence->seq[seq_len] = '\0';

    clean_sequence(sequence->seq);

    return sequence;
}

RawSequence* read_raw_sequence(const char* fname, int* seq_count) {
    FILE *f = fopen(fname, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = malloc(fsize + 1);
    if (!string) {
        fclose(f);
        return NULL;
    }
    
    fread(string, fsize, 1, f);
    fclose(f);
    string[fsize] = 0;

    char **parts = split_string(string, '>', seq_count);
    RawSequence* sequences = malloc(*seq_count * sizeof(RawSequence));
    if (!sequences) {
        free(string);
        for (int i = 0; i < *seq_count; i++) {
            free(parts[i]);
        }
        free(parts);
        return NULL;
    }

    int valid_count = 0;
    for (int i = 0; i < *seq_count; i++) {
        RawSequence* s = split_seq(parts[i]);
        free(parts[i]);
        
        if (s) {
            sequences[valid_count++] = *s;
            free(s);
        }
    }
    
    *seq_count = valid_count;
    free(parts);
    free(string);
    
    return sequences;
}

CompressedSequence* compress_sequence(const RawSequence* seq) {
    int raw_len = strlen(seq->seq);
    int num_bytes = (raw_len + 3) / 4;

    CompressedSequence* new_seq = malloc(sizeof(CompressedSequence));
    if (!new_seq) return NULL;

    new_seq->header = seq->header;
    new_seq->seq_len = num_bytes;
    new_seq->real_len = raw_len;
    new_seq->seq = calloc(num_bytes, sizeof(uint8_t));
    if (!new_seq->seq) {
        free(new_seq);
        return NULL;
    }

    for (int i = 0; i < num_bytes; i++) {
        uint8_t byte = 0;
        for (int j = 0; j < 4; j++) {
            int idx = i * 4 + j;
            char base = (idx < raw_len) ? seq->seq[idx] : 'A';
            byte = (byte << 2) | encode_base(base);
        }
        new_seq->seq[i] = byte;
    }

    return new_seq;
}

/*
    FORMAT: sizeof(header), header (uncompressed), sizeof(sequence), sequence (compressed)
*/
int write_compressed_sequences(CompressedSequence** sequences, int count, char* fname) {
    FILE* f;
    f = fopen(fname, "wb");

    if (f == NULL) {
      perror("Error opening file. Use --help for guide");
      return 1;
    }

   for (int i = 0; i < count; i++) {
        const CompressedSequence *current = sequences[i];
        size_t header_len = strlen(current->header);

        fwrite(&header_len, sizeof(size_t), 1, f);
        fwrite(current->header, 1, header_len, f);

        fwrite(&current->seq_len, sizeof(size_t), 1, f);
        fwrite(&current->real_len, sizeof(size_t), 1, f);
        fwrite(current->seq, 1, current->seq_len, f);
   }

   fclose(f);
   return 0;
}

int compress_file(char* fname, char* outname, char* custom_name) {

    char* splitname;

    if (custom_name != NULL) {
        splitname = malloc(strlen(custom_name) + 1);
        strcpy(splitname, custom_name);
    } else {
        char *dot = strrchr(fname, '.');
        size_t len;
        if (dot) {
            len = dot - fname;
        } else {
            len = strlen(fname);
        }
        splitname = malloc(len + 3);
        strncpy(splitname, fname, len);
        splitname[len] = '\0';
        strcat(splitname, ".f");
    }
    
    int num_seqs;
    
    FILE *f_in = fopen(fname, "rb");
    if (!f_in) {
        printf("Error reading file: %s. Use --help for guide\n", fname);
        return 1;
    }
    fseek(f_in, 0, SEEK_END);
    long input_size = ftell(f_in);
    fclose(f_in);

    RawSequence* raw_fasta = read_raw_sequence(fname, &num_seqs);
    
    if (!raw_fasta) {
        printf("Error reading file: %s. Use --help for guide\n", fname);
        return 1;
    }

    CompressedSequence** compressed_fasta = malloc(sizeof(CompressedSequence*) * num_seqs);

    for (int i = 0; i < num_seqs; i++) {
        compressed_fasta[i] = compress_sequence(&raw_fasta[i]);
    }

    if (write_compressed_sequences(compressed_fasta, num_seqs, splitname) == 0) {
        FILE *f_out = fopen(splitname, "rb");
        long output_size = 0;
        if (f_out) {
            fseek(f_out, 0, SEEK_END);
            output_size = ftell(f_out);
            fclose(f_out);
        }

        printf("Successfully compressed to file: %s\n", splitname);
        printf("Original size: %ld bytes\n", input_size);
        printf("Compressed size: %ld bytes\n", output_size);
        return 0;
    }

    printf("%s\n", "There was an issue compressing your file. Use --help for guide");
    return 1;
}

CompressedSequence* read_compressed_sequence(const char* fname, int* seq_count) {
    FILE* f = fopen(fname, "rb");
    if (!f) {
        perror("Error opening file. Use --help for guide");
        return NULL;
    }

    int capacity = 10;
    int count = 0;
    CompressedSequence* sequences = malloc(capacity * sizeof(CompressedSequence));
    if (!sequences) {
        fclose(f);
        return NULL;
    }

    while (1) {
        size_t header_len;

        if (fread(&header_len, sizeof(size_t), 1, f) != 1) {
            break;
        }


        if (count >= capacity) {
            capacity *= 2;
            CompressedSequence* temp = realloc(sequences, capacity * sizeof(CompressedSequence));
            if (!temp) {

                free(sequences);
                fclose(f);
                return NULL;
            }
            sequences = temp;
        }


        sequences[count].header = malloc(header_len + 1);
        fread(sequences[count].header, 1, header_len, f);
        sequences[count].header[header_len] = '\0';


        fread(&sequences[count].seq_len, sizeof(size_t), 1, f);
        fread(&sequences[count].real_len, sizeof(size_t), 1, f);


        sequences[count].seq = malloc(sequences[count].seq_len);
        fread(sequences[count].seq, 1, sequences[count].seq_len, f);

        count++;
    }

    fclose(f);
    *seq_count = count;
    return sequences;
}

RawSequence* descompress_sequence(const CompressedSequence* seq) {
    RawSequence* new_seq = malloc(sizeof(RawSequence));
    new_seq->header = seq->header;
    new_seq->seq = malloc(sizeof(char) * (seq->real_len + 1));

    int char_idx = 0;
    for (int i = 0; i < seq->seq_len; i++) {
        for (int b = 0; b < 4; b++) {
            if (char_idx >= seq->real_len) break;
            uint8_t byte = seq->seq[i];
            uint8_t base = (byte >> (6 - b * 2)) & 0b11;
            new_seq->seq[char_idx++] = decodebase(base);
        }
    }
    new_seq->seq[seq->real_len] = '\0';

    return new_seq;
}

int write_raw_sequence(RawSequence** sequences, int count, char* fname) {

    FILE* fp = fopen(fname, "wb");

    if (fp == NULL) {
        perror("Error writing to file. Use --help for guide");
    }

    for (int i = 0; i < count; i++) {

        fputc('>', fp);
        fwrite(sequences[i]->header, sizeof(char), strlen(sequences[i]->header), fp);
        fputc('\n', fp);

        fwrite(sequences[i]->seq, sizeof(char), strlen(sequences[i]->seq), fp);
        fputc('\n', fp);
    }

    fclose(fp);

    return 0;

}

int decompress_file(char* fname, char* outname, char* custom_name) {
    char* splitname;

    if (custom_name != NULL) {
        splitname = malloc(strlen(custom_name) + 1);
        strcpy(splitname, custom_name);
    } else {
        char *dot = strrchr(fname, '.');
        size_t len;
        if (dot) {
            len = dot - fname;
        } else {
            len = strlen(fname);
        }
        splitname = malloc(len + 7);
        strncpy(splitname, fname, len);
        splitname[len] = '\0';
        strcat(splitname, ".fasta");
    }

    int num_seqs;
    CompressedSequence* compressed_data = read_compressed_sequence(fname, &num_seqs);

    if (!compressed_data) {
        printf("Error reading file: %s. Use --help for guide\n", fname);
        return 1;
    }

    RawSequence** decompressed_data = malloc(sizeof(RawSequence*) * num_seqs);

    

    for (int i = 0; i < num_seqs; i++) {
        decompressed_data[i] = descompress_sequence(&compressed_data[i]);
    }

    if (write_raw_sequence(decompressed_data, num_seqs, splitname)) {
        perror("Error writing to file. Use --help for guide");
        return 1;
    }

    printf("Successfully decompressed to file: %s\n", splitname);
    return 0;

}

int main(int argc, char *argv[]) {

    char* method = argv[1];
    char* src = argv[2];
    char* dest = NULL;


    if (argc < 3) {
        if (!strcmp(argv[1], "--help")) {
            printf("FASTA Compression Tool - Usage Guide\n");
            printf("====================================\n\n");
            printf("This tool compresses and decompresses FASTA files using 2-bit encoding.\n\n");
            printf("COMMANDS:\n");
            printf("  compress <input_file> [output_file]\n");
            printf("    Compresses a FASTA file. If output_file is not specified,\n");
            printf("    creates a .f file with the same base name.\n\n");
            printf("  decompress <input_file> [output_file]\n");
            printf("    Decompresses a .f file back to FASTA format. If output_file\n");
            printf("    is not specified, creates a .fasta file with the same base name.\n\n");
            printf("EXAMPLES:\n");
            printf("  %s compress sequence.fasta\n", argv[0]);
            printf("  %s compress sequence.fasta compressed.f\n", argv[0]);
            printf("  %s decompress compressed.f\n", argv[0]);
            printf("  %s decompress compressed.f output.fasta\n\n", argv[0]);
            return 0;
        }

        printf("%s", "No argument supplied. Use --help for guide\n");
        return 0;
    } else if (argc > 3) {
        dest = argv[3];
    }

    char* filename = argv[2];

    if (!strcmp(argv[1], "compress")) {
        compress_file(src, dest, dest);
    } else if (!strcmp(argv[1], "decompress")) {
        decompress_file(src, dest, dest);
    } else {
        printf("Invalid command. Use --help for guide'\n");
        return 1;
    }

    
    char* outputname;
    
    

    return 0;

}
