/**
 * @file buddy.c
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <coin-data/local/impl/buddy.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

// #ifdef __cplusplus
// extern "C" {
// #endif

/**
 * @brief 说明
 * 
 * buddy 有两个基本单位：Block、Page
 * 一个 Block 包含若干个 2 的 n 次幂的 Page
 * 内存分配的基本单位为 Block
*/

// 定义 buddy 系统的常量
const size_t g_page_size     = (0x1 << 12);          // 记录每个 page 的大小，单位：字节
const size_t g_max_block_idx = BUDDY_MAX_BLOCK_NUM;  // 记录最大索引范围

/**
 * @brief 计算 crc32 校验和
 * 
 * @param addr 
 * @param size 
 * @return uint32_t 
 */
uint32_t crc32(void* addr, size_t size)
{
    return 0x55AA;
}

/**
 * @brief 检查 crc32 校验和
 * 
 * @param addr 
 * @param size 
 * @return true 
 * @return false 
 */
bool crc32_check(void* addr, size_t size)
{
    return (((uint8_t*)addr)[size - 3] == 0x55 && ((uint8_t*)addr)[size - 4] == 0xAA);
}

/**
 * @brief 根据等级计算该 block 大小
 * 
 * @param level 
 * @return size_t 
 */
size_t calculate_block_size(const size_t level)
{
    return (0x1 << level) * g_page_size;
}

/**
 * @brief 计算 buddy 的头尺寸
 * 
 * @return size_t 
 */
size_t calculate_buddy_head_size()
{
    // 对齐到 page size
    const size_t struct_size = sizeof(struct buddy_head_t);
    return (struct_size / g_page_size + (((struct_size % g_page_size) == 0) ? 0 : 1)) * g_page_size;
}

/**
 * @brief 判断两个 block 是否为 buddy 关系
 * 
 * @param first 
 * @param second 
 * @return true 
 * @return false 
 */
bool buddy_is_buddy(struct block_head_t* first, struct block_head_t* second)
{
    uint64_t a = (uint64_t)first;
    uint64_t b = (uint64_t)second;
    const uint64_t level = first->level;

    if(first->level != second->level)
    {
        return false;
    }

    if(first->is_first && !second->is_first)
    {
        return false;
    }

    if(first->host_pid != second->host_pid)
    {
        return false;
    }

    if(first->memory_idx != second->memory_idx)
    {
        return false;
    }

    if((a > b) && ((a - b) == (0x1 << level) * g_page_size))
    {
        return true;
    }
    if((a < b) && ((b - a) == (0x1 << level) * g_page_size))
    {
        return true;
    }
    
    return false;
}

/**
 * @brief 计算块的头信息所需的空间大小
 * 
 * @return size_t 
 */
size_t buddy_calculate_block_head_size()
{
    return sizeof(struct block_head_t);
}

/**
 * @brief 计算创建一个 level 等级的 buddy system 所需的内存块大小
 * 
 * @param level 
 * @return size_t 
 */
size_t buddy_calculate_memory_size(const size_t level)
{
    const size_t memory_size = calculate_block_size(level);
    return memory_size;
}

/**
 * @brief 根据内存大小计算内存块所属等级
 * 
 * @param memory_size 
 * @return size_t 
 */
size_t calculate_block_level(size_t memory_size)
{
    size_t block_level = 0;
    if (memory_size < g_page_size)
    {
        return -1;
    }

    for (block_level = 0; block_level < 32; block_level++)
    {
        if (memory_size < ((0x1 << block_level) * g_page_size))
        {
            break;
        }
    }

    return (block_level - 1);
}

/**
 * @brief 根据内存大小计算内存块所属等级
 * 
 * @param memory_size 内存大小，单位：字节
 * @return size_t 所属等级
 */
size_t calculate_which_level(size_t memory_size)
{
    size_t block_level = 0;

    for (block_level = 0; block_level < g_max_block_idx; block_level++)
    {
        if (memory_size <= ((0x1 << block_level) * g_page_size))
        {
            break;
        }
    }

    return (block_level);
}

/**
 * @brief 将一个 block 初始化为 free 状态
 * 
 * @param block 
 * @param previous 
 * @param next 
 * @param level 
 * @param is_first 
 * @return int 
 */
