#include <iostream>
#include <ctime>
#include <cstdlib>
using namespace std;

class Polygon {
  protected:
    int width, height;
  public:
    void set_values (int a, int b)
      { width=a; height=b; }
    virtual int area ()
      { return 0; }
};

class Rectangle: public Polygon {
  public:
    int area ()
      { return width * height; }
};

class Triangle: public Polygon {
  public:
    int area ()
      { return (width * height / 2); }
};

int __attribute__((annotate("sensitive"))) abc = 8;
int __attribute__((annotate("sensitive"))) s1 = 8;
int __attribute__((annotate("sensitive"))) s2 = 8;

int main () {
  Rectangle rect;
  Triangle trgl;
  Polygon poly;
  char __attribute__((annotate("sensitive"))) abcd;
  int __attribute__((annotate("sensitive"))) w;
  int __attribute__((annotate("sensitive"))) h;
  int count = 10;
  Polygon * __attribute__((annotate("sensitive"))) ppoly1 = &rect;
  Polygon * __attribute__((annotate("sensitive"))) ppoly2 = &trgl;
  Polygon * ppoly3 = &poly;

 srand((unsigned)time(0));

 for (int i = 0; i < count; i++) {
        w = rand() %100;
	h = rand() %100;
	ppoly1->set_values (4,5);
  	ppoly2->set_values (4,5);
  	ppoly3->set_values (4,5);
  	cout << ppoly1->area() << '\n';
  	cout << ppoly2->area() << '\n';
  	cout << ppoly3->area() << '\n';
  }

  abcd = 'c';
  cout << abc << '\n';
  cout << abcd << '\n';
  return 0; 
}
