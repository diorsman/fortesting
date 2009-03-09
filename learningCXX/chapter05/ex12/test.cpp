#include <string>
#include <iostream>
#include <cstring>

using namespace std;

int
main()
{
    string str = "xabaacbaxabb";
    int count = 0;
    string::size_type loc = str.find("ab");
    while (loc != string::npos) {
        count++;
        loc = str.find("ab", loc + 2);
    }
    cout << "count = " << count << endl;
    
    const char *s = str.c_str();
    count = 0;
    const char *p = strstr(s, "ab");
    while (p != NULL) {
        count++;
        p = strstr(p + 2, "ab");
    }
    cout << "count = " << count << endl;

    return 0;
}
