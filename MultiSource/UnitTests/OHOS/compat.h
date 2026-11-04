#include <atomic>
#include <stdexcept>
#include <string>

struct Base {
  virtual ~Base() = default;
  virtual std::string text() const = 0;
};
struct Derived : Base {
  std::string value;
  explicit Derived(std::string text) : value(std::move(text)) {}
  std::string text() const override { return value; }
};
struct Fault : std::runtime_error {
  using std::runtime_error::runtime_error;
};
