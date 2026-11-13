#pragma once
#include <iostream>
using namespace std;
class X;
class Y {
public:
	void g(X* p);
};
class Z {
public:
	void f(X* p);
};
class X {
public:
	friend class Z;
	friend void Y::g(X* p);
	friend void h(X* p);
	int geti();
private:
	int i=0;
};
void h(X* p);