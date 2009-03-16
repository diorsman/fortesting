#include <iostream>

using namespace std;

int
main(int argc, char *argv[])
{
    int i;

    //i = 2 / 0;
    
    i = 1;
    while (i > 0) {
        i++;
    }
    cout << "i = " << i << endl;
    
    i = -1;
    while (i < 0) {
        i--;
    }
    cout << "i = " << i << endl;

    return 0;
}
