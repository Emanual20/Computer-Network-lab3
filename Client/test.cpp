#include<iostream>
#include<fstream>
#include<ctime>
#include<time.h>
#include<Windows.h>
using namespace std;
ifstream fin;
int main() {
	time_t start_t, end_t;
	double diff_t;

	//string option;
	//while (cin >> option) {
	//	fin.open(option, ios::binary | ios::app);
	//	fin.seekg(0, ios::end);
	//	cout << option << " 's length is: " << fin.tellg() << endl;
	//	fin.close();
	//}

	time(&start_t);
	Sleep(5000);
	time(&end_t);
	diff_t = difftime(end_t, start_t);
	cout << start_t << " " << end_t << " " << diff_t << endl;

	return 0;
}