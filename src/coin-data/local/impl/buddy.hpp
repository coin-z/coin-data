/**
 * @file buddy.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef __BUDDY_H__
#define __BUDDY_H__

#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>


// #ifdef __cplusplus
// extern "C" {
// #endif


#ifndef BUDDY_MAX_BLOCK_NUM
#define BUDDY_MAX_BLOCK_NUM (32)
#endif

#pragma pack(4)

/**
 * @brief 每个 block 中使用该结构体记录内存状态信息
 * 
 */
struct block_head_t
{
    struct block_head_t *previous;
    struct block_head_t *next;

    size_t   size;
    size_t   level;
    size_t   host_pid;
    size_t   memory_idx;
    uint32_t is_free;
    uint32_t is_first;
    uint32_t crc;
};

/**
 * @brief 用来记录 buddy 管理的内存块的信息
 * 
 */
struct buddy_head_t
{

    size_t total_size;
    size_t head_size;
    size_t memory_size;
    size_t memory_idx;
    pthread_mutex_t lock;
    struct block_head_t *block_list[BUDDY_MAX_BLOCK_NUM];

};

#pragma pack()

size_t buddy_calculate_memory_size(const size_t level);
int    buddy_init_system(void* addr, const size_t size);
int    buddy_attch_memory(struct buddy_head_t* buddy, void* addr, const size_t size);
void*  buddy_malloc(struct buddy_head_t *buddy, const size_t size);
int    buddy_free(struct buddy_head_t *buddy, const void* addr);
void   dump_buddy(struct buddy_head_t *buddy);

void   buddy_memory_check(void* addr, const size_t size);

// #ifdef __cplusplus
// }
// #endif

#endif // __BUDDY_H__
