#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define SEARCH_FAILURE          0xffffffffffffffff
#define ROOT_ID                 0xffffffffffffffff
#define BYTES_PER_SECT          512
#define SECTORS_PER_BLOCK       (bytesperblock / BYTES_PER_SECT)
#define BYTES_PER_BLOCK         (SECTORS_PER_BLOCK * BYTES_PER_SECT)
#define ENTRIES_PER_SECT        2
#define ENTRIES_PER_BLOCK       (SECTORS_PER_BLOCK * ENTRIES_PER_SECT)
#define FILENAME_LEN            218
#define RESERVED_BLOCKS         16
#define FILE_TYPE               0
#define DIRECTORY_TYPE          1
#define DELETED_ENTRY           0xfffffffffffffffe
#define RESERVED_BLOCK          0xfffffffffffffff0
#define END_OF_CHAIN            0xffffffffffffffff

typedef struct {
    uint64_t parent_id;
    uint8_t type;
    char name[FILENAME_LEN];
    uint8_t perms;
    uint16_t owner;
    uint16_t group;
    uint64_t time;
    uint64_t payload;
    uint64_t size;
} __attribute__((packed)) entry_t;

typedef struct {
    uint64_t target_entry;
    entry_t target;
    entry_t parent;
    char name[FILENAME_LEN];
    int failure;
    int not_found;
} path_result_t;

static int verbose = 0;

static FILE* image;
static uint64_t imgsize;
static uint64_t blocks;
static uint64_t fatsize;
static uint64_t fatstart = RESERVED_BLOCKS;
static uint64_t dirsize;
static uint64_t dirstart;
static uint64_t datastart;
static uint64_t bytesperblock;

static inline uint8_t rd_byte(uint64_t loc) {
    fseek(image, (long)loc, SEEK_SET);
    return (uint8_t)fgetc(image);
}

static inline void wr_byte(uint64_t loc, uint8_t x) {
    fseek(image, (long)loc, SEEK_SET);
    fputc((int)x, image);
    return;
}

static inline uint16_t rd_word(uint64_t loc) {
    uint16_t x = 0;
    fseek(image, (long)loc, SEEK_SET);
    fread(&x, 2, 1, image);
    return x;
}

static inline void wr_word(uint64_t loc, uint16_t x) {
    fseek(image, (long)loc, SEEK_SET);
    fwrite(&x, 2, 1, image);
    return;
}

static inline uint32_t rd_dword(uint64_t loc) {
    uint32_t x = 0;
    fseek(image, (long)loc, SEEK_SET);
    fread(&x, 4, 1, image);
    return x;
}

static inline void wr_dword(uint64_t loc, uint32_t x) {
    fseek(image, (long)loc, SEEK_SET);
    fwrite(&x, 4, 1, image);
    return;
}

static inline uint64_t rd_qword(uint64_t loc) {
    uint64_t x = 0;
    fseek(image, (long)loc, SEEK_SET);
    fread(&x, 8, 1, image);
    return x;
}

static inline void wr_qword(uint64_t loc, uint64_t x) {
    fseek(image, (long)loc, SEEK_SET);
    fwrite(&x, 8, 1, image);
    return;
}

static inline entry_t rd_entry(uint64_t entry) {
    entry_t res;
    uint64_t loc = (dirstart * bytesperblock) + (entry * sizeof(entry_t));

    if (loc >= (dirstart + dirsize) * bytesperblock) {
        fprintf(stderr, "PANIC! ATTEMPTING TO READ DIRECTORY OUT OF BOUNDS!\n");
        abort();
    }

    fseek(image, (long)loc, SEEK_SET);
    fread(&res, sizeof(entry_t), 1, image);

    return res;
}

static inline void wr_entry(uint64_t entry, entry_t entry_src) {
    uint64_t loc = (dirstart * bytesperblock) + (entry * sizeof(entry_t));

    if (loc >= (dirstart + dirsize) * BYTES_PER_BLOCK) {
        fprintf(stderr, "PANIC! ATTEMPTING TO WRITE DIRECTORY OUT OF BOUNDS!\n");
        abort();
    }

    fseek(image, (long)loc, SEEK_SET);
    fwrite(&entry_src, sizeof(entry_t), 1, image);

    return;
}

