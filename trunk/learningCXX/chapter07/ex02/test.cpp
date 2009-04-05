#include <iostream>

using namespace std;

typedef int (&rifii) (int, int);
//typedef int (*rifii) (int, int);

int
f(int, int)
{
    cout << "f() be called." << endl;
    return 0;
}

int
main(int argc, char *argv[])
{
    rifii r = f;
    r(0, 0);
    
    return 0;
}
