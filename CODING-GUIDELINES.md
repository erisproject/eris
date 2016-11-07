# Eris coding guidelines

This document covers the coding guidelines used in Eris code for consistency.
Not all classes currently conform to this, but all new code should aim to do
so.

This document is neither exhaustive nor absolute.  Minor violations may be
acceptable if they have a good rationale behind them.

These guidelines are inspired by (but differ from) the [Google C++ Style
Guide](http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml)

## Naming conventions for variables/classes/etc.

- Class names should start with a capital letter.  If consisting of multiple
  words, the first letter of each word should be capitalized with no underscore
  between words.  Example: `Awesome`, `MyClass`, but not `someClass` or
  `That_Class`.

- namespaces should be lower-case and usually in the `eris` namespace or a
  namespace nested within `eris` (e.g. `eris::firm`).

- Structs should always be declared in lower case with underscores separating
  words, such as `some_struct` or `blah`.

- Methods should start with a lower-case letter, with upper-case letters used
  to distinguish words and no underscores, such as `meth` or `myMethod`.

- Public member variables should be lower-case, with underscores separating
  words, such as `foo` or `some_value`.

- Private and protected member variables should be lower-case with underscores
  separating words, and followed by an underscore, such as `foo_` or
  `some_value_`.

- Local variables should generally be named in the same way as public member
  variables, but there is considerably more freedom since such variables are
  not part of the API.


## Directory and source filename structure

- Headers for C++ code use a .hpp extension.  Only if a header is pure C code
  may .h be used.

- Header file hierarchy should follow the C++ namespace hierarchy.  For
  example, class eris::ClassName belongs in `eris/ClassName.hpp`;
  eris::firm::Magical belongs in `eris/firm/Magical.hpp`.

- Headers for classes are generally stored in a header file with the same name
  as the class, such as `eris::ClassName` being defined in
  `eris/ClassName.hpp`.  Some exceptions for tightly related classes are
  acceptable: for example, `eris/Bundle.hpp` defines both `Bundle` and
  `BundleNegative` classes, while `eris/Firm.hpp` has both `Firm` and `FirmNoProd`
  classes.

- Header files beginning with a lower-case letter are used for collections of
  small classes or definitions.  For example, the `eris_id_t` typedef is
  defined in eris/types.hpp, and a few small algorithm functions and classes
  are in `eris/algorithms.hpp`.

- Source files associated with headers go under `src/eris`, and should have
  exactly the same name as the header file, but with a `.cpp` extension instead
  of `.hpp`.  (`.c` is acceptable for pure-C classes).


## Documentation

Eris uses doxygen for API documentation.  Rules to follow when adding doxygen
documentation:

- Every public and protected member should be documented in its `.hpp` file
  with doxygen-compatible comments.

- Documentation comments should be useful enough for someone to understand the
  interface and purpose of a function or variable (for public members), and
  useful enough for a subclass writer to make use of the member (for protected
  members).

- Multi-line comments should be formatted using `/** ... */` comments, in the
  following style:

      /** Comment starts here and
       * continues here.
       *
       * More comments.
       *
       * \param blah description of parameter
       */
      void foo(int blah);

- Single-line documentation comments may use either `/** ... */` comments, or
  `/// ...` comments. `/// ...` should *not* be used for multi-line comments.

