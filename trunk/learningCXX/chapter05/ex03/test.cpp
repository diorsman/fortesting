#include <iostream>

using namespace std;

typedef unsigned char uc;
typedef const unsigned char cuc;
typedef int *ip;
typedef char **cpp;
typedef char *const cap;
typedef int *ipa7[7];
typedef int **const ipa7p;
typedef int *iaa[8][7];

int
main()
{
    cout << "sizeof(uc) = " << sizeof(uc) << endl;
    cout << "sizeof(cuc) = " << sizeof(cuc) << endl;
    cout << "sizeof(ip) = " << sizeof(ip) << endl;
    cout << "sizeof(cpp) = " << sizeof(cpp) << endl;
    cout << "sizeof(cap) = " << sizeof(cap) << endl;
    cout << "sizeof(ipa7) = " << sizeof(ipa7) << endl;
    cout << "sizeof(ipa7p) = " << sizeof(ipa7p) << endl;
    cout << "sizeof(iaa) = " << sizeof(iaa) << endl;
    return 0;
}
