#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

typedef struct // cache line
{
    int valid;     // 有效位    0 or 1
    int tag;       // 标记位
    int timestamp; // 时间戳  LRU
} line;

typedef struct // cache set
{
    line *lines;
} set;

typedef struct // cache
{
    set *sets; // Number of sets
    int b;     // Block size 2^b bytes
    int E;     // Associativity (number of lines per set)
    int s;     // Number of set index bits (S = 2^s is the number of sets)
} CacheStruct;

CacheStruct cache;

int verbose = 0; //  0 or 1

int hit_count = 0;
int miss_count = 0;
int eviction_count = 0;

void init_cache(CacheStruct *cache, int sets, int associativity, int block_size);
void parase_trace_file(FILE *fp);
void update_cache(unsigned int address, int size);
void update(int set_index, int line_index, int tag);
int get_index(int tag, int set_index);
int is_full(int set_index);
int find_LRU(int set_index);

//  Initialize cache
void init_cache(CacheStruct *cache, int sets, int associativity, int block_size)
{
    cache->b = block_size;
    cache->E = associativity;
    cache->s = sets;

    cache->sets = (set *)malloc(sizeof(set) * sets);
    for (int i = 0; i < (1 << sets); i++)
    {
        cache->sets[i].lines = (line *)malloc(sizeof(line) * associativity);
        for (int j = 0; j < associativity; j++)
        {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag = -1;
            cache->sets[i].lines[j].timestamp = 0;
        }
    }
}

void parase_trace_file(FILE *fp)
{
    char operation;   // L, S, M
    unsigned address; // 0400d7d4
    int size;         // 8

    while (fscanf(fp, " %c %x,%d", &operation, &address, &size) > 0)
    {
        printf("%c %x,%d", operation, address, size);
        switch (operation)
        {
        case 'L':
            update_cache(address, size);
            printf("\n");
            break;
        case 'S':
            update_cache(address, size);
            printf("\n");
            break;
        case 'M':
            update_cache(address, size);
            update_cache(address, size);
            printf("\n");
            break;
        default:
            break;
        }
    }
}

void update_cache(unsigned address, int size)
{
    int tag = address >> (cache.s + cache.b);

    int set_index = (address >> cache.b) & ((1 << cache.s) - 1);
    // int block_offset = address & block_mask;
    // int block_mask = (1 << cache.b) - 1;
    // printf("\n%d %d %d\n", tag, block_offset, set_index);

    int index = get_index(tag, set_index);

    if (index != -1) // hit
    {
        printf(" hit");
        hit_count++;
        update(set_index, index, tag);
    }
    else // miss
    {
        miss_count++;
        printf(" miss");
        int full_index = is_full(set_index);
        if (full_index == -1) // full
        {
            printf(" eviction");
            eviction_count++;
            int LRU_index = find_LRU(set_index);
            update(set_index, LRU_index, tag);
        }
        else
        {
            update(set_index, full_index, tag);
        }

        // update_timestamp(set_index, is_full(set_index));
    }
}

void update(int set_index, int line_index, int tag)
{
    cache.sets[set_index].lines[line_index].valid = 1;
    cache.sets[set_index].lines[line_index].tag = tag;

    for (int i = 0; i < cache.E; i++)
    {
        if (cache.sets[set_index].lines[i].valid == 1)
            cache.sets[set_index].lines[i].timestamp++;
    }

    cache.sets[set_index].lines[line_index].timestamp = 0;
}

int find_LRU(int set_index)
{
    int max = 0;
    int max_index = 0;
    for (int i = 0; i < cache.E; i++)
    {
        if (cache.sets[set_index].lines[i].timestamp > max)
        {
            max = cache.sets[set_index].lines[i].timestamp;
            max_index = i;
        }
    }

    return max_index;
}

// return index of line
int get_index(int tag, int set_index)
{
    for (int i = 0; i < cache.E; i++)
    {
        if (cache.sets[set_index].lines[i].tag == tag && cache.sets[set_index].lines[i].valid == 1)
        {
            return i;
        }
    }

    return -1;
}

int is_full(int set_index)
{
    for (int i = 0; i < cache.E; i++)
    {
        if (cache.sets[set_index].lines[i].valid == 0)
            return i;
    }
    return -1;
}

int main(int argc, char *argv[])
{
    //./csim-ref -v -s 4 -E 1 -b 4 -t traces/yi.trace

    int s = 0;
    int E = 0;
    int b = 0;
    char *trace_file = ""; //  traces/yi.trace

    int c;
    while ((c = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (c)
        {
        case 's':
            s = atoi(optarg);
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
        case 'v':
            verbose = 1;
            printf("verbose: %d\n", verbose);
            break;
        case 'h':
            printf("Usage: %s -s <sets> -E <blocks> -b <lines> -t <trace_file>\n", argv[0]);
            exit(0);
            break;
        default:
            fprintf(stderr, "Usage: %s -s <sets> -E <blocks> -b <lines> -t <trace_file>\n", argv[0]);
            return 1;
        }
    }

    // printf("sets: %d, associativity: %d, block_size: %d, trace_file: %s\n", sets, associativity, block_size, trace_file);

    //  Initialize cache
    // int cache_size = sets * associativity * block_size;
    init_cache(&cache, s, E, b);

    //  Read trace file
    FILE *fp = fopen(trace_file, "r");
    if (fp == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    parase_trace_file(fp);

    fclose(fp);

    printSummary(hit_count, miss_count, eviction_count);

    return 0;
}