static uint64_t import_chain(FILE *source) {
    uint64_t start_block;
    uint64_t block;
    uint64_t prev_block;
    uint64_t loc = (fatstart * bytesperblock);
    uint8_t *block_buf = malloc(bytesperblock);
    if (!block_buf) {
        perror("malloc failure");
        abort();
    }

    fseek(source, 0L, SEEK_END);
    uint64_t source_size = (uint64_t)ftell(source);
    rewind(source);

    uint64_t source_size_blocks = source_size / bytesperblock;
    if (source_size % bytesperblock) source_size_blocks++;

    if (verbose) {
        fprintf(stdout, "file size: %" PRIu64 "\n", source_size);
        fprintf(stdout, "file size in blocks: %" PRIu64 "\n", source_size_blocks);
    }

    // find first block
    for (block = 0; rd_qword(loc); block++) loc += sizeof(uint64_t);
    start_block = block;
    if (verbose) fprintf(stdout, "first block of chain is #%" PRIu64 "\n", start_block);    
    
    for (uint64_t x = 0; ; x++) {
        prev_block = (fatstart * bytesperblock) + (block * sizeof(uint64_t));
        wr_qword(loc, END_OF_CHAIN);
        
        fseek(image, (long)(block * bytesperblock), SEEK_SET);    
        // copy block
        fread(block_buf, bytesperblock, 1, source);
        fwrite(block_buf, bytesperblock, 1, image);

        if ((uint64_t)ftell(source) >= source_size)
            break;

        if (x == source_size_blocks)
            break;
        
        // find next block
        loc = (fatstart * bytesperblock);
        for (block = 0; rd_qword(loc); block++) loc += sizeof(uint64_t);
        
        wr_qword(prev_block, block);
    }

    free(block_buf);
    return start_block;
}

static void export_chain(FILE *dest, entry_t src) {
    uint64_t cur_block;
    uint8_t *block_buf = malloc(bytesperblock);
    if (!block_buf) {
        perror("malloc failure");
        abort();
    }

    for (cur_block = src.payload; cur_block != END_OF_CHAIN; ) {
        fseek(image, (long)(cur_block * bytesperblock), SEEK_SET);
        // copy block
        if (((uint64_t)ftell(dest) + bytesperblock) >= src.size) {
            fread(block_buf, src.size % bytesperblock, 1, image);
            fwrite(block_buf, src.size % bytesperblock, 1, dest);
            break;
        } else {
            fread(block_buf, bytesperblock, 1, image);
            fwrite(block_buf, bytesperblock, 1, dest);
        }

        cur_block = rd_qword((fatstart * bytesperblock) + (cur_block * sizeof(uint64_t)));
    }

    free(block_buf);
    return;        
}

static uint64_t search(const char *name, uint64_t parent, uint8_t type) {
    // returns unique entry #, SEARCH_FAILURE upon failure/not found
    for (uint64_t i = 0; ; i++) {
        entry_t entry = rd_entry(i);
        if (!entry.parent_id) return SEARCH_FAILURE;              // check if past last entry
        if (i >= (dirsize * ENTRIES_PER_BLOCK)) return SEARCH_FAILURE;  // check if past directory table
        if ((entry.parent_id == parent) && (entry.type == type) && (!strcmp(entry.name, name)))
            return i;
    }
}

static path_result_t path_resolver(const char *path, uint8_t type) {
    // returns a struct of useful info
    // failure flag set upon failure
    // not_found flag set upon not found
    // even if the file is not found, info about the "parent"
    // directory and name are still returned
    char name[FILENAME_LEN];
    entry_t parent = {0};
    int last = 0;
    int i;
    path_result_t result;
    entry_t empty_entry = {0};
    
    result.name[0] = 0;
    result.target_entry = 0;
    result.parent = empty_entry;
    result.target = empty_entry;
    result.failure = 0;
    result.not_found = 0;
    
    parent.payload = ROOT_ID;
    
    if ((type == DIRECTORY_TYPE) && !strcmp(path, "/")) {
        result.target.payload = ROOT_ID;
        return result; // exception for root
    }
    if ((type == FILE_TYPE) && !strcmp(path, "/")) {
        result.failure = 1;
        return result; // fail if looking for a file named "/"
    }
    
    if (*path == '/') path++;

next:    
    for (i = 0; *path != '/'; path++) {
        if (!*path) {
            last = 1;
            break;
        }
        name[i++] = *path;
    }
    name[i] = 0;
    path++;
    
    if (!last) {
        uint64_t search_res = search(name, parent.payload, DIRECTORY_TYPE);
        if (search_res == SEARCH_FAILURE) {
            result.failure = 1; // fail if search fails
            return result;
        }
        parent = rd_entry(search_res);
    } else {
        uint64_t search_res = search(name, parent.payload, type);
        if (search_res == SEARCH_FAILURE)
            result.not_found = 1;
        else {
            result.target = rd_entry(search_res);
            result.target_entry = search_res;
        }
        result.parent = parent;
        strcpy(result.name, name);
        return result;
    }

    goto next;
}

