#pragma once
#include <vector>
#include "agent/consumer/Consumer.hpp"
#include "Bundle.hpp"
#include "types.hpp"

class Quadratic : Consumer {
    public:
        // Initialize with empty
        Quadratic();
        // Initialize with map of good -> [coefs]
        Quadratic(std::map<eris_id_t, std::vector<double>> coef);

        void setCoefs(eris_id_t gid, std::vector<double> c);

        void setCoefs(eris_id_t gid, std::initializer_list<double> c);

        double utility(Bundle b);
        // FIXME: what about a way to update A/B/g?
    protected:
        std::map<eris_id_t, std::vector<double>> coef;
};
