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
} CompressedSequence;

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
      perror("Error opening file");
      return 1;
    }

   for (int i = 0; i < count; i++) {
        const CompressedSequence *current = sequences[i];
        size_t header_len = strlen(current->header);

        fwrite(&header_len, sizeof(size_t), 1, f);
        fwrite(current->header, 1, header_len, f);

        fwrite(&current->seq_len, sizeof(size_t), 1, f);
        fwrite(current->seq, 1, current->seq_len, f);
   }

   fclose(f);
   return 0;
}

int compress_file(char* fname, char* outname, char* custom_name) {

    char* splitname = custom_name;

    if (custom_name == NULL) {
        int a;
        splitname = split_string(fname, '.', &a)[0];
    }

    char* new_splitname = malloc(strlen(splitname) + 3);
    strcpy(new_splitname, splitname);
    strcat(new_splitname, ".f");
    splitname = new_splitname;
    
    int num_seqs;
    RawSequence* raw_fasta = read_raw_sequence(fname, &num_seqs);
    
    if (!raw_fasta) {
        printf("Error reading file: %s\n", fname);
        return 1;
    }

    CompressedSequence** compressed_fasta = malloc(sizeof(CompressedSequence*) * num_seqs);

    for (int i = 0; i < num_seqs; i++) {
        compressed_fasta[i] = compress_sequence(&raw_fasta[i]);
    }

    if (write_compressed_sequences(compressed_fasta, num_seqs, splitname) == 0) {
        printf("Successfully compressed to file: %s\n", splitname);
        return 0;
    }

    printf("%s\n", "There was an issue compressing your file");
    return 1;
}

CompressedSequence* read_compressed_sequence(const char* fname, int* seq_count) {
    FILE* f = fopen(fname, "rb");
    if (!f) {
        perror("Error opening file");
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
    new_seq->seq = malloc(sizeof(char) * seq->seq_len);

    for (int i = 0; i < seq->seq_len; i++) {
        for (int b = 0; b < 4; b++) {
            uint8_t byte = seq->seq[i];
            uint8_t base = (byte >> (6 - b * 2)) & 0b11;
            new_seq->seq[i * 4 + b] = decodebase(base);
        }
    }

    return new_seq;
}

int write_raw_sequence(RawSequence** sequences, int count, char* fname) {

    FILE* fp = fopen(fname, "wb");

    if (fp == NULL) {
        perror("Error writing to file");
    }

    for (int i = 0; i < count; i++) {

        fwrite(sequences[i]->header, sizeof(char), strlen(sequences[i]->header), fp);
        fputc('\n', fp);

        fwrite(sequences[i]->seq, sizeof(char), strlen(sequences[i]->seq), fp);
        fputc('\n', fp);
    }

    fclose(fp);

    return 0;

}

int decompress_file(char* fname, char* outname, char* custom_name) {
    char* splitname = custom_name;

    if (custom_name == NULL) {
        int a;
        splitname = split_string(fname, '.', &a)[0];
    }

    char* new_splitname = malloc(strlen(splitname) + 7);
    strcpy(new_splitname, splitname);
    strcat(new_splitname, ".fasta");
    splitname = new_splitname;

    int num_seqs;
    CompressedSequence* compressed_data = read_compressed_sequence(fname, &num_seqs);

    if (!compressed_data) {
        printf("Error reading file: %s\n", fname);
        return 1;
    }

    RawSequence** decompressed_data = malloc(sizeof(RawSequence*) * num_seqs);

    

    for (int i = 0; i < num_seqs; i++) {
        decompressed_data[i] = descompress_sequence(&compressed_data[i]);
    }

    if (write_raw_sequence(decompressed_data, num_seqs, splitname)) {
        perror("Error writing to file");
        return 1;
    }

    return 0;

}

int main(int argc, char *argv[]) {

    char* method = argv[1];
    char* src = argv[2];
    char* dest = NULL;


    if (argc < 3) {
        printf("%s", "No argument supplied\n");
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
        printf("Invalid command. Use 'compress' or 'decompress'\n");
        return 1;
    }

    
    char* outputname;
    
    

    return 0;

}