- Mathematical equations (as opposed to computational equations) should be in
  LaTeX using [doxygen's MathJax
  interface](http://www.stack.nl/~dimitri/doxygen/manual/formulas.html) whenever
  such notation is helpful.  For example, \f$\pi = 3.1415\hdots\f$.

- Namespaces should be documented in an appropriate and logical place.  If
  classes in a given namespace typically have a common base class, the base
  class header should contain the namespace documentation (for example,
  `eris/Firm.hpp` documents the `eris::firm` namespace).

- All documentation should be in the .hpp header files.  The .cpp source files
  should contain code comments as needed to help in following the source code,
  but *documentation* belongs in the header files.


## General code guidelines

### Headers
- Every header should begin with `#pragma once` (rather than an explicit
  include guard definition).

- Eris library code should use angle-bracket includes rather than quoted
  includes, i.e. `#include <path/name.hpp>` instead of `#include
  "path/name.hpp"`.

- Use inline functions/methods only when they are small and frequently called,
  and have relatively simple logic, or are otherwise required (e.g. for
  templated classes or methods)

- Use as few `#include`s as is practical.  Consider using forward declarations
  of classes when feasible (and definitely do so to avoid cyclical
  dependencies).  `#include`s in a header should only be used for code needed
  in the header itself; if the include is only needed for the `.cpp` source
  code, the include belongs in the `.cpp` file.

### Scoping
- Use the narrowest scope possible.  That is, avoid declaring all local
  variables at the top of a function/method/block: instead declare local
  variables as needed in the code.

- Initialize variables during declaration, where feasible.  For example, prefer
  `int answer = 42;` to `int magic; magic = 42;`

### Classes
- Minimize work in constructors.

- Single-argument constructors should usually be declared explicit, to avoid
  implicit type conversion.  If a single-argument constructor is purposely not
  explicit (for example, wrapper classes or copy constructors), it should be
  (briefly) documented.

- Classes should always be declared with the `class` operator; a `struct`
  should only be used as a passive container.

- Avoid private inheritance.  Usually private inheritance is better served by
  storing the private class instance as a private member object instead.
  Exceptions should be carefully noted (for instance, the `eris::noncopyable`
  class is explicitly designed to be privately inherited).

- Don't use operator overloading unless such overloading is reasonably obvious
  and natural to users of the class.  Bundle uses it extensively, since
  operations like adding two bundles together makes immediate sense.  Most
  classes don't have such a natural interpretation.

- Default method parameters are acceptable.  If the default value is a non-zero
  numeric value and is likely to be used by an inheriting class, expose the
  default using a constexpr variable with at least as much scope as the method
  it is used in (i.e.  public for public methods, public or protected for
  protected methods).  For example:

      public:
          static constexpr double default_foo = 3.0;
          virtual void doSomething(double foo = default_foo);

  Don't bother with a `constexpr`, however, when the default is boolean,
  nullptr, or similar.

- Overridden methods should have an explicit "override" identifier,
  particularly to make it easier to update if the base class arguments need to
  change in some way.

### Argument passing
- In general, pass simple types by value, and pass complex types by const
  reference, unless otherwise justified.

### C++ language features
- Eris is C++11 code; all standard C++11 features are acceptable and
  encouraged.

- use `auto` to avoid pointlessly cluttered type names (especially for
  templated type names).

- Avoid direct use of pointers as much as possible, preferring
  `std::shared_ptr`, `std::unique_ptr`, etc.

## Code formatting

- 4-space indent.  No tabs.

- Aim for 100-character wide code.  Some code may exceed this, slightly, if
  wrapping would be ugly.  Documentation comments should always be wrapped at
  100 characters (because any decent editor makes this easy), unless strictly
  needed (for example, for a code example).

- Opening braces (`{`) following unwrapped lines should be at the end of a
  line.  If the opening brace follows a wrapped declaration (for example, a
  complicated method declaration or if statement), the brace may be at the end
  of the last line or on a new line; in the latter case the brace should not be
  indented.

- Closing braces should line up with the first character that started the
  block, if on their own line, and on the same line as the opening brace, if
  not.  For example:
      if (blah) {
          ...
      }
      else if (something complicated that
          requires wrapping)
      { ... }
      else { ... }

- Don't "cuddle" elses: the 'else' keyword should start the line.

- Prefer spaces around operators and comparisons, e.g. `a + b` and `a == b`
  rather than `a+b` and `a==b`, unless doing so is pointlessly verbose.

- Pad the outside, not the inside, of parentheses (EXCEPT for parentheses used
  for a function call, see below).  e.g. `(a + b) * 3` and `if (a >= b) { ...
  }` rather than `( a + b )*3` or `if(a >= b) { ... }` or `if (a >= b){ ... }`.

- Don't-pad-inside takes precedence for nested parentheses, e.g. `((a + b) *
  3)` should be used instead of `( (a + b) * 3)`.

- Never put a space between a function call and the opening parenthesis for its
  arguments.  e.g. `func(a, b)`, not `func (a, b)`.
  - There is one exception to this with operator methods: both `operator=(...)`
    and `operator = (...)` should be considered acceptable: the former follows
    this rule, the latter follows the "space-around-operators-and-comparisons"
    rule, above).

- No spaces around pointer and reference expressions.  e.g. `*p`, `&r`,
  `p->a`, `r.a`.

- Declarations of pointers and references should attach the `*` or `&` to the
  variable, not the type.  e.g. `char *c` not `char* c`.  This is particularly
  important when declaring multiple variables, since the prohibited `char* a,
  b, c` would declare b and c as chars, rather than `char*`'s.

- Functions or methods returning references or pointers should disregard the
  previous rule: that is, write `int& foo() { ... }` instead of `int &foo() {
  ... }`.

- Function/method arguments should either be all on one line, or listed one per
  line, indented, beginning on a new line.  (Both rules may be followed at
  once: all may be on the same line following the method name).  For multi-line
  function declarations, the closing parenthesis may be on the same line as the
  last argument, or on its own line, lined up with either the arguments or the
  opening declaration.  e.g.

      bool f1(int a, double b, double c) {

      bool f2(
          int a, double b, double c) {

      bool f3(
          int a,
          double b,
          double c) {

      bool f4(
          int a,
          double b,
          double c
          ) {
      
      bool f5(
          int a, double b, double c
      ) {

  are all acceptable, while the following are not:

      bool f5(
          int a, double b,
          double c) {

      bool f6(
      int a,
      double b,
      double c) {

- Do not indent code contained by namespaces.

- When all the code in a file is inside a nested namespace, declare the
  namespace on one line and similarly close the namespace on one line with
  multiple closing brackets (spaces between the brackets are optional).  For
  example:
  
      namespace top { namespace nested {
      
      ...
      
      }}

- The public:/protected:/private: keyword may be outdended, such as:

      class SomeClass {
      public:
          SomeClass();
          void someMethod();

  This is not, however, required: the following is also acceptable:

      class SomeClass {
          public:
              SomeClass();
              void someMethod();

