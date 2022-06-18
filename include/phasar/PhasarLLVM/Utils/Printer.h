/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * Printer.h
 *
 *  Created on: 07.09.2018
 *      Author: rleer
 */

#ifndef PHASAR_PHASARLLVM_UTILS_PRINTER_H_
#define PHASAR_PHASARLLVM_UTILS_PRINTER_H_

#include <ostream>
#include <sstream>
#include <string>

namespace psr {

namespace detail {
template <typename WriterCallBack>
std::string stringify(WriterCallBack &&Writer) {
  std::string Buffer;
  llvm::raw_string_ostream StrS(Buffer);
  std::invoke(std::forward<WriterCallBack>(Writer), StrS);
  StrS.flush();
  return Buffer;
}
} // namespace detail

template <typename AnalysisDomainTy> struct NodePrinter {
  using N = typename AnalysisDomainTy::n_t;

  NodePrinter() = default;
  NodePrinter(const NodePrinter &) = delete;
  NodePrinter &operator=(const NodePrinter &) = delete;
  NodePrinter(NodePrinter &&) = delete;
  NodePrinter &operator=(NodePrinter &&) = delete;
  virtual ~NodePrinter() = default;

  virtual void printNode(llvm::raw_ostream &OS, N Stmt) const = 0;

  [[nodiscard]] std::string NtoString(N Stmt) const { // NOLINT
    return detail::stringify([this, &Stmt](auto &OS) { printNode(OS, Stmt); });
  }
};

template <typename AnalysisDomainTy> struct DataFlowFactPrinter {
  using D = typename AnalysisDomainTy::d_t;

  DataFlowFactPrinter() = default;
  DataFlowFactPrinter(const DataFlowFactPrinter &) = delete;
  DataFlowFactPrinter &operator=(const DataFlowFactPrinter &) = delete;
  DataFlowFactPrinter(DataFlowFactPrinter &&) = delete;
  DataFlowFactPrinter &operator=(DataFlowFactPrinter &&) = delete;
  virtual ~DataFlowFactPrinter() = default;

  virtual void printDataFlowFact(llvm::raw_ostream &OS, D Fact) const = 0;

  [[nodiscard]] std::string DtoString(D Fact) const { // NOLINT
    return detail::stringify(
        [this, &Fact](auto &OS) { printDataFlowFact(OS, Fact); });
  }
};

template <typename V> struct ValuePrinter {
  ValuePrinter() = default;
  ValuePrinter(const ValuePrinter &) = delete;
  ValuePrinter &operator=(const ValuePrinter &) = delete;
  ValuePrinter(ValuePrinter &&) = delete;
  ValuePrinter &operator=(ValuePrinter &&) = delete;
  virtual ~ValuePrinter() = default;

  virtual void printValue(llvm::raw_ostream &OS, V Val) const = 0;

  [[nodiscard]] std::string VtoString(V Val) const { // NOLINT
    return detail::stringify([this, &Val](auto &OS) { printValue(OS, Val); });
  }
};

template <typename T> struct TypePrinter {
  TypePrinter() = default;
  TypePrinter(const TypePrinter &) = delete;
  TypePrinter &operator=(const TypePrinter &) = delete;
  TypePrinter(TypePrinter &&) = delete;
  TypePrinter &operator=(TypePrinter &&) = delete;
  virtual ~TypePrinter() = default;

  virtual void printType(llvm::raw_ostream &OS, T Ty) const = 0;

  [[nodiscard]] std::string TtoString(T Ty) const { // NOLINT
    return detail::stringify([this, &Ty](auto &OS) { printType(OS, Ty); });
  }
};

template <typename AnalysisDomainTy> struct EdgeFactPrinter {
  using l_t = typename AnalysisDomainTy::l_t;

  EdgeFactPrinter() = default;
  EdgeFactPrinter(const EdgeFactPrinter &) = delete;
  EdgeFactPrinter &operator=(const EdgeFactPrinter &) = delete;
  EdgeFactPrinter(EdgeFactPrinter &&) = delete;
  EdgeFactPrinter &operator=(EdgeFactPrinter &&) = delete;
  virtual ~EdgeFactPrinter() = default;

  virtual void printEdgeFact(llvm::raw_ostream &OS, l_t L) const = 0;

  [[nodiscard]] std::string LtoString(l_t L) const { // NOLINT
    return detail::stringify([this, &L](auto &OS) { printEdgeFact(OS, L); });
  }
};

template <typename AnalysisDomainTy> struct FunctionPrinter {
  using F = typename AnalysisDomainTy::f_t;

  FunctionPrinter() = default;
  FunctionPrinter(const FunctionPrinter &) = delete;
  FunctionPrinter &operator=(const FunctionPrinter &) = delete;
  FunctionPrinter(FunctionPrinter &&) = delete;
  FunctionPrinter &operator=(FunctionPrinter &&) = delete;
  virtual ~FunctionPrinter() = default;

  virtual void printFunction(llvm::raw_ostream &OS, F Func) const = 0;

  [[nodiscard]] std::string FtoString(F Func) const { // NOLINT
    return detail::stringify(
        [this, &Func](auto &OS) { printFunction(OS, Func); });
  }
};

template <typename ContainerTy> struct ContainerPrinter {
  ContainerPrinter() = default;
  ContainerPrinter(const ContainerPrinter &) = delete;
  ContainerPrinter &operator=(const ContainerPrinter &) = delete;
  ContainerPrinter(ContainerPrinter &&) = delete;
  ContainerPrinter &operator=(ContainerPrinter &&) = delete;
  virtual ~ContainerPrinter() = default;

  virtual void printContainer(llvm::raw_ostream &OS,
                              ContainerTy Container) const = 0;

  [[nodiscard]] std::string
  ContainertoString(ContainerTy Container) const { // NOLINT
    return detail::stringify(
        [this, &Container](auto &OS) { printContainer(OS, Container); });
  }
};

} // namespace psr

#endif
