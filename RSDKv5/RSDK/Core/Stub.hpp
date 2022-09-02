#ifndef GARBAGE_H
#define GARBAGE_H

#if defined(_arch_dreamcast)

#include <stdio.h>
#include <string>

template <typename T, size_t N>
constexpr size_t arrayLength(T (&)[N])
{
    return N;
}

template <typename... Args>
inline std::string format(const std::string& format, Args... args)
{
    if (format.length() < 1)
    {
        return {};
    }

    int characterCount = snprintf(nullptr, 0, format.c_str(), args...);

    if (characterCount < 1)
    {
        return {};
    }

    const size_t bufferLength = characterCount + 1;
    auto* buffer = new char[bufferLength];

    characterCount = snprintf(buffer, bufferLength, format.c_str(), args...);

    if (characterCount < 1)
    {
        delete[] buffer;
        return {};
    }

    std::string str(buffer, characterCount);
    delete[] buffer;
    return str;
}

std::string sizeSuffix(size_t value);

size_t getTheoreticalFreeMemory();

void printTheoreticalFreeMemory_(const char* file, size_t line, const char* function);

#define printTheoreticalFreeMemory() printTheoreticalFreeMemory_(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#define printHere() printf("here! %s:%u -> %s\n", __FILE__, static_cast<size_t>(__LINE__), __PRETTY_FUNCTION__)
#define DC_STUB() printf("STUB: %s:%u -> %s\n", __FILE__, static_cast<size_t>(__LINE__), __PRETTY_FUNCTION__)

#else  // defined(_arch_dreamcast)

#define printHere()

#endif

#endif  // GARBAGE_H