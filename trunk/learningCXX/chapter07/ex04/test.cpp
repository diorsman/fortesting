#include <fstream>
#include <iostream>

using namespace std;

int
main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        ifstream fin(argv[i]);
        char buf[8192];
        do {
            fin.read(buf, sizeof(buf));
            cout << buf;
        } while (fin.good());
    }

    return 0;
}
