#include <iostream>

using namespace std;

int
main()
{
    char c = '0';
    char *cp = &c;

    int ia[10] = {0, 1, 2};
    //int *const iap = ia;
    int * const & iar = ia;
    
    // {...} can be used to initialize an array, but it's not an array.
    const char *sa[] = {"string1", "string2", "string3"};
    const char **const sap = sa;
    //*sap = "hello";   // right
    //**sap = '\0';     // wrong

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
