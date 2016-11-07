# Eris - agent-based economic modelling library

## Description

Eris is a C++ library designed for simulating “economies“ consisting of agents
who follow programmable rules, with the intention of deriving complex
macro-level behaviour that is an emergent property of agent interaction.  This
project provides no implementation itself, but is intended to be used as the
basis of an agent-based model.  See the
[creativity](https://git.imaginary.ca/eris/creativity/) project for a working
example of a moderately complex ABM built using eris.

This project was motivated by the inadequacy of mathematical models, which
often impose severe constraints on agents in the name of tractability.

At its core, this library imposes few constraints on the behaviour of agents.
Individuals agents can, for instance, be programmed to be ultra-rational, or
can be “dumb” in the sense of following only simple rules of thumb.  The
behaviours and means of interacting are left to packages developed using this
library.

Beyond the core, the library offers many specialized implementations that may
be used as appropriate, but can equally well be ignored, enhanced, or replaced
with alternative implementations.

The library name, Eris, is the name of the Greek goddess of chaos.

## Documentation

### Library overview

See the [Eris overview](OVERVIEW.md) documentation for an introductory overview
to how the library works.

### API documentation

The API documentation can be built from the source code, as described in the
Compiling section below, and is also available at
https://imaginary.ca/eris/api/.  A good place to start is the
[eris::Simulation](https://imaginary.ca/eris/api/classeris_1_1Simulation.html)
class, and the classes in the
[eris::interopt](https://imaginary.ca/eris/api/namespaceeris_1_1interopt.html)
and
[eris::intraopt](https://imaginary.ca/eris/api/namespaceeris_1_1intraopt.html)
namespaces.

See also the various [classes of the creativity
ABM](https://imaginary.ca/eris/creativity/annotated.html) for a working example
of a moderately complex agent-based model.

## Installing (Debian-based systems)

Regularly updated library, header, and documentation packages are available for
amd64 systems by adding one of the following lines to /etc/apt/sources.list:

    deb https://imaginary.ca/debian jessie main
    deb https://imaginary.ca/debian sid main
    deb https://imaginary.ca/ubuntu trusty main
    deb https://imaginary.ca/ubuntu wily main

The first is for debian stable; the second for debian testing and unstable; the
last two are for Ubuntu releases.

The required key signature for package verification can be installed using:

    curl -s https://imaginary.ca/public.gpg | sudo apt-key add -

You generally want to install the liberis-dev package and possibly the eris-doc
package.

These packages are built from the "debian", "debian-jessie", "ubuntu-trusty",
and "ubuntu-wily" branches of the eris package repository.

## Compiling

### Requirements

- [boost](http://www.boost.org/); only the Math component is needed.
- [Eigen](http://eigen.tuxfamily.org/)
- A C++ compiler supporting the C++11 standard, such as
  [clang](http://clang.llvm.org/) (3.3+) or [g++](https://gcc.gnu.org/) (4.9+)

If you wish to build the HTML API documentation, additional requirements are:
- [doxygen](http://www.stack.nl/~dimitri/doxygen/)
- [graphviz](http://www.graphviz.org)
- [mathjax](https://www.mathjax.org)

### Building

To compile on a unix-like system, create a new build directory somewhere, then
from this directory run:

    cmake /path/to/eris-src
    make -j4

You can install directly to the system (usually under /usr/local) using:

    make install

## License

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

## Author

Jason Rhinelander -- [e-mail](mailto:jason@imaginary.ca), [homepage](https://imaginary.ca)

