#include <iostream>

using namespace std;

const char *month_name[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};
const int month_day[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

struct month_info {
    const char *name;
    const int day;
};

month_info month_infos[] = {
    {"Jan", 31},
    {"Feb", 28},
    {"Mar", 31},
    {"Apr", 30},
    {"May", 31},
    {"Jun", 30},
    {"Jul", 31},
    {"Aug", 31},
    {"Sep", 30},
    {"Oct", 31},
    {"Nov", 30},
    {"Dec", 31},
};

int
main()
{
    for (unsigned int i = 0; i < sizeof(month_name) / sizeof(char *); i++) 
        cout << month_name[i] << ": " << month_day[i] << endl;
    cout << endl;
    for (unsigned int i = 0; i < sizeof(month_infos) / sizeof(month_info); i++) 
        cout << month_infos[i].name << ": " << month_infos[i].day << endl;
    return 0;
}
