#include <windows.h>
#include <iostream>
using namespace std;
void question1();
void question2();
void question3();
void question4();
void question5();

int main() {
	int num = 0;
	while (true) {
		system("cls");
		cout << "输入一个数字：\n0：退出 1：除法函数 2：质数判断 3：最大公约数和最小公倍数问题 4.求Fiboonacci级数 5.计算x的y次幂" << endl;
		cin >> num;
		system("cls");
		switch (num) {
		case 0:
			cout << "即将退出" << endl;
			return 0;
		case 1:
			question1();
			break;
		case 2:
			question2();
			break;
		case 3:
			question3();
			break;
		case 4:
			question4();
			break;
		case 5:
			question5();
			break;
		default:
			cout << "输入不对，即将退出" << endl;
			return 0;
		}
		system("pause");
	}
	return 0;
}
short int func(unsigned short int a, unsigned short int b) {
	if (b == 0)
		return -1;
	return a / b;
}
void question1() {
	unsigned short int a, b;
	cout << "输入第一个参数（非负数）：" << endl;
	cin >> a ;
	cout << "输入第二个参数（非负数）：" << endl;
	cin >> b;
	cout <<"函数返回结果为："<< func(a, b) << endl;
}
bool check(unsigned int a) {//检查是否是质数
	if (a < 2)
		return false;
	if (a == 2)
		return true;
	if (a % 2 == 0)
		return false;
	int b = sqrt(a);
	for (int i = 3; i <= b; i=i+2) {
		if (a % i == 0)
			return false;
	}
	return true;
}
void question2() {
	unsigned int a;
	cout << "输入要判断的数（非负数）：" << endl;
	cin >> a;
	if (check(a))
		cout << "这个数是质数" << endl;
	else
		cout << "这个数不是质数" << endl;
}
void question3() {
	unsigned short int a, b;
	cout << "输入第一个数（正整数）：" << endl;
	cin >> a;
	cout << "输入第二个数（正整数）：" << endl;
	cin >> b;
	if (a == 0 || b == 0) {
		cout << "输入不合法，即将退出" << endl;
		return;
	}
	if (a > b)
		swap(a, b);
	int resulta=1;//最大公约数
	for (int i = 1;i<=a; ++i) {
		if (a % i == 0 && b % i == 0)
			resulta = i;
	}
	for (int i = b;i<=a*b; i++) {
		if (i % a == 0 && i % b == 0) {
			cout << "最大公约数："<<resulta << "最小公倍数"<<i << endl;
			return;
		}
	}
}
unsigned long long fiboo(unsigned short idx) {//idx>2
	if (idx <= 2)
		return 1;
	return fiboo(idx - 1) + fiboo(idx - 2);
}
void question4() {
	unsigned short a=0;
	cout << "输入index：（大于等于1的整数）" << endl;
	cin >> a;
	if (a == 0) {
		cout << "输入不合法，即将退出" << endl;
		return;
	}

	cout << "数列第"<<a<<"项为" << fiboo(a) << endl;
}
int getPower(int x, int y) {
	if (y < 0)
		return 0;
	if (y == 0)
		return 1;
	return getPower(x,y-1)*x;
}
double getPower(double x, int y) {
	if (y == 0)
		return 1;
	if (y > 0)
		return getPower(x, y - 1) * x;
	return getPower(x, y + 1) / x;
}
void question5() {
	int a, m;
	double b;
	cout << "输入一个整数a作为底数：" << endl;
	cin >> a;
	cout << "输入一个实数b作为底数：" << endl;
	cin >> b;
	cout << "输入一个整数m作为指数：" << endl;
	cin >> m;
	if ((a == 0||b==0) && m == 0) {
		cout << "出错！无法计算0的0次" << endl;
		return;
	}
	cout << "结果分别为："<<getPower(a,m)<<","<<getPower(b,m) << endl;
}
