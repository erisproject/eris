#include <typeinfo>
#include <iostream>



template <class T>
class C1 {
    public:
        C1() {}

        void foo() {
            std::cout << typeid(T).hash_code() << "\n";
        }
};

class C2 {
    public:
        C2() {}

        void m1() {
            std::cout << "Hi!\n";
        }
};

int main() {

    C1<C2> o1;
    o1.foo();
    std::cout << typeid(C2).hash_code();
}
