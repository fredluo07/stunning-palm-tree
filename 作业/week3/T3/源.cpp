#include "标头.h"
int main() {
	X x;
	Y y;
	Z z;
	cout<<"i初始值："<<x.geti()<<endl;
	y.g(&x);
	cout << "g(X*)调用后i的值：" << x.geti() << endl;
	z.f(&x);
	cout << "f(X*)调用后i的值：" << x.geti() << endl;
	h(&x);
	cout << "h(X*)调用后i的值：" << x.geti() << endl;
	return 0;
}