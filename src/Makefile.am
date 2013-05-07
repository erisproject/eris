CXXFLAGS = -O4 -std=c++11 -Wall -Werror

bin_PROGRAMS = test lin-dem bundle-test grad-hessian
test_SOURCES = test.cpp Eris.cpp Simulation.cpp Good.cpp consumer/Quadratic.cpp

grad_hessian_SOURCES = grad-hessian.cpp Eris.cpp Simulation.cpp Good.cpp consumer/Quadratic.cpp

lin_dem_SOURCES = lin-dem.cpp Eris.cpp Simulation.cpp Good.cpp consumer/Quadratic.cpp \
				  firm/PriceFirm.cpp

bundle_test_SOURCES = bundle-test.cpp Eris.cpp Simulation.cpp Good.cpp
