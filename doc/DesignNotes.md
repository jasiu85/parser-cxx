# Design notes

## Parser

This is a combination of a few ideas:
* PEG grammars
* functional parser combinators

These ideas might be simplified somewhat, as full generality might not be
required.

_Parser_ is a function that takes a string (in future will be generalized to
input source) and returns a value. Simplified signature:

    Parser<T> = std::function<T(std::string)>

### Continuations

The notion of a single result has to be generalized in order to allow powerful
combinators. Instead of returning a value, it will be passed to a continuation:

    ParserOutput<T> = std::function<void(T)>
    
    Parser<T> = std::function<void(std::string, ParserOutput<T>)> 

A parser can call the output function 0, 1, or more times. This means,
respectively: failed to parse, unambiguous parse, ambiguous parse.

### Input manipulation

What is passed to the parser is the remaining input. Various combinators will
manipulate the input in various way. In particular, backtracking has to be able
to return to a previous input state. This is easily achieved by making a copy
of the input, but a `std::string` is not efficient at that, so instead use
`std::string_view`:

    ParserInput = std::string_view

    Parser<T> = std::function<void(ParserInput, ParserOutput<T>)>

### Basic combinators

Now basic combinators can be implemented as outlined below. This is pseudo-code.
Figuring out proper passing style (value, rvalue ref, lvalue ref) and ownership
(move-only, shared) will be figured out later.

#### ParseEof

    ParseEof = [] (ParseInput input, ParseOutput<void> output) {
      if (input.empty()) {
        output();
      }
    }

#### ParseString

    ParseString(string s) = [s] (ParseInput input, ParseOutput<string> output) {
      if (consume_prefix(input, s)) {
        output(s);
      }
    }

Note: `consume_prefix` modifies `input`. Most likely `output` is a parser, so
this in-place modification of `input` is needed for correct sequencing of
parsers.

#### ParseSequence

    ParseSequence(Parser<T1> p1, Parser<T2> p2) = [p1, p2] (ParseInput input, ParseOutput<pair<T1, T2>> output) {
      p1(input, [p2, output](T1 t1) {
        p2(input, [t1, output](T2 t2) {
          output(make_pair(t1, t2));
        });
      });
    }

Here the assumption that `input` is modified in-place is utilized. This is
_applicative_ in style, i.e. `p2` is independent of `p1`. Also, the results are
simply returned as a pair. In practice more elaborate ways of combining results
will be needed:

    ParseSequence(Parser<T1> p1, Parser<T2> p2, std::function<R(T1, T2)> f) = [p1, p2, f] (ParseInput input, ParseOutput<R> output) {
      p1(input, [p2, f, output](T1 t1) {
        p2(input, [t1, f, output](T2 t2) {
          output(f(t1, t2));
        });
      });
    }
   
It will be also nice to generalize to more than 2 parsers, but that's going to
be somewhat hard to spell out.

#### ParseChoice

    ParseChoice(Parser<T> p1, Parser<T> p2) = [p1, p2] (ParseInput input, ParseOutput<T> output) {
        p1(copy(input), output);
        p2(copy(input), output);
    }

Here backtracking is achieved by making copy of the input (as opposed to
sequencing, which exploits in-place input modifications). Also important is the
fact that `output` can be called more than once.

The choice is only between 2 parses. Easily generalizes:

    ParseChoice(vector<Parser<T>> parsers) = [parsers](ParseInput input, ParseOutput<T> output) {
      for (auto p : parsers) {
        p(copy(input), output);
      }
    }

The choice is symmetrical, i.e. each branch will be tested. This is likely
to result in an exponential parse time for grammars that have a lot of
ambiguity in them. A biased combinator can be used to fix this:

    ParseChoiceBiased(vector<Parser<T>> parsers) = [parsers] (ParseInput input, ParseOutput<T> output) {
      bool parsed = false;
      for (auto p : parsers) {
        p(copy(input), [&parsed, output](T t) {
          parsed = true;
          output(t);
        }); 
        if (parsed) {
          break;
        }
      }
    }

#### Monadic bind

Monads exist at multiple levels here:
1. CPS is used to control backtracking.
1. Reader monad could be used to manage input, but in C++ in-place modifications
   probably work better.
1. Monading sequencing.

The last point is not yet covered. The existing sequencing combinator is
`applicative`, i.e. the behavior of the 2nd parser does not depend on result
from the 1st one. This should not be needed for CFG languages, or PEG parsing.
But it will be provided nevertheless, so that the suite of combinators is
complete.

    ParseSequenceBind(Parser<T1> p1, std::function<Parser<T2>(T1)> p2) = [p1, p2] (ParseInput input, ParseOutput<T2> output) {
      p1(input, [p2, output](T1 t1) {
        p2(t1)(input, [output](T2 t2) {
          output(t2);
        }
      }
    }
