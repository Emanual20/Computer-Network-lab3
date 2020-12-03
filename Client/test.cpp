#include<iostream>
#include<fstream>
#include<ctime>
#include<time.h>
#include<Windows.h>
#include<string>
using namespace std;
ifstream fin;

char buffer[] = "hello";

int main() {
	
	string s = buffer;
	cout << s << endl;
	strcpy(buffer,"what");
	cout << s << endl;

	return 0;
}