static inline uint64_t get_free_id(void) {
    uint64_t id = 1;
    uint64_t i;

    for (i = 0; ; i++) {
        entry_t entry = rd_entry(i);
        if (!entry.parent_id)
            break;
        if ((entry.type == 1) && (entry.payload == id))
            id = (entry.payload + 1);
    }
    
    return id;
}

static void mkdir_cmd(int argc, char **argv) {
    uint64_t i;
    entry_t entry = {0};
    
    if (argc < 4) {
        fprintf(stderr, "%s: %s: missing argument: directory name.\n", argv[0], argv[2]);
        return;
    }

    path_result_t path_result = path_resolver(argv[3], DIRECTORY_TYPE);

    // check if it exists
    if (!(path_result.not_found)) {
        fprintf(stderr, "%s: %s: directory `%s` already exists.\n", argv[0], argv[2], argv[3]);
        return;
    }
    
    // find empty entry
    for (i = 0; ; i++) {
        entry_t entry_i = rd_entry(i);
        if ((entry_i.parent_id == 0) || (entry_i.parent_id == DELETED_ENTRY))
            break;
    }
    
    entry.parent_id = path_result.parent.payload;
    if (verbose) fprintf(stdout, "new directory's parent ID: %" PRIu64 "\n", entry.parent_id);
    entry.type = DIRECTORY_TYPE;
    strcpy(entry.name, path_result.name);
    entry.payload = get_free_id();
    if (verbose) fprintf(stdout, "new directory's ID: %" PRIu64 "\n", entry.payload);
    if (verbose) fprintf(stdout, "writing to entry #%" PRIu64 "\n", i);
    entry.time = (uint64_t)time(NULL);
    
    wr_entry(i, entry);
    
    if (verbose) fprintf(stdout, "created directory `%s`\n", argv[3]);

    return;
}

static void import_cmd(int argc, char **argv) {
    FILE *source;
    entry_t entry = {0};
    uint64_t i;
    
    if (argc < 4) {
        fprintf(stderr, "%s: %s: missing argument: source file.\n", argv[0], argv[2]);
        return;
    }
    if (argc < 5) {
        fprintf(stderr, "%s: %s: missing argument: destination file.\n", argv[0], argv[2]);
        return;
    }

    struct stat s;
    stat(argv[3], &s);
    if (!S_ISREG(s.st_mode)) {
        fprintf(stderr, "%s: warning: source file `%s` is not a regular file, exiting.\n", argv[0], argv[3]);
        return;
    }

    // make directory
    if (path_resolver(argv[4], FILE_TYPE).failure) {
        char newdirname[4096];
        int i = 0;
subdir:
        for (;; i++) {
            if (argv[4][i] == '/')
                break;
            newdirname[i] = argv[4][i];
        }
        newdirname[i] = 0;
        char *oldargv3 = argv[3];
        argv[3] = newdirname;
        mkdir_cmd(argc, argv);
        argv[3] = oldargv3;
        if (path_resolver(argv[4], FILE_TYPE).failure) {
            newdirname[i++] = '/';
            goto subdir;
        }
    }

    path_result_t path_result = path_resolver(argv[4], FILE_TYPE);

    // check if the file exists
    if (!path_result.not_found) {
        fprintf(stderr, "%s: %s: error: file `%s` already exists.\n", argv[0], argv[2], argv[4]);
        return;
    }
    
    if ((source = fopen(argv[3], "r")) == NULL) {
        fprintf(stderr, "%s: %s: error: couldn't access `%s`.\n", argv[0], argv[2], argv[3]);
        return;
    }

    entry.parent_id = path_result.parent.payload;
    entry.type = FILE_TYPE;
    strcpy(entry.name, path_result.name);
    entry.payload = import_chain(source);
    fseek(source, 0L, SEEK_END);
    entry.size = (uint64_t)ftell(source);
    entry.time = (uint64_t)time(NULL);
    
    // find empty entry
    for (i = 0; ; i++) {
        entry_t entry_i = rd_entry(i);
        if ((entry_i.parent_id == 0) || (entry_i.parent_id == DELETED_ENTRY))
            break;
    }
    wr_entry(i, entry);
    
    fclose(source);
    if (verbose) fprintf(stdout, "imported file `%s` as `%s`\n", argv[3], argv[4]);
    return;
}

