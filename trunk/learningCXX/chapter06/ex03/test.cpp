#include <string>
#include <map>
#include <iostream>

using namespace std;

struct values {
    double amount;
    int count;
};

int
main(int argc, char *argv[])
{
    string n;
    double v;
    map<string, values> nvs;
    while (cin >> n >> v) {
        nvs[n].amount += v;
        nvs[n].count++;
    }

    cout << string(40, '*') << endl;
    cout << "name\tamount\tmean" << endl;
    cout << string(40, '*') << endl;
    double t = 0;
    int c = 0;
    typedef map<string, values>::const_iterator MI;
    for (MI i = nvs.begin(); i != nvs.end(); i++) {
        cout << i->first << "\t" << i->second.amount << "\t" << i->second.amount / i->second.count << endl;
        t += i->second.amount;
        c += i->second.count;
    }
    cout << string(40, '*') << endl;
    cout << "total\t" << t << "\t" << t/c << endl;
    cout << string(40, '*') << endl;

    return 0;
}
