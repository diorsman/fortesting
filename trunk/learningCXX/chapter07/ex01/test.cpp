#include <iostream>

using namespace std;

void
f(char *cp, int &i)
{
}

typedef void (*fp_t)(char*, int&);

fp_t fp = f;

void
f2(fp_t fp)
{
}

fp_t
f3()
{
    return 0;
}

fp_t
f4(fp_t fp)
{
    return fp;
}

int
main(int argc, char *argv[])
{
    return 0;
}
