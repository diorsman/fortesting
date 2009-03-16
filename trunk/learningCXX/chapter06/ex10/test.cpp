#include <iostream>

using namespace std;

unsigned int
my_strlen(const char *s)
{
    register const char *p = s;
    while (*p != '\0') p++;
    return p - s;
}

char *
my_strcpy(char *dest, const char *src)
{
    register char *p = dest;
    while ((*p++ = *src++) != 0) {};
    return dest;
}

int
my_strcmp(const char *s1, const char *s2)
{
    while (1) {
        int diff = *s1 - *s2;
        if (diff == 0) {
            /* continue */
            if (*s1 != '\0') {
                s1++;
                s2++;
                continue;
            } else {
                return 0;
            }
        } else {
            return diff;
        }
    }
}

int
main(int argc, char *argv[])
{
    char str1[] = "hello, world!";
    char str2[] = "hello, you!";
    cout << "length of str1: " << my_strlen(str1) << endl;
    cout << "length of str2: " << my_strlen(str2) << endl;

    char buf[100];
    my_strcpy(buf, str1);
    cout << "buf is: " << buf << endl;

    cout << "compare str1 and str2: " << my_strcmp(str1, str2) << endl;
    cout << "compare str2 and str1: " << my_strcmp(str2, str1) << endl;
    cout << "compare str1 and str1: " << my_strcmp(str1, str1) << endl;
    
    return 0;
}
