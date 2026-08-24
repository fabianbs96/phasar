// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint15_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: A LLVM-based static analysis framework

void sink([[clang::annotate("psr.sink")]] int) {}
void sanitize([[clang::annotate("psr.sanitizer")]] int *) noexcept {}
class Source {
public:
  [[clang::annotate("psr.source")]] virtual int get() { return 42; }
};

Source *makeSource() { return new Source; }
extern void disposeSource(Source *src);

struct DoubleIntPair {
  double d;
  int i;
};

int main() {
  auto src = makeSource();
  DoubleIntPair dip = {3.1415926, src->get()};

  auto x = dip.i;
  sanitize(&dip.i);

  sink(dip.i);
  sink(x);

  disposeSource(src);
}
