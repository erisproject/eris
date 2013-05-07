CXXFLAGS = -O4 -std=c++11 -Wall -Werror

_eris_base = Eris.cpp Simulation.cpp Good.cpp Market.cpp

bin_PROGRAMS = test lin-dem bundle-test grad-hessian
test_SOURCES = test.cpp $(_eris_base) consumer/Quadratic.cpp firm/PriceFirm.cpp market/Bertrand.cpp

grad_hessian_SOURCES = grad-hessian.cpp $(_eris_base) consumer/Quadratic.cpp

lin_dem_SOURCES = lin-dem.cpp $(_eris_base) consumer/Quadratic.cpp firm/PriceFirm.cpp

bundle_test_SOURCES = bundle-test.cpp $(_eris_base)
