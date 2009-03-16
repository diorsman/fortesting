#include <cstring>
#include <iostream>

using namespace std;

inline char 
itoc(int i)
{
    static char digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    return digits[i];
}

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

char *
my_itoa(int i, char b[])
{
    if (i == 0) {
        b[0] = '0';
        b[1] = '\0';
        return b;
    }
    // negative?
    bool neg = (i < 0);
    if (neg)
        i = -i;
    // convert
    int idx = 0;
    while (i > 0) {
        int m = i % 10;
        b[idx++] = itoc(m);
        i /= 10;
    }
    if (neg)
        b[idx++] = '-';
    b[idx] = '\0';
    // reverse
    rev(b);
    return b;
}

int
main(int argc, char *argv[])
{
    int i = 12345;
    char buf[100];
    cout << "my_itoa(" << i << ") = " << my_itoa(i, buf) << endl;
    cout << "my_itoa(" << -i << ") = " << my_itoa(-i, buf) << endl;
    return 0;
}
