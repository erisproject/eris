#include <eris/serialize/serializer.hpp>

namespace eris { namespace serialize {

std::ostream& operator<<(std::ostream &out, const serializer_base &s) {
    s.store_to(out);
    return out;
}

std::istream& operator>>(std::istream &in, serializer_base &s) {
    s.load_from(in);
    return in;
}

std::istream& operator>>(std::istream &in, serializer_base &&s) {
    s.load_from(in);
    return in;
}

}}
