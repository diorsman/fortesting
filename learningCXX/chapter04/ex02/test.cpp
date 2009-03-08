#include <iostream>
#include <complex>

using namespace std;

// original
char ch;
string s;
int count = 1;
const double pi = 3.1415926;
extern int error_number;

const char *name = "Njal";
const char *season[] = { "spring", "summer", "fall", "winter" };

struct Date { int d, m, y; };
int day(Date *p) { return p->d; }
double sqrt(double);
template<class T> T abs(T a) { return a < 0 ? -a : a; }

typedef complex<short> Point;
struct User;
enum Beer { Carlsberg, Tuborg, Thor };
namespace NS { int a; }

void f();

int
main()
{
    f();
    return 0;
}
