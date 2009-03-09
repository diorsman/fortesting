#include <iostream>
#include <cstring>

using namespace std;

int
main()
{
    char str[] = "a short string";

    cout << "sizeof(str) = " << sizeof(str) << endl;
    cout << "strlen(\"a short string\") = " << strlen("a short string") << endl;

    return 0;
}
