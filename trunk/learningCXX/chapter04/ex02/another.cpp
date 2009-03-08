#include <iostream>
#include <complex>

using namespace std;

// my
extern char ch;
extern string s;
extern int count;
const double pi = 3.1415926;    // see 9.2
//extern const double pi;    // wrong
int error_number;

extern const char *name;    // see 9.2
extern const char *season[];

struct Date;
int day(Date);
double sqrt(double d) { return d; }
template<class T> T abs(T a);

typedef complex<short> Point;   // see 9.2
struct User { char *name; };
enum Beer { Carlsberg, Tuborg, Thor };
namespace NS {}

void 
f()
{
    cout << pi << endl;
}
