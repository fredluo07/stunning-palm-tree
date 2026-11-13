#include <iostream>
using namespace std;
class cat {
private:
	static unsigned int numOfCats;
public:
	cat() {
	}
	static void addcat() {
		numOfCats++;
	}
	static unsigned int getNumOfCats() {
		return numOfCats;
	}


};
unsigned int cat::numOfCats = 0;
int main() {
	cout << "num of cats:" << cat::getNumOfCats() << endl;
	cat::addcat();
	cout << "num of cats:" << cat::getNumOfCats() << endl;
	return 0;
}