#include <iostream>

using namespace std;

const char *month_name[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

void
print(const char *str)
{
    cout << str << endl;
}

int
main()
{
    for (unsigned int i = 0; i < sizeof(month_name) / sizeof(const char *); i++)
        print(month_name[i]);
    return 0;
}
