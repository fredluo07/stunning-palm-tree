#include <iostream>
#include <string>
using namespace std;
unsigned int countletter(const char *s){
	unsigned int num = 0;
	for (int i = 0; s[i]; ++i) {
		if (s[i] != ' ' && s[i] != ',' && s[i] != '.')
			++num;
	}
	return num;
}
int main() {
	string str;
	cout << "请输入一句英文：" << endl;
	getline(cin,str);
	cout << "字母数量："<<countletter(str.c_str()) << endl;
	return 0;
}