static void export_cmd(int argc, char **argv) {
    FILE *dest;
    
    if (argc < 4) {
        fprintf(stderr, "%s: %s: missing argument: source file.\n", argv[0], argv[2]);
        return;
    }
    if (argc < 5) {
        fprintf(stderr, "%s: %s: missing argument: destination file.\n", argv[0], argv[2]);
        return;
    }

    path_result_t path_result = path_resolver(argv[3], FILE_TYPE);
    
    // check if the file doesn't exist
    if (path_result.not_found) {
        fprintf(stderr, "%s: %s: error: file `%s` not found.\n", argv[0], argv[2], argv[3]);
        return;
    }
    
    if ((dest = fopen(argv[4], "w")) == NULL) {
        fprintf(stderr, "%s: %s: error: couldn't access `%s`.\n", argv[0], argv[2], argv[4]);
        return;
    }

    export_chain(dest, path_result.target);
    
    fclose(dest);
    if (verbose) fprintf(stdout, "exported file `%s` as `%s`\n", argv[3], argv[4]);
    return;
}

static void ls_cmd(int argc, char **argv) {
    uint64_t id;

    if (argc < 4)
        id = ROOT_ID;
    else {
        if (path_resolver(argv[3], DIRECTORY_TYPE).not_found) {
            fprintf(stderr, "%s: %s: error: invalid directory `%s`.\n", argv[0], argv[2], argv[3]);
            return;
        } else
            id = path_resolver(argv[3], DIRECTORY_TYPE).target.payload;
    }

    if (verbose) fprintf(stdout, "  ---- ls ----\n");
    
    for (uint64_t i = 0; rd_entry(i).parent_id; i++) {
        if (rd_entry(i).parent_id != id) continue;
        if (rd_entry(i).type == DIRECTORY_TYPE) fputc('[', stdout);
        fputs(rd_entry(i).name, stdout);
        if (rd_entry(i).type == DIRECTORY_TYPE) fputc(']', stdout);
        fputc('\n', stdout);
    }

    return;
}

static void format_pass1(int argc, char **argv) {

    if (argc <= 3) {
        fprintf(stderr, "%s: error: unspecified block size.\n", argv[0]);
        fclose(image);
        abort();
    }

    if (verbose) fprintf(stdout, "formatting...\n");

    bytesperblock = atoi(argv[3]);

    if ((bytesperblock <= 0) || (bytesperblock % 512)) {
        fprintf(stderr, "%s: error: block size MUST be a multiple of 512.\n", argv[0]);
        fclose(image);
        abort();
    }

    if (imgsize % bytesperblock) {
        fprintf(stderr, "%s: error: image is not block-aligned.\n", argv[0]);
        fclose(image);
        abort();
    }

    blocks = imgsize / bytesperblock;

    // write signature
    fseek(image, 4, SEEK_SET);
    fputs("_ECH_FS_", image);
    // total blocks
    wr_qword(12, blocks);
    // directory size
    wr_qword(20, blocks / 20); // blocks / 20 (roughly 5% of the total)
    // block size
    wr_qword(28, bytesperblock);

    fseek(image, (RESERVED_BLOCKS * bytesperblock), SEEK_SET);
    if (verbose) fprintf(stdout, "zeroing");

    // zero out the rest of the image
    uint8_t *zeroblock = calloc(bytesperblock, 1);
    if (!zeroblock) {
        perror("calloc failure");
        abort();
    }
    for (uint64_t i = (RESERVED_BLOCKS * bytesperblock); i < imgsize; i += bytesperblock) {
        fwrite(zeroblock, bytesperblock, 1, image);
        if (verbose) fputc('.', stdout);
    }
    free(zeroblock);

    if (verbose) fputc('\n', stdout);
    
    return;

}

