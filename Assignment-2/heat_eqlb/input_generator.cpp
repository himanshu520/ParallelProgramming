#include<bits/stdc++.h>
#define MAX_INPUT_FILES 10
using namespace std;

const double PI = acos(-1);

int main() {

    int array_size[] = {2, 4, 6}, no_of_iterations[] = {50, 100, 200};
    int input_file_no = 1;
    srand(time(0));
    
    for(auto arr_sz : array_size)
        for(auto iter : no_of_iterations) {
            const char * input_file_name = ("input" + to_string(input_file_no++) + ".txt").c_str();
            freopen(input_file_name, "w", stdout);
            cout << arr_sz << "\n";
            cout << iter << "\n";
            for(int i = 0; i < arr_sz; i++)
                cout << fixed << setprecision(5) << (rand() % 1000) * sin((rand() % 1000) * PI / 1000) << "\n";
        }
    
    return 0;
}