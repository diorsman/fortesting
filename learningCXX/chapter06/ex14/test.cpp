#include <iostream>

using namespace std;

inline void
swap(char *p, char *q)
{
    char t = *p;
    *p = *q;
    *q = t;
}

char *
rev(char *s)
{
    int l = strlen(s);
    register char *p = s, *q = s + l - 1;
    while (p < q)
        swap(p++, q--);
    return s;
}

int
main(int argc, char *argv[])
{
    char str[] = "hello, world!";
    cout << rev(str) << endl;
    return 0;
}