int init_free_block(
    struct block_head_t* block, 
    struct block_head_t* previous, 
    struct block_head_t* next, 
    const size_t level,
    const uint32_t is_first
    )
{
    if(block == NULL)
    {
        return 0;
    }

    block->size = 0;
    block->is_free = true;
    block->previous = previous;
    block->next = next;
    block->level = level;
    block->is_first = is_first;
    block->crc = 0;
    block->crc = crc32(block, buddy_calculate_block_head_size());

    return 0;
}

/**
 * @brief 将一个 block 初始化为 used 状态
 * 
 * @param block 
 * @param level 
 * @return int 
 */
int init_used_block(struct block_head_t* block, const size_t level)
{

    if(block == NULL)
    {
        return 0;
    }

    block->size = 0;
    block->is_free = false;
    block->previous = NULL;
    block->next = NULL;
    block->crc = 0;
    block->crc = crc32(block, buddy_calculate_block_head_size());

    return 0;
}

/**
 * @brief 向 block 列表中插入元素
 *        插入规则：block 地址由小到大插入
 * 
 * @param block_list 
 * @param block 
 * @return int 
 */
int buddy_insert_blcok(struct block_head_t** block_list, struct block_head_t* block)
{
    struct block_head_t* insert_block = (*block_list);

    // 如果 block_list 为空列表，则直接插入
    if(*block_list == NULL)
    {
        *block_list = block;
        return 0;
    }

    // 如果 block_list 不为空，则遍历链表，找到合适的位置插入
    for(insert_block = (*block_list); insert_block->next != NULL; insert_block = insert_block->next)
    {
        if(block < insert_block)
        {
            break;
        }
    }

    // 插入元素
    if(block < insert_block)
    {
        struct block_head_t* previous = insert_block->previous;
        init_free_block(previous, previous->previous, block, previous->level, previous->is_first);
        init_free_block(block, previous, insert_block, block->level, block->is_first);
        init_free_block(insert_block, block, insert_block->next, insert_block->level, insert_block->is_first);
    }
    // 将 block 插入到链表尾部
    else
    {
        init_free_block(insert_block, insert_block->previous, block, insert_block->level, insert_block->is_first);
        init_free_block(block, insert_block, NULL, block->level, block->is_first);
    }

    return 0;
}

/**
 * @brief 从 buddy 的一个链表中移除一个 block
 * 
 * @param block_list 
 * @param block 
 * @return int 
 */
int buddy_remove_block(struct block_head_t** block_list, struct block_head_t* block)
{
    struct block_head_t* previous = block->previous;
    struct block_head_t* next = block->next;

    // 如果是头部节点，那么迁移
    if(block == *block_list)
    {
        *block_list = block->next;
    }

    if(previous != NULL)
    {
        init_free_block(previous, previous->previous, next, previous->level, previous->is_first);
    }
    if(next != NULL)
    {
        init_free_block(next, previous, next->next, next->level, next->is_first);
    }

    return 0;
}

/**
 * @brief 初始化 buddy system
 * 
 * @param addr 
 * @param size 
 * @return int 
 */
int buddy_init_system(void *addr, const size_t size)
{
    if(!addr)
    {
        return -1;
    }
    // 根据地址计算
    struct buddy_head_t *buddy = (struct buddy_head_t*)addr;
    buddy->total_size = size;
    buddy->head_size = calculate_buddy_head_size();
    buddy->memory_size = size - buddy->head_size;
    buddy->memory_idx = 0;

    for(size_t i = 0; i < g_max_block_idx; i++)
    {
        buddy->block_list[i] = NULL;
    }

    // 初始化共享锁
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&buddy->lock, &attr);

    pthread_mutex_unlock(&buddy->lock);

    return 0;
}

/**
 * @brief 挂接内存块到 buddy 系统中
 * 
 * @param addr 
 * @param size 
 * @return int 
 */
int buddy_attch_memory(struct buddy_head_t* buddy, void *addr, const size_t size)
{
    if(!buddy || !addr)
    {
        return -1;
    }

    pthread_mutex_lock(&buddy->lock);

    // 计算该内存所处 block 等级
    size_t level = calculate_block_level(size);

    // 初始化内存块，并插入对应的等级
    struct block_head_t *block_obj = (struct block_head_t *)addr;

    // 初始化 block head 信息
    block_obj->host_pid = getpid();
    buddy->memory_idx += 1;
    block_obj->memory_idx = buddy->memory_idx;
    init_free_block(block_obj, NULL, NULL, level, 1);

    // 插入到 buddy 系统
    buddy_insert_blcok(&buddy->block_list[level], block_obj);

    pthread_mutex_unlock(&buddy->lock);

    return 0;
}

/**
 * @brief 通过 buddy system 申请一块内存空间
 * 
 * @param buddy 
 * @param size 
 * @return void* 
 */
void* buddy_malloc(struct buddy_head_t *buddy, const size_t size)
{
    pthread_mutex_lock(&buddy->lock);

    // 计算所需要分配的内存块等级
    const size_t level = calculate_which_level(size + buddy_calculate_block_head_size());

    // 如果不存在则检查高等级 block 是否存在空闲块，如果存在则逐级拆分到该内存块，并返回拆分后的空闲块
    size_t level_idx = level;
    void* malloc_addr = NULL;

    // 找到有效的 block
    do
    {
        /* code */
        if(buddy->block_list[level_idx])
        {
            break;
        }

        level_idx += 1;

    } while (level_idx < g_max_block_idx);

    // 逐步拆分 block
    for(size_t idx = level_idx; 
        buddy->block_list[idx] != NULL && idx > level; 
        idx--)
    {
        // 取出当前 block 的首个元素
        struct block_head_t* take_head = buddy->block_list[idx];
        buddy->block_list[idx] = take_head->next;

        if(buddy->block_list[idx] != NULL)
        {

            init_free_block(buddy->block_list[idx], NULL, 
                (buddy->block_list[idx] == NULL) ? NULL : buddy->block_list[idx]->next,
                idx, buddy->block_list[idx]->is_first);

        }

        take_head->next = NULL;
        take_head->previous = NULL;

        // 拆分这个 block 为次一级 level 的 block
        struct block_head_t* block_first = take_head;
        struct block_head_t* block_second = (struct block_head_t*)(((uint64_t)take_head) + calculate_block_size(idx - 1));
        
        // 记录身份标识信息
        block_second->host_pid   = block_first->host_pid;
        block_second->memory_idx = block_first->memory_idx;

        init_free_block(block_second, block_first, NULL, idx - 1, 1);

        // 将拆分后的 block 插入对应等级的 block
        struct block_head_t** block_tail = &buddy->block_list[idx - 1];
        while(((*block_tail) != NULL))
        {
            (*block_tail) = (*block_tail)->next;
        }

        (*block_tail) = block_first;

        init_free_block(*block_tail, (*block_tail)->previous, block_second, idx - 1, 0);
    }

    // 检查对应等级是否存在空闲的内存块，如果存在则将首个块返回
    if(buddy->block_list[level])
    {
        struct block_head_t *ret = buddy->block_list[level];

        buddy->block_list[level] = ret->next;
        if(buddy->block_list[level] != NULL)
        {
            init_free_block(buddy->block_list[level], NULL, buddy->block_list[level]->next, buddy->block_list[level]->level, buddy->block_list[level]->is_first);
        }

        init_used_block(ret, level);
        ret->size = size;

        malloc_addr = (void*)( (uint8_t*)(ret) + buddy_calculate_block_head_size() );
    }

    pthread_mutex_unlock(&buddy->lock);

    return malloc_addr;
}

/**
 * @brief 释放 buddy system 申请的内存空间
 * 
 * @param buddy 
 * @param addr 
 * @return int 
 */
