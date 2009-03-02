#include <iostream>

using namespace std;

int
main()
{
    char c = '0';
    int i = 0;
    char *cp = &c;
    int *ip = &i;

    cout << (void *)cp << " " << (void *)ip << endl;

    return 0;
}
