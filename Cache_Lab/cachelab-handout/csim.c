#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Cache line structure
typedef struct
{
    uint8_t valid;
    uint64_t tag;
    uint32_t LRU_counter;
} cache_line;

// Function declarations
void manipulate(char identifier, uint64_t address, uint32_t size);
void insert(uint32_t set_index, uint64_t tag);
void evict(uint32_t set_index, uint64_t tag);
void update(uint32_t set_index);
void load_trace();

// Global variable declarations
cache_line** cache;
uint32_t S, s, E, b, verbose;
uint32_t hits, misses, evictions;
char* trace_file;
uint8_t M_unique;     // Ensures M instruction output is printed only once

// Check if the operation hits the cache
void manipulate(char identifier, uint64_t address, uint32_t size)
{ 
    uint32_t set_index = (address >> b) & ((0x1 << s) - 1);
    uint64_t tag = address >> (b + s);

    if (verbose && M_unique)
    {
        printf("%c %lx,%d ", identifier, address, size);
        M_unique = 0;
    }

    // If the set is empty, cache miss — simulate loading the block into cache
    if (cache[set_index] == NULL)
    {
        cache[set_index] = (cache_line*)malloc(sizeof(cache_line) * E);
        // Initialize to prevent segfault
        for (int i = 0; i < E; i++) 
        {
            cache[set_index][i].valid = 0;
            cache[set_index][i].tag = 0;
            cache[set_index][i].LRU_counter = 0;
        }
        insert(set_index, tag);
    }
    // If the set is not empty, check valid bit and tag
    else
    {
        for (int i = 0; i < E; i++)
        {
            if (cache[set_index][i].valid && cache[set_index][i].tag == tag)
            {
                hits++;
                cache[set_index][i].LRU_counter = 1;
                update(set_index);
                if (verbose)
                {
                    printf("hit ");
                }
                return;
            }
        }
        // Valid bit is 0, or tag mismatch — cache miss
        insert(set_index, tag);
    }
}

// Simulate loading a block into cache
void insert(uint32_t set_index, uint64_t tag)
{
    misses++;
    if (verbose)
    {
        printf("miss ");
    }

    cache_line* set = cache[set_index];
    for (int i = 0; i < E; i++)
    {
        if (!set[i].valid)
        {
            set[i].valid = 1;
            set[i].tag = tag;
            set[i].LRU_counter = 1;
            update(set_index);
            return;
        }
    }
    // Cache set is full, evict the LRU line and load the new one
    evict(set_index, tag);
}

// Simulate a full cache set: evict the LRU line and load the new line
void evict(uint32_t set_index, uint64_t tag)
{
    uint32_t replace_index = 0;
    uint32_t max_LRU;

    evictions++;
    if (verbose)
    {
        printf("eviction ");
    }

    cache_line* set = cache[set_index];
    max_LRU = set[0].LRU_counter;
    for (int i = 0; i < E; i++)
    {
        if (set[i].LRU_counter > max_LRU)
        {
            max_LRU = set[i].LRU_counter;
            replace_index = i;
        }
    }
    set[replace_index].valid = 1;
    set[replace_index].tag = tag;
    set[replace_index].LRU_counter = 1;
    update(set_index);
}

/*
    Relative LRU update: since all operations are within a single set, only compare
    LRU_counter values within that set. Update only the valid lines in the set indexed
    by the current operation — no global update needed. Reduces time complexity from
    O(S * E) to O(E).
*/
void update(uint32_t set_index)
{
    cache_line* set = cache[set_index];
    for (int j = 0; j < E; j++)
    {
        if (set[j].valid)
        {
            set[j].LRU_counter++;
        }
    }
}

// Load trace file, read line by line, and call manipulate for each operation
void load_trace()
{
    FILE* pFile = fopen(trace_file, "r");
    if (!pFile) return;

    char identifier;
    uint64_t address;
    uint32_t size;

    while(fscanf(pFile, " %c %lx,%d", &identifier, &address, &size) > 0)
    {
        M_unique = 1;
        switch (identifier)
        {
        case 'I':
            break;
        case 'M':
            // M operation is a read followed by a write — process twice
            manipulate(identifier, address, size);
            manipulate(identifier, address, size);
            break;
        case 'L':
        case 'S':
            manipulate(identifier, address, size);
            break;
        }
        if (verbose && identifier != 'I') printf("\n");
    }
    fclose(pFile);
}

// Main function — uses getopt to parse command-line arguments
int main(int argc, char** argv)
{
    int opt;
    while(-1 != (opt = getopt(argc, argv, "hvs:E:b:t:")))
    {
        switch (opt)
        {
        case 's':
            s = atoi(optarg);
            S = 0x1 << s;
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        case 'h':
            printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n"
                "Options:\n"
                "  -h Print this help message.\n"
                "  -v Optional verbose flag. \n"
                "  -s <num> Number of set index bits.\n"
                "  -E <num> Number of lines per set.\n"
                "  -b <num> Number of block offset bits.\n"
                "  -t <file> Trace file.\n"
                "\n"
                "Examples:\n"
                " linux> ./%s -s 4 -E 1 -b 4 -t traces/yi.trace\n"
                " linux> ./%s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", 
            argv[0], argv[0], argv[0]);
            exit(0);
        case 'v':
            verbose = 1;
            break;
        }
    }

    cache = (cache_line**)malloc(sizeof(cache_line*) * S);
    // Initialize to prevent segfault
    for (int i = 0; i < S; i++) cache[i] = NULL;

    load_trace();

    // Free memory: first free each set's array, then free the outer pointer array
    for (int i = 0; i < S; i++) free(cache[i]);
    free(cache);

    printSummary(hits, misses, evictions);
    return 0;
}
