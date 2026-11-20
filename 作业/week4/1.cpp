#include <iostream>
using namespace std;
class SimpleCircle {
public:
	SimpleCircle(int r=1) {
		radius = r;
	}
	int const* itsRadius = &radius;
	double cal_c() { return 2 * pi * (*itsRadius); }
	double cal_s() { return pi * (*itsRadius) * (*itsRadius) * 0.5; }
private:
	int radius = 0;
	const double pi = 3.1415926535;
};
int main() {
	int r = 0;
	cout << "请输入半径：" << endl;
	cin >> r;
	SimpleCircle c(r);
	cout << "周长："<<c.cal_c() << " 面积：" << c.cal_s() << endl;
	return 0;
}