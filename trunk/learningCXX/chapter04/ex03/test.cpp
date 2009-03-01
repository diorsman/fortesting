#include <iostream>

using namespace std;

int
main()
{
    enum e1 { e1_first, e1_second };
    enum e2 { e2_first, e2_second = 0XFFFFFFFFFFLLU };

    cout << "sizeof(char) = " << sizeof(char) << "\n";
    cout << "sizeof(bool) = " << sizeof(bool) << "\n";
    cout << "sizeof(int) = " << sizeof(int) << "\n";
    cout << "sizeof(double) = " << sizeof(double) << "\n";

    cout << "sizeof(short) = " << sizeof(short) << "\n";
    cout << "sizeof(long) = " << sizeof(long) << "\n";
    cout << "sizeof(long long) = " << sizeof(long long) << "\n";
    cout << "sizeof(float) = " << sizeof(float) << "\n";
    cout << "sizeof(long double) = " << sizeof(long double) << "\n";

    cout << "sizeof(char *) = " << sizeof(char *) << "\n";
    cout << "sizeof(bool *) = " << sizeof(bool *) << "\n";
    cout << "sizeof(int *) = " << sizeof(int *) << "\n";
    cout << "sizeof(double *) = " << sizeof(double *) << "\n";
    cout << "sizeof(void *) = " << sizeof(void *) << "\n";

    cout << "sizeof(e1) = " << sizeof(e1) << "\n";
    cout << "sizeof(e2) = " << sizeof(e2) << "\n";

    return 0;
}
