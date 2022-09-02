#if defined(_arch_dreamcast)

#include <malloc.h>
#include <stdio.h>

#include <cmath>
#include <string>

#include "Stub.hpp"

static constexpr size_t memorySize = 16ul * 1024ul * 1024ul;
static constexpr const char* suffixes[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };

std::string sizeSuffix(size_t value)
{
    if (value < 1024)
    {
        return format("%u %s", value, suffixes[0]);
    }

    constexpr size_t maxIndex = arrayLength(suffixes) - 1;
    size_t i = 0;
    auto dValue = static_cast<double>(value);
    while (floor(dValue / static_cast<double>(1024)) >= 1 && i < maxIndex)
    {
        dValue /= static_cast<double>(1024);
        i++;
    }

    return format("%.2f %s", dValue, suffixes[i]);
}

size_t getTheoreticalFreeMemory()
{
    auto info = mallinfo();
    return memorySize - info.arena;
}

void printTheoreticalFreeMemory_(const char* file, const size_t line, const char* function)
{
    const size_t f = getTheoreticalFreeMemory();
    printf("----------------\n%s:%u\n%s\ntheoretical free memory: %u (%s)\n----------------\n",
           file, line, function,
           f, sizeSuffix(f).c_str());
}

#endif  // defined(_arch_dreamcast)
