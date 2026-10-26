//懒得分开写了 干脆写在一个文件里了
#include <iostream>
#include <string>
#include <vector>
using namespace std;
void question1();
void question2();
void question3();
void question4();
void question5();
void question6();
void question7();
void question8();
int main() {
	int num = 0;
	while (true) {
		system("cls");
		cout << "输入一个数字：\n0：退出 1：利润计算 2：电影评价 3：回文数判断 4.寻找三位水仙花数\n5.构造数列求和 6.9*9口诀表 7.生兔子问题 8.摘桃子问题" << endl;
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
		case 6:
			question6();
			break;
		case 7:
			question7();
			break;
		case 8:
			question8();
			break;
		default:
			cout << "输入不对，即将退出" << endl;
			return 0;
		}
		system("pause");
	}
	return 0;
}

void question1() {
	cout << "请输入利润：（万元）" << endl;
	double i = 0;
	cin >> i;
	double reward;
	if (i < 0)
	{
		cout << "输入不合法，即将返回..." << endl;
		return;
	}
	if (i <= 10)
		reward = i * 0.1;
	else if (i <= 20)
		reward = 10 * 0.1 + (i - 10) * 0.075;
	else if (i <= 40)
		reward = 10 * 0.1 + 10 * 0.075 + (i - 20) * 0.05;
	else if (i <= 60)
		reward = 10 * 0.1 + 10 * 0.075 + 20 * 0.05 + (i - 40) * 0.03;
	else if (i <= 100)
		reward = 10 * 0.1 + 10 * 0.075 + 20 * 0.05 + 20 * 0.03 + (i - 60) * 0.015;
	else
		reward = 10 * 0.1 + 10 * 0.075 + 20 * 0.05 + 20 * 0.03 + 40 * 0.015 + (i - 100) * 0.01;
	cout << "发放奖金：" << reward << "万元" << endl;
}
void question2() {
	cout << "请输入电影打分：(0~10分)" << endl;
	int i = 0;
	cin >> i;
	if (i < 0 || i>10)
	{
		cout << "输入不合法，即将返回..." << endl;
		return;
	}
	cout << "你认为电影";
	switch (i)
	{
	case 10:case 9:
		cout << "经典" << endl;
		break;
	case 8:case 7:
		cout << "非常好" << endl;
		break;
	case 6:case 5:
		cout << "一般" << endl;
		break;
	default://包括4 3 2 1 0
		cout << "烂片" << endl;
		break;
	}
}
void question3() {
	cout << "请输入一个非负整数：" << endl;
	int i = 0;
	cin >> i;
	if (i < 0)
	{
		cout << "输入不合法，即将返回..." << endl;
		return;
	}
	string numA = to_string(i);
	string numB = numA;
	int lengthA = numA.length();
	for (int m = 0; m < lengthA; m++) {
		numB[lengthA - 1 - m] = numA[m];
	}
	if (numA == numB) {
		cout << "这个数字是回文数！" << endl;
	}
	else {
		cout << "这个数字不是回文数！" << endl;
	}
}
void question4() {
	cout << "水仙花数有以下：" << endl;
	for (int i = 100; i < 1000; ++i) {
		int a =i/100;
		int b =(i-a*100)/10;
		int c =i-a*100-b*10;
		if(a*a*a+b*b*b+c*c*c == i)
		cout <<  a <<  b  <<c << endl;
	}
}
void question5() {
	cout << "请输入正整数a （数列第一项）" << endl;
	unsigned int a = 1;
	cin >> a;
	cout << "请输入正整数n （数列项数）" << endl;
	unsigned int n = 1;
	cin >> n;
	if (a<=0 || n<=0)
	{
		cout << "输入不合法，即将返回..." << endl;
		return;
	}
	unsigned long long num_atm = 0;
	unsigned long long sum = 0;
	cout << "数列belike:";
	for (int i = 0; i < n; ++i) {
		num_atm += a*pow(10, i);
		sum += num_atm;
		cout << num_atm << ",";
	}
	cout << endl << "和为："<<sum << endl;
}
void question6() {
	cout << "9*9口诀表：" << endl;
	for (int f = 1; f < 10; ++f)
		for (int l = f; l < 10; ++l)
			cout << f << "*" << l << "=" << f * l << "  ";
	cout << endl;
}
void question7() {
	vector<int>thismonth;
	thismonth.push_back(1);
	for (int month = 0; month < 20; ++month) {
		for (int i = 0; i < thismonth.size(); ++i)
			if (thismonth[i] >= 3) {
				thismonth.push_back(1);
			}
		cout << "第"<<month+1<<"月兔子总数："<<thismonth.size()*2 << endl;
		for (int i = 0; i < thismonth.size(); ++i)
			++(thismonth[i]);
	}
}
bool isEqual(double a, double b) {
	return abs(a - b) < 1e-9;  // 误差小于 1e-9 就认为相等
}
void question8() {
	//干脆直接枚举 + 验证  最简单
	for (int num = 10;; num++) {
		double rest = num;
		for (int day = 0; day < 9; ++day) {
			rest = (rest / 2) - 1;
		}
		if (isEqual(rest, 1.0)) {
			cout<<"找到了，第一天摘了"<<num<<"个桃子"<<endl;
			return;
		}
		if (num >= 9999) {
			cout<<"出错了 还没找到 即将退出"<<endl;
			return;
		}
	}
}