int buddy_free(struct buddy_head_t *buddy, const void* addr)
{
    pthread_mutex_lock(&buddy->lock);

    struct block_head_t* head = (struct block_head_t*)((uint8_t*)addr - buddy_calculate_block_head_size());

    // 检查状态
    if(head->is_free)
    {
        fprintf(stderr, "buddy_free: addr %p double free\n", addr);
        abort();
    }

    // 检查校验和
    if(!crc32_check(head, sizeof(struct block_head_t)))
    {
        perror("buddy");
        abort();
    }

    while (head != NULL)
    {
        // 检查校验和，如果错误则放弃
        if (!crc32_check(head, sizeof(struct block_head_t)))
        {
            perror("buddy");
            pthread_mutex_unlock(&buddy->lock);
            abort();
        }

        // 遍历对应 level 下的 block，找到合适的插入点
        struct block_head_t *insert_point = buddy->block_list[head->level];

        if(insert_point == NULL)
        {
            init_free_block(head, NULL, NULL, head->level, head->is_first);
            buddy->block_list[head->level] = head;
            break;
        }

        for (insert_point; insert_point != NULL; insert_point = insert_point->next)
        {

            // 检查是否存在合并关系
            if (buddy_is_buddy(head, insert_point))
            {
                // 可以合并，将 insert_point 从当前 level 中移除
                if(buddy_remove_block(&buddy->block_list[insert_point->level], insert_point) != 0)
                {
                    fprintf(stderr, "buddy: remove block failed, buddy_free:%d\n", __LINE__);
                    abort();
                }

                // 计算新地址
                struct block_head_t* addr = head < insert_point ? head : insert_point;
                // 重新初始化 addr 并进行合并
                init_free_block(addr, NULL, NULL, addr->level + 1, addr->is_first);
                head = addr;
                break;
            }
            // 找到次序节点则在此插入
            else if (head < insert_point)
            {
                // 执行插入动作
                struct block_head_t *previous = insert_point->previous;

                init_free_block(head, previous, insert_point, head->level, head->is_first);
                init_free_block(insert_point, head, insert_point->next, insert_point->level, insert_point->is_first);
                if (previous == NULL)
                {
                    buddy->block_list[head->level] = head;
                }
                else
                {
                    init_free_block(previous, previous->previous, head, previous->level, previous->is_first);
                }
                head = NULL;
                break;
            }
            // 查找到最后一个元素，此时将节点插入尾部
            else if (insert_point->next == NULL)
            {
                init_free_block(insert_point, insert_point->previous, head, insert_point->level, insert_point->is_first);
                init_free_block(head, insert_point, NULL, head->level, head->is_first);
                head = NULL;
                break;
            }

        }

    }

    pthread_mutex_unlock(&buddy->lock);

    return 0;
}

/**
 * @brief 输出 buddy system 的内存块情况
 * 
 * @param buddy 
 */
void dump_buddy(struct buddy_head_t *buddy)
{
    pthread_mutex_lock(&buddy->lock);
    printf("============================================================\n");
    printf("size head: %ld, mem: %ld, total: %ld, page size: %ld\n", buddy->head_size, buddy->memory_size, buddy->total_size, g_page_size);
    printf("addr head: %lx, mem: %lx\n", (uint64_t)buddy, (uint64_t)buddy + buddy->head_size);
    for(size_t idx = 0; idx < g_max_block_idx; idx++)
    {

        printf("level %02ld[%016lx]:", idx, (0x1 << idx) * g_page_size);
        for(struct block_head_t* b = buddy->block_list[idx]; b != NULL; b = b->next)
        {
            printf("[0x%0lx, %ld] ", (uint64_t)b, b->previous != NULL ? (uint64_t)b - (uint64_t)b->previous : 0);
        }
        printf("\n");
    }
    pthread_mutex_unlock(&buddy->lock);
}

void buddy_memory_check(void* addr, const size_t size)
{
    size_t level = calculate_block_level(size);
    // 对各个内存等级进行访问检查
    printf("addr: %p, size: %ld(%lx)\n", addr, size, size);
    for(size_t l = 0; l < level; l++)
    {
        size_t step = (0x01 << l);
        size_t idx = 0;
        size_t total_size = step * g_page_size;
        while(total_size * idx < size)
        {
            struct block_head_t* b = static_cast<struct block_head_t*>(addr + (uint64_t)(total_size * idx));
            
            if(b + sizeof(struct block_head_t) > (addr + size))
            {
                break;
            }
            
            printf("addr: %p, size: %ld(%lx), ", addr, size, size);
            printf("check level: %ld, step: %ld, ", l, step);
            printf("check addr: %p\n", b);
            init_free_block(b, NULL, NULL, l, 0);
            
            idx++;
        }
    }
}

// #ifdef __cplusplus
// }
// #endif