static void format_pass2(void) {
    // mark reserved blocks
    uint64_t loc = fatstart * bytesperblock;

    for (uint64_t i = 0; i < (RESERVED_BLOCKS + fatsize + dirsize); i++) {
        wr_qword(loc, RESERVED_BLOCK);
        loc += sizeof(uint64_t);
    }

    if (verbose) fprintf(stdout, "format complete!\n");

    return;
}

int main(int argc, char **argv) {

    if ((argc > 1) && (!strcmp(argv[1], "-v"))) {
        verbose = 1;
        argv[1] = argv[0];
        argv++;
        argc--;
    }

    if (argc == 1) {
        fprintf(stderr, "Usage: %s (-v) [image] <action> <args...>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    if ((image = fopen(argv[1], "r+")) == NULL) {
        fprintf(stderr, "%s: error: couldn't access `%s`.\n", argv[0], argv[1]);
        return EXIT_FAILURE;
    }

    fseek(image, 0L, SEEK_END);
    imgsize = (uint64_t)ftell(image);
    rewind(image);

    if ((argc > 2) && (!strcmp(argv[2], "format"))) format_pass1(argc, argv);

    char signature[8] = {0};
    fseek(image, 4, SEEK_SET);
    fread(signature, 8, 1, image);
    if (strncmp(signature, "_ECH_FS_", 8)) {
        fprintf(stderr, "%s: error: echidnaFS signature missing.\n", argv[0]);
        fclose(image);
        return EXIT_FAILURE;
    }
    if (verbose) fprintf(stdout, "echidnaFS signature found\n");
    
    if (verbose) fprintf(stdout, "image size: %" PRIu64 " bytes\n", imgsize);

    bytesperblock = rd_qword(28);
    if (verbose) fprintf(stdout, "bytes per block: %" PRIu64 "\n", BYTES_PER_BLOCK);

    if (imgsize % bytesperblock) {
        fprintf(stderr, "%s: error: image is not block-aligned.\n", argv[0]);
        fclose(image);
        return EXIT_FAILURE;
    }

    blocks = imgsize / bytesperblock;
    
    if (verbose) fprintf(stdout, "block count: %" PRIu64 "\n", blocks);

    if (verbose) fprintf(stdout, "declared block count: %" PRIu64 "\n", rd_qword(12));
    if (rd_qword(12) != blocks) {
        fprintf(stderr, "%s: warning: declared block count mismatch.\n", argv[0]);
    }

    fatsize = (blocks * sizeof(uint64_t)) / bytesperblock;
    if ((blocks * sizeof(uint64_t)) % bytesperblock) fatsize++;    
    if (verbose) fprintf(stdout, "expected allocation table size: %" PRIu64 " blocks\n", fatsize);

    if (verbose) fprintf(stdout, "expected allocation table start: block %" PRIu64 "\n", fatstart);

    dirsize = rd_qword(20);
    if (verbose) fprintf(stdout, "declared directory size: %" PRIu64 " blocks\n", dirsize);

    dirstart = fatstart + fatsize;
    if (verbose) fprintf(stdout, "expected directory start: block %" PRIu64 "\n", dirstart);

    datastart = RESERVED_BLOCKS + fatsize + dirsize;
    if (verbose) fprintf(stdout, "expected reserved blocks: %" PRIu64 "\n", datastart);

    if (verbose) fprintf(stdout, "expected usable blocks: %" PRIu64 "\n", blocks - datastart);

    if (rd_word(510) == 0xaa55) {
        if (verbose) fprintf(stdout, "the image is bootable\n");
    } else {
        if (verbose) fprintf(stdout, "the image is NOT bootable\n");
    }

    if (argc > 2) {
        if (!strcmp(argv[2], "mkdir")) mkdir_cmd(argc, argv);
        else if (!strcmp(argv[2], "ls")) ls_cmd(argc, argv);
        else if (!strcmp(argv[2], "format")) format_pass2();
        else if (!strcmp(argv[2], "import")) import_cmd(argc, argv);
        else if (!strcmp(argv[2], "export")) export_cmd(argc, argv);

        else fprintf(stderr, "%s: error: invalid action: `%s`.\n", argv[0], argv[2]);
    } else
        fprintf(stderr, "%s: no action specified, exiting.\n", argv[0]);

    fclose(image);

    return EXIT_SUCCESS;
}
