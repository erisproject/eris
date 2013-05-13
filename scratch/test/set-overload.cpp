#include <functional>
#include <iostream>

// Trying out a subclass that does something on assignment, but until
// assignment occurs, works transparently as a double with value 0.
//
// The main problem I see with this approach for Bundle is that, since
// accessing and mutating of bundle quantities is likely to be a common task,
// the proxy class is going to be unnecessary overhead; thus I think I prefer
// the set() method.
//

class blah;

class create_on_assign {
    public:
        create_on_assign(blah *b) : b(b) {}
        double& operator= (double n);
        double& operator+= (double n);
        operator double&() const;
    private:
        blah *b;
};

class blah {
    public:
        blah(double y) : y(y) {}
        double operator[] (int z) const {
            std::cout << "const[] operator called\n";
            return y;
        }
        create_on_assign operator[] (int z) {
            std::cout << "create_on_assign operator called\n";
            create_on_assign c(this);
            return c;
        }
        bool assigned = false;
        double y;
};

double& create_on_assign::operator= (double n) {
    b->y = n;
    b->assigned = true;
    return b->y;
}
double& create_on_assign::operator+= (double n) {
    b->y += n;
    b->assigned = true;
    return b->y;
}
create_on_assign::operator double&() const {
    std::cout << "double operator called\n";
    return b->y;
}

int main() {
    blah b(0.0);

#define PRINTME(bbb) std::cout << ".y=" << bbb.y << ", .assigned=" << bbb.assigned << ", [12]=" << bbb[12] << "\n"
    PRINTME(b);
    PRINTME(b);
    b[12] = 42;
    PRINTME(b);
    PRINTME(b);
    b[12]++;
    PRINTME(b);
    const blah bb = b;
    double f = bb[12];
    PRINTME(b);
    PRINTME(bb);
    bb[12] = 23;
}
