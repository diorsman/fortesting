#include <iostream>

using namespace std;

int
main()
{
    char c = '0';
    char *cp = &c;

    int ia[10] = {0, 1, 2};
    int * const & ir = ia;
    
    // {...} can be used to initialize an array, but it's not an array.
    char *sa[] = {"string1", "string2", "string3"};
    char **sap = sa;

    char **sap2 = &cp;

    const int ci = 0;

    const int *cip = &ci;

    int i = 0;
    int * const icp = &i;

    // are that uninitialized entrys zero?
    for (int n = 0; n < 10; n++)
        cout << ia[n] << "\n";

    return 0;
}
