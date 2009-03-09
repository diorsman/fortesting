#include <iostream>

using namespace std;

int
main()
{
    char str[] = "hello";

    for (char *p = str; *p != '\0'; p++)
        cout << *p;
    cout << endl;
    
    for (int n = 0; str[n] != '\0'; n++)
        cout << str[n];
    cout << endl;
    
    return 0;
}
