/**
 * @file crc32.h
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <stdint.h>

namespace coin
{
uint32_t calculate_crc32(uint32_t seed, const unsigned char *buf, size_t len);
}
