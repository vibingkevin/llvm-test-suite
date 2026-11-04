#include "compat.h"

static const std::string initial(256, 'x');
static thread_local int calls;
extern "C" Base *make_object() { return new Derived(initial); }
extern "C" void delete_object(Base *object) { delete object; }
extern "C" void grow_string(std::string *text) { text->append(initial); }
extern "C" void throw_fault() { throw Fault(initial); }
extern "C" bool catch_fault(void (*callback)()) {
  try { callback(); } catch (const Fault &fault) {
    return std::string(fault.what()) == "callback";
  }
  return false;
}
extern "C" int tick(std::atomic<int> *count) {
  count->fetch_add(1, std::memory_order_seq_cst);
  return ++calls;
}
