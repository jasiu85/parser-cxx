#include <experimental/string_view>
#include <functional>
#include <iostream>
#include <string>

using namespace std::string_literals;

using std::experimental::string_view;

using ParserInput = string_view;

template <typename T>
using ParserOutput = std::function<void(T&&)>;

struct Unit {
  Unit() {}
};

template <typename T>
using Parser = std::function<void(ParserInput&, ParserOutput<T>&&)>;

Parser<Unit> ParseEof() {
  return [] (ParserInput& input, ParserOutput<Unit>&& output) {
    if (input.empty()) {
      output({});
    }
  };
}

Parser<string_view> ParseString(string_view s) {
  return [s = std::move(s)] (ParserInput& input, ParserOutput<string_view>&& output) mutable {
    auto n = s.size();
    if (input.substr(0, n) == s) {
      input.remove_prefix(n);
      std::cerr << "ParseString(" << s << ") success" << '\n';
      output(std::move(s));
    }
  };
}

template <typename T>
Parser<T> ParseChoice(Parser<T>&& p1, Parser<T>&& p2) {
  return [p1 = std::move(p1), p2 = std::move(p2)] (ParserInput& input, ParserOutput<T>&& output) {
    {
      auto input_copy1 = input;
      p1(input_copy1, [&output](T&& t) {
        std::cerr << "ParseChoice p1 success" << '\n';
        output(std::move(t));
      });
    }
    {
      auto input_copy2 = input;
      p2(input_copy2, [&output](T&& t) {
        std::cerr << "ParseChoice p2 success" << '\n';
        output(std::move(t));
      });
    }
  };
}

template <typename T1, typename T2>
Parser<std::pair<T1, T2>> ParseSequence(Parser<T1>&& p1, Parser<T2>&& p2) {
  return [p1 = std::move(p1), p2 = std::move(p2)] (ParserInput& input, ParserOutput<std::pair<T1, T2>>&& output) {
    p1(input, [&input, p2 = std::move(p2), output = std::move(output)] (T1&& t1) {
      std::cerr << "ParseSequence p1 success" << '\n';
      p2(input, [t1 = std::move(t1), output = std::move(output)] (T2&& t2) {
        std::cerr << "ParseSequence p1 success" << '\n';
        output(std::make_pair(std::move(t1), std::move(t2)));
      });
    });
  };
}

int main() {
  auto hello_world_parser = ParseChoice(
    ParseString("Hello"),
    ParseString("World"));
  auto result_printer = [] (const string_view& s) {
    std::cout << s << '\n';
  };
  auto raw_input = "World"s;
  auto parser_input = string_view(raw_input);
  hello_world_parser(parser_input, std::move(result_printer));
  return 0;
}
