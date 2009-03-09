#include <iostream>

using namespace std;

void
swap1(int *i, int *j)
{
    int tmpi;

    tmpi = *i;
    *i = *j;
    *j = tmpi;
}

void
swap2(int &i, int &j)
{
    int tmpi;

    tmpi = i;
    i = j;
    j = tmpi;
}

int
main()
{
    int i = 0, j = 1;

    cout << "i = " << i << "; j = " << j << endl;
    swap1(&i, &j);
    cout << "i = " << i << "; j = " << j << endl;
    swap2(i, j);
    cout << "i = " << i << "; j = " << j << endl;

    return 0;
}
