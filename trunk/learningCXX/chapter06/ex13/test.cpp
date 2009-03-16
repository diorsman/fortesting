#include <iostream>

using namespace std;

char *
cat(const char *s1, const char *s2)
{
    int l1 = strlen(s1);
    int l2 = strlen(s2);
    char *rets = new char[l1 + l2 + 1];
    register char *p = rets; 
    register const char *q = s1;
    while ((*p++ = *q++)) {};
    p = rets + l1;
    q = s2;
    while ((*p++ = *q++)) {};
    return rets;
}

int
main(int argc, char *argv[])
{
    const char *s1 = "hello,";
    const char *s2 = "world!";
    char *c = cat(s1, s2);
    cout << c << endl;
    delete []c;
    return 0;
}
