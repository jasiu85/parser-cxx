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
using ParserFunction = std::function<void(ParserInput&, ParserOutput<T>)>;

template <typename T>
struct Parser {
  virtual void Run(ParserInput& input, ParserOutput<T> output) = 0;
};

struct ParseEof final : public Parser<Unit> {
  void Run(ParserInput& input, ParserOutput<Unit> output) override {
    if (input.empty()) {
      output({});
    }
  }
};

struct ParseString final : public Parser<string_view> {
  ParseString(string_view s) : s(s) {}
  string_view s;
  void Run(ParserInput& input, ParserOutput<string_view> output) override {
    auto n = s.size();
    auto result = input.substr(0, n);
    if (result == s) {
      input.remove_prefix(n);
      std::cerr << "ParseString(" << s << ") success" << '\n';
      output(std::move(result));
    }
  };
};

template <typename T>
struct ParseSymmetricChoice final : public Parser<T> {
  ParseSymmetricChoice(Parser<T>& p1, Parser<T>& p2) : p1(p1), p2(p2) {}
  Parser<T>& p1;
  Parser<T>& p2;
  void Run(ParserInput& input, ParserOutput<T> output) override {
    {
      auto input_copy1 = input;
      p1.Run(input_copy1, [&output](T&& t) {
        std::cerr << "ParseSymmetricChoice p1 success" << '\n';
        output(std::move(t));
      });
    }
    {
      auto input_copy2 = input;
      p2.Run(input_copy2, [&output](T&& t) {
        std::cerr << "ParseSymmetricChoice p2 success" << '\n';
        output(std::move(t));
      });
    }
  };
};

template <typename T>
struct ParseBiasedChoice final : public Parser<T> {
  ParseBiasedChoice(Parser<T>& p1, Parser<T>& p2) : p1(p1), p2(p2) {}
  Parser<T>& p1;
  Parser<T>& p2;
  void Run(ParserInput& input, ParserOutput<T> output) override {
    // TODO(jasiu): This requires synchronous calls, it will break.
    bool success = false;
    {
      auto input_copy1 = input;
      p1.Run(input_copy1, [&success, &output](T&& t) {
        std::cerr << "ParseBiasedChoice p1 success" << '\n';
        success = true;
        output(std::move(t));
      });
    }
    if (success) {
      return;
    }
    {
      auto input_copy2 = input;
      p2.Run(input_copy2, [&output](T&& t) {
        std::cerr << "ParseBiasedChoice p2 success" << '\n';
        output(std::move(t));
      });
    }
  };
};

template <typename T1, typename T2>
struct ParseSequence final : public Parser<std::pair<T1, T2>> {
  ParseSequence(Parser<T1>& p1, Parser<T2>& p2) : p1(p1), p2(p2) {}
  Parser<T1>& p1;
  Parser<T2>& p2;
  void Run(ParserInput& input, ParserOutput<std::pair<T1, T2>> output) override {
    p1.Run(input, [this, &input, &output] (T1&& t1) {
      std::cerr << "ParseSequence p1 success" << '\n';
      p2.Run(input, [this, t1 = std::move(t1), &output] (T2&& t2) {
        std::cerr << "ParseSequence p2 success" << '\n';
        output(std::make_pair(std::move(t1), std::move(t2)));
      });
    });
  };
};

template <typename T, typename U>
struct ParseTransform final : public Parser<U> {
  ParseTransform(Parser<T>& p, std::function<U(T&&)> f) : p(p), f(f) {}
  Parser<T>& p;
  std::function<U(T&&)> f;
  void Run(ParserInput& input, ParserOutput<U> output) override {
    p.Run(input, [this, &output](T&& t) {
      std::cerr << "ParseTransform success" << '\n';
      output(f(std::move(t)));
    });
  }
};

int main() {
  auto h = ParseString{"Hello"};
  auto w = ParseString{"World"};
  auto hw = ParseSequence<string_view, string_view>{h, w};
  auto hw2 = ParseTransform<std::pair<string_view, string_view>, std::string>{
    hw, [](std::pair<string_view, string_view>&& p) { return std::string(p.first) + std::string(p.second); }};
  auto result_printer = [] (std::string&& s) {
    std::cout << "Result " << s << '\n';
  };
  auto raw_input = "HelloWorld"s;
  auto parser_input = string_view(raw_input);
  hw2.Run(parser_input, result_printer);
  return 0;
}
