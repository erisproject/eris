#include "market/Bertrand.hpp"

namespace eris { namespace market {

    Bertrand::Bertrand(Bundle output, Bundle priceUnit, bool randomize)
        : Market(output, priceUnit), randomize(randomize) {}

    double Bertrand::price(double q) const {
        // Keys are prices, values are fraction of q*_output available at that price
        std::map<double, double> priceAggQ;
        // Keys are prices, values are maps of <firm -> fraction of q*_output available>
        std::map<double, std::map<eris_id_t, double>> priceFirm;

        double aggQuantity = 0.0;
        Bundle qBundle = q * _output;

        for (auto f : suppliers) {
            SharedMember<firm::PriceFirm> firm = f.second;
            // Make sure the "price" object in this market can pay for the units the firm wants
            if (_price.covers(firm->price())) {
                double productivity = firm->canProduceAny(qBundle);
                if (productivity > 0) {
                    aggQuantity += productivity;
                    // First we need the market output produced per firm output bundle unit, then we
                    // multiple that by the firm's price per market price.  This is because one firm
                    // could have (price=2,output=2), while another has (price=3,output=3) and another
                    // has (price=1,output=1); all three have the same per-unit price.
                    // Intuitively we want:
                    //     (market.output/market.price) / (firm.output/firm.price)
                    // but we have to actually compute it as:
                    //     (market.outout/firm.output) * (firm.price / market.price)
                    // because those divisions are lossy when Bundles aren't scaled versions of each
                    // other (see Bundle.hpp's description of Bundle division)
                    double firmPrice = (_output / firm->output()) * (firm->price() / _price);
                    priceAggQ[firmPrice] += productivity;
                    priceFirm[firmPrice][firm] = productivity;
                }
            }
        }

        // aggQuantity is in terms of the requested output bundle; if it doesn't add up to at least
        // 1, the entire market cannot supply the requested quantity at any price.
        if (aggQuantity < 1.0) { return -1.0; }

        double finalPrice = 0.0;
        double needQ = 1.0;
        for (auto pf : priceAggQ) {
            double price = pf.first;
            double quantity = pf.second;
            std::cout << "price=" << price << ", quantity=" << quantity << "\n";
            if (quantity >= needQ) {
                finalPrice += price*needQ;
                break;
            }
            else {
                finalPrice += price*quantity;
                needQ -= quantity;
            }
        }

        return finalPrice;
    }

    double Bertrand::buy(double q, double pMax, const Bundle &assets) {
        throw output_NA(); // FIXME TODO
    }

    void Bertrand::addFirm(SharedMember<Firm> f) {
        if (!dynamic_cast<firm::PriceFirm*>(f.ptr.get()))
            throw std::invalid_argument("Firm passed to Bertrand.addFirm(...) is not a PriceFirm instance");
        Market::addFirm(f);
    }

} }

