#include "RawFiles.h"
#include <SerialFlash.h>
#include <cstdlib>
#include <cstring>
#include <strings.h>

int RawFiles::Compare(const void *a, const void *b)
{
    const char *left = static_cast<const char *>(a);
    const char *right = static_cast<const char *>(b);
    char *end_left;
    char *end_right;
    const unsigned long number_left = std::strtoul(left, &end_left, 10);
    const unsigned long number_right = std::strtoul(right, &end_right, 10);
    if (end_left != left && end_right != right && strcasecmp(end_left, ".raw") == 0 && strcasecmp(end_right, ".raw") == 0 && number_left != number_right)
    {
        return number_left < number_right ? -1 : 1;
    }
    // Numeric filenames precede nonnumeric names, preserving a consistent total order.
    const bool numeric_left = end_left != left && strcasecmp(end_left, ".raw") == 0;
    const bool numeric_right = end_right != right && strcasecmp(end_right, ".raw") == 0;
    if (numeric_left != numeric_right)
    {
        return numeric_left ? -1 : 1;
    }
    return std::strcmp(left, right);
}

void RawFiles::Scan()
{
    count = 0;
    truncated = false;
    SerialFlash.opendir();
    char name[256];
    uint32_t bytes;
    while (SerialFlash.readdir(name, sizeof(name), bytes))
    {
        const size_t length = std::strlen(name);
        if (length < 5 || strcasecmp(name + length - 4, ".raw") != 0 || bytes < 4 || (bytes & 1u) != 0)
        {
            continue;
        }
        if (length >= NameSize || count == Capacity)
        {
            truncated = true;
            continue;
        }
        std::strcpy(names[count++], name);
    }
    std::qsort(names, count, NameSize, Compare);
}

int RawFiles::Find(const char *name) const
{
    for (int i = 0; i < count; ++i)
    {
        if (std::strcmp(names[i], name) == 0)
        {
            return i;
        }
    }
    return -1;
}

const char *RawFiles::Name(int index) const { return names[index]; }
int RawFiles::Count() const { return count; }
bool RawFiles::Truncated() const { return truncated; }
