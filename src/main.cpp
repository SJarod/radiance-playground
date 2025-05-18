#include <iostream>

#include <tracy/Tracy.hpp>

#include "client/application.hpp"

void *operator new(std ::size_t count)
{
    auto ptr = malloc(count);
    TracyAlloc(ptr, count);
    return ptr;
}
void operator delete(void *ptr) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

int main()
{
    Application app;
    while (app.runLoop())
    {
    }

    return 0;
}