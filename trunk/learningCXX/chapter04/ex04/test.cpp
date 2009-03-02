#include <iostream>

using namespace std;

int
main()
{
    char lower_alphas[] = "abcdefghijklmnopqrstuvwxyz";
    char upper_alphas[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char digits[] = "0123456789";

    for (char *p = lower_alphas; *p != '\0'; p++)
        cout << "'" << *p << "' = " << int(*p) << "(" << hex << int(*p) << dec << ")" <<"\n";
    for (char *p = upper_alphas; *p != '\0'; p++)
        cout << "'" << *p << "' = " << int(*p) << "(" << hex << int(*p) << dec << ")" <<"\n";
    for (char *p = digits; *p != '\0'; p++)
        cout << "'" << *p << "' = " << int(*p) << "(" << hex << int(*p) << dec << ")" <<"\n";
    
    return 0;
}
