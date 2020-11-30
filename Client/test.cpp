#include<iostream>
#include<fstream>
using namespace std;
ifstream fin;
int main() {
	string option;
	while (cin >> option) {
		fin.open(option, ios::binary | ios::app);
		fin.seekg(0, ios::end);
		cout << option << " 's length is: " << fin.tellg() << endl;
		fin.close();
	}
	return 0;
}