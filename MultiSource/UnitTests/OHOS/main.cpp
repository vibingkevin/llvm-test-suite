#include "compat.h"
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <thread>

#define check(condition) do { if (!(condition)) { \
  std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
  std::abort(); } } while (false)
static void callback() { throw Fault("callback"); }

int main(int argc, char **argv) {
  check(argc == 2);
  void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!library) { std::fprintf(stderr, "%s\n", dlerror()); return 1; }
  auto make = reinterpret_cast<Base *(*)()>(dlsym(library, "make_object"));
  auto drop = reinterpret_cast<void (*)(Base *)>(dlsym(library, "delete_object"));
  auto grow = reinterpret_cast<void (*)(std::string *)>(dlsym(library, "grow_string"));
  auto raise = reinterpret_cast<void (*)()>(dlsym(library, "throw_fault"));
  auto catch_it = reinterpret_cast<bool (*)(void (*)())>(dlsym(library, "catch_fault"));
  auto tick = reinterpret_cast<int (*)(std::atomic<int> *)>(dlsym(library, "tick"));
  check(make && drop && grow && raise && catch_it && tick);
  Base *object = make();
  check(dynamic_cast<Derived *>(object) != nullptr);
  std::string text = object->text();
  check(text == std::string(256, 'x'));
  delete object;
  drop(new Derived("created by caller"));
  grow(&text);
  check(text == std::string(512, 'x'));
  bool caught = false;
  try { raise(); } catch (const Fault &fault) {
    caught = std::string(fault.what()) == std::string(256, 'x');
  }
  check(caught && catch_it(callback));
  std::atomic<int> count{0};
  check(tick(&count) == 1 && tick(&count) == 2);
  std::thread worker([&] { check(tick(&count) == 1 && tick(&count) == 2); });
  worker.join();
  check(count.load() == 4);
  if (std::getenv("OHOS_COMPAT_TRACE")) {
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line))
      if (line.find("libc++") != std::string::npos || line.find("ld-musl") != std::string::npos)
        std::fprintf(stderr, "%s\n", line.c_str());
  }
  check(dlclose(library) == 0);
  std::puts("PASS");
}
