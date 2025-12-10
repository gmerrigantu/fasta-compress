# FASTA Compress

A simple and efficient tool to compress and decompress FASTA files using 2-bit encoding for DNA sequences.

## Features

- **Compression**: Converts DNA sequences (A, C, G, T) into a compact 2-bit binary format.
- **Decompression**: Restores the original FASTA format from the compressed binary file.
- **Efficiency**: Reduces file size for large genomic datasets by almost 2/3.
- **Preservation**: Maintains sequence headers and original sequence lengths.

## Compilation

To compile the project, use a C compiler like `gcc`:

```bash
gcc main.c -o fasta-compress
```

## Usage

### Compress

Compress a FASTA file. If no output filename is specified, it creates a `.f` file with the same base name.

```bash
./fasta-compress compress <input_file> [output_file]
```

**Example:**

```bash
./fasta-compress compress sequence.fasta
# Output: sequence.f
```

### Decompress

Decompress a `.f` file back to FASTA format. If no output filename is specified, it creates a `.fasta` file.

```bash
./fasta-compress decompress <input_file> [output_file]
```

**Example:**

```bash
./fasta-compress decompress sequence.f
# Output: sequence.fasta
```

### Help

View the usage guide:

```bash
./fasta-compress --help
```
