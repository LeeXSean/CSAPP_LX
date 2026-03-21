#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// 定义缓存行结构
typedef struct
{
    uint8_t valid;
    uint64_t tag;
    uint32_t LRU_counter;
} cache_line;

// 函数声明
void manipulate(char identifier, uint64_t address, uint32_t size);
void insert(uint32_t set_index, uint64_t tag);
void evict(uint32_t set_index, uint64_t tag);
void update(uint32_t set_index);
void load_trace();

// 全局变量声明
cache_line** cache;
uint32_t S, s, E, b, verbose;
uint32_t hits, misses, evictions;
char* trace_file;
uint8_t M_unique;     // 保证 M 指令只输出一次

// 操作是否命中
void manipulate(char identifier, uint64_t address, uint32_t size)
{ 
    uint32_t set_index = (address >> b) & ((0x1 << s) - 1);
    uint64_t tag = address >> (b + s);

    if (verbose && M_unique)
    {
        printf("%c %lx,%d ", identifier, address, size);
        M_unique = 0;
    }

    // 如果组为空，则未命中，模拟缓存将块导入到缓存中
    if (cache[set_index] == NULL)
    {
        cache[set_index] = (cache_line*)malloc(sizeof(cache_line) * E);
        // 初始化，防止段错误
        for (int i = 0; i < E; i++) 
        {
            cache[set_index][i].valid = 0;
            cache[set_index][i].tag = 0;
            cache[set_index][i].LRU_counter = 0;
        }
        insert(set_index, tag);
    }
    // 如果组不为空，验证有效位和标记位
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
        // 有效位为0，或标记位不匹配
        insert(set_index, tag);
    }
}

// 模拟块导入缓存
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
    // 当前缓存组已满，驱逐最久之前用的行，导入新的行
    evict(set_index, tag);
}

// 模拟缓存组已满，采用LRU驱逐旧行，导入新行
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
    相对更新大小，由于每次都是在组内操作，只需比较组内的LRU_counter大小即可
    针对不同的操作，根据其组索引，只更新其组内的有效行的LRU_counter
    无需全局更新LRU_counter，时间复杂度由O(S * E)降为O(E)
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

// 导入trace文件，行读取，并对每种操作执行manipulate函数
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
            // M 操作是先读取再保存，操作两次
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

// 主函数，包括从命令行读取参数的getopt函数
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
    // 初始化，防止段错误
    for (int i = 0; i < S; i++) cache[i] = NULL;

    load_trace();

    // 释放内存，首先释放cache中存储的指针指向的内存，在释放cache二级指针指向的内容
    for (int i = 0; i < S; i++) free(cache[i]);
    free(cache);

    printSummary(hits, misses, evictions);
    return 0;
}
