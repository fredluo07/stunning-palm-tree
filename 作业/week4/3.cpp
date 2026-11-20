#include <iostream>
#include <string>
using namespace std;
void reverse(string& s) {
	size_t len = s.length();
	if (len > 1) {
		string sub = s.substr(1, len - 2);
		reverse(sub);
		s = s[len - 1] + sub + s[0];
	}
}
int main() {
	string str;
	cout << "请输入一个字符串：" << endl;
	getline(cin,str);
	reverse(str);
	cout << "反转后："<<str << endl;
	return 0;
}