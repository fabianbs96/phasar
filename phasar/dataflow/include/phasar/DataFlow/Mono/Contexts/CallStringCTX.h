#ifndef PHASAR_DATAFLOW_MONO_CONTEXTS_CALLSTRINGCTX_H
#define PHASAR_DATAFLOW_MONO_CONTEXTS_CALLSTRINGCTX_H

#include "phasar/Utils/Printer.h"

#include "llvm/Support/raw_ostream.h"

#include "boost/functional/hash.hpp"

#include <deque>
#include <functional>
#include <initializer_list>

namespace psr {

template <typename N, unsigned K> class CallStringCTX {
protected:
  std::deque<N> CallString;
  static constexpr unsigned KLimit = K;
  friend struct std::hash<psr::CallStringCTX<N, K>>;

public:
  CallStringCTX() = default;

  CallStringCTX(std::initializer_list<N> IList) : CallString(IList) {
    if (IList.size() > KLimit) {
      throw std::runtime_error(
          "initial call std::string length exceeds maximal length K");
    }
  }

  void push_back(N Stmt) { // NOLINT
    if (CallString.size() > KLimit - 1) {
      CallString.pop_front();
    }
    CallString.push_back(Stmt);
  }

  N pop_back() { // NOLINT
    if (!CallString.empty()) {
      N Stmt = CallString.back();
      CallString.pop_back();
      return Stmt;
    }
    return N{};
  }

  [[nodiscard]] bool isEqual(const CallStringCTX &Rhs) const {
    return CallString == Rhs.CallString;
  }

  [[nodiscard]] bool isDifferent(const CallStringCTX &Rhs) const {
    return !isEqual(Rhs);
  }

  friend bool operator==(const CallStringCTX<N, K> &Lhs,
                         const CallStringCTX<N, K> &Rhs) {
    return Lhs.isEqual(Rhs);
  }

  friend bool operator!=(const CallStringCTX<N, K> &Lhs,
                         const CallStringCTX<N, K> &Rhs) {
    return !Lhs.isEqual(Rhs);
  }

  friend bool operator<(const CallStringCTX<N, K> &Lhs,
                        const CallStringCTX<N, K> &Rhs) {
    return Lhs.cs < Rhs.cs;
  }

  llvm::raw_ostream &print(llvm::raw_ostream &OS,
                           const NodePrinterBase<N> &NP) const {
    OS << "Call string: [ ";
    for (auto C : CallString) {
      NP.printNode(OS, C);
      if (C != CallString.back()) {
        OS << " * ";
      }
    }
    return OS << " ]";
  }

  [[nodiscard]] bool empty() const { return CallString.empty(); }

  [[nodiscard]] std::size_t size() const { return CallString.size(); }
}; // namespace psr

} // namespace psr

namespace std {

template <typename N, unsigned K> struct hash<psr::CallStringCTX<N, K>> {
  size_t operator()(const psr::CallStringCTX<N, K> &CS) const noexcept {
    boost::hash<std::deque<N>> HashDeque;
    std::hash<unsigned> HashUnsigned;
    size_t U = HashUnsigned(K);
    size_t H = HashDeque(CS.CallString);
    return U ^ (H << 1);
  }
};

} // namespace std

#endif
