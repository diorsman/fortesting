#include <iostream>
#include <cctype>

using namespace std;

int
my_atoi(const char *s)
{
    // get radix
    int radix = 10; // default radix
    const char *p = s;
    if (s[0] == '0') {
        // 8 or 16
        if (s[1] == 'x' || s[1] == 'X') {
            radix = 16;
            p = s + 2;
        } else {
            radix = 8;
            p = s + 1;
        }
    }
    // get number
    int ret = 0;
    while (isdigit(*p)) {
        switch (*p) {
        case '0':
            ret = radix * ret;
            break;
        case '1':
            ret = radix * ret + 1;
            break;
        case '2':
            ret = radix * ret + 2;
            break;
        case '3':
            ret = radix * ret + 3;
            break;
        case '4':
            ret = radix * ret + 4;
            break;
        case '5':
            ret = radix * ret + 5;
            break;
        case '6':
            ret = radix * ret + 6;
            break;
        case '7':
            ret = radix * ret + 7;
            break;
        case '8':
            ret = radix * ret + 8;
            break;
        case '9':
            ret = radix * ret + 9;
            break;
        case 'a':
        case 'A':
            ret = radix * ret + 10;
            break;
        case 'b':
        case 'B':
            ret = radix * ret + 11;
            break;
        case 'c':
        case 'C':
            ret = radix * ret + 12;
            break;
        case 'd':
        case 'D':
            ret = radix * ret + 13;
            break;
        case 'e':
        case 'E':
            ret = radix * ret + 14;
            break;
        case 'f':
        case 'F':
            ret = radix * ret + 15;
            break;
        }
        p++;
    }
    return ret;
}

int
main(int argc, char *argv[])
{
    char *d10 = "123";
    cout << "my_atoi(" << d10 << ") = " << my_atoi(d10) << "(" << 123 << ")" << endl;
    char *d16 = "0x123";
    cout << "my_atoi(" << d16 << ") = " << my_atoi(d16) << "(" << 0x123 << ")" << endl;
    char *d8 = "0123";
    cout << "my_atoi(" << d8 << ") = " << my_atoi(d8) << "(" << 0123 << ")" << endl;

    return 0;
}
