# Design notes

## Parser

_Parser_ is a function that takes a string (in future will be generalized to
input source) and returns a value. Simplified signature:

    template <typename ResultType>
    using Parser = std::function<ResultType(std::string)>;
