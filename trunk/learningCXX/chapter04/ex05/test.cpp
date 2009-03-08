#include <iostream>
#include <limits>

using namespace std;

int
main()
{
    cout << int(numeric_limits<char>::min()) << " <= Instance of the type char <= " << int(numeric_limits<char>::max()) << "\n";

    cout << int(numeric_limits<bool>::min()) << " <= Instance of the type bool <= " << int(numeric_limits<bool>::max()) << "\n";

    cout << short(numeric_limits<short>::min()) << " <= Instance of the type short <= " << short(numeric_limits<short>::max()) << "\n";
    cout << int(numeric_limits<int>::min()) << " <= Instance of the type int <= " << int(numeric_limits<int>::max()) << "\n";
    cout << long(numeric_limits<long>::min()) << " <= Instance of the type long <= " << long(numeric_limits<long>::max()) << "\n";

    cout << float(numeric_limits<float>::min()) << " <= Instance of the type float <= " << float(numeric_limits<float>::max()) << "\n";
    cout << double(numeric_limits<double>::min()) << " <= Instance of the type double <= " << double(numeric_limits<double>::max()) << "\n";

    return 0;
}
