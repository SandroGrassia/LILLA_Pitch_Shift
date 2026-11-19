#pragma once

class RawFiles
{
public:
    static constexpr int Capacity = 1024;
    static constexpr unsigned NameSize = 64;
    void Scan();
    int Find(const char *name) const;
    const char *Name(int index) const;
    int Count() const;
    bool Truncated() const;
private:
    char names[Capacity][NameSize] = {};
    int count = 0;
    bool truncated = false;
    static int Compare(const void *a, const void *b);
};
