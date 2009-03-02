#include <iostream>
#include <time.h>

using namespace std;

struct Date {
    int year;
    int month;
    int day;
};

static Date
input_date(void)
{
    Date d;

    cout << "Input date: ";
    cin >> d.year >> d.month >> d.day;

    return d;
}

static void
output_date(Date &d)
{
    cout << "Today is " << d.year << "/" << d.month << "/" << d.day << "." << endl;
}

static Date
today(void)
{
    time_t t;
    struct tm tm;
    Date d;

    time(&t);
    localtime_r(&t, &tm);
    d.year = 1900 + tm.tm_year;
    d.month = tm.tm_mon + 1;
    d.day = tm.tm_mday;

    return d;
}

int
main()
{
    Date d;

    d = today();
    output_date(d);

    d = input_date();
    output_date(d);
    
    return 0;
}
