# Design notes

## Parser

_Parser_ is a function that takes a string (in future will be generalized to
input source) and returns a value. Simplified signature:

    template <typename ResultType>
    using Parser = std::function<ResultType(std::string)>;

### Continuations

In order to allow flexible chaining, a parser does not return the result by
value, instead it accepts a continuation, i.e. a function that accepts the
result value as parameter. This function can be called 0, 1, or more times.
Updated signature:

    template <typename ResultType>
    using Continuation = std::function<void(ResultType)>;

    template <typename ResultType>
    using Parser = std::function<void(std::string, Continuation<ResultType>>;



### Parameterized parsers

In order to allow chaining parsers, a notion of _parser function_ or
_parameterized parser_ is needed. It is a function taking an argument, and
returning a parser. Simplified signature:

    template <typename ParamType, typename ResultType>
    using ParameterizedParser = std::function<Parser<ResultType>(ParamType)>;

Regular parser can be considered to be a parameterized parser that takes void
as argument:

    template <typename ResultType>
    using Parser = ParameterizedParser<void, ResultType>;

But this is a circular definition, so instead `ParameterizedParser` should
become the basic type, and should be renamed to simply `Parser`:

    template <typename ResultType>
    BasicParser = std::function<void(std::string, Continuation<ResultType>)>;

    template<typename ParamType, typename ResultType>
    using Parser = std::function<BasicParser<ResultType>(ParamType)>;

### Monadic bind

Chaining of 2 parameterized parsers can now work as follows:

    template <typename A, typename B, typename C>
    Parser<A, C> ChainParsers(Parser<A, B> p1, Parser<B, C> p2) {
      // Return a Parser<A, C>, i.e. a function that takes A, and returns BasicParser<C>.
      return [p1, p2] (A a) -> BasicParser<C> {
        // An A is now given, give back a BasicParser<C>, i.e. a function taking string and a Continuation<C>.
        return [a, p1, p2] (std::string input, Continuation<C> action) -> void {
          // Apply p1 to a to get a parser, and pass in input and a Continuation<B>.
          p1(a)(input, [p2, input, action] (B b) -> void {
            // The continuation gets b, applies p2 to it, and passes in input and final action.
            p2(b)(input, action);
          }
        }
      }
    }
    
There is a simplification here: The same input is passed both to `p1` and `p2`.
It is expected that `p1` will consume some input. The assumption is that the
there is an underlying shared state that will be modified in-place.

### Reader monad

The in-place modification of input is different from pure monadic approach, in
which a `Reader` monad is applied. The differences are as follows:
* In reader world, all code is implicitly a function of input/context; in C++
  the input is passed in explicitly.
* In reader world, input can be modified, and the result is used to spin off
  a sub-reader; in C++ input is modified in place.
* In reader world, the reader monad is embedded into continuation monad, which
  allows for a DFS-like changing and undoing changes to context. 
