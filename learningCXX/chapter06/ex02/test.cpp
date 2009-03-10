#include <iostream>

using namespace std;

inline void
reset_var(int &a, int &b, int &c, int &d, int &x, int &i)
{
    a = 1, b = 2, c = 3, d = 5, x = 7, i = 11;
}

int
f(int i, int j)
{
    return i + j;
}

int
main(int argc, char *argv[])
{
    int a, b, c, d, x, i, val;

    // original
    reset_var(a, b, c, d, x, i);
    val = (a = b + c * d << 2 & 8);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (a & 077 != 3);
    cout << "val = " << val << endl;
    
    reset_var(a, b, c, d, x, i);
    val = (a == b || a == c && c < 5);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (c = x != 0);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (0 <= i < 7);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (f(1, 2) + 3);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (a = -1 + + b -- -5);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (a = b == c++);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (a = b = c = 0);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (a - b, c = d);
    cout << "val = " << val << endl << endl;

    // explicit
    reset_var(a, b, c, d, x, i);
    val = (a = (((b + (c * d)) << 2) & 8));
    // caution! 1 + 2 << 3 == (1 + 2) << 3
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (a & (077 != 3));
    cout << "val = " << val << endl;
    
    reset_var(a, b, c, d, x, i);
    val = ((a == b) || ((a == c) && (c < 5)));
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (c = (x != 0));
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = ((0 <= i) < 7);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (f(1, 2) + 3);
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (a = (((-1) + (+(b--))) - 5));
    // caution! the priority of "++" is higher than '-'/'+'.
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (a = (b == (c++)));
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = (a = (b = (c = 0)));
    cout << "val = " << val << endl;

    reset_var(a, b, c, d, x, i);
    val = ((a - b), c = d);
    cout << "val = " << val << endl;

    return 0;
}
