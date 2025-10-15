#include "phasar/PhasarLLVM/Utils/AddressTakenFunctions.h"

#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Value.h"

#include <cstring>
#include <new>

using namespace psr;

// Derived from LLVM's llvm::Function::hasAddressTaken()
static bool isAddressTakenImpl(const llvm::Value *F) {
  if (!F) {
    return false;
  }

  for (const auto &Use : F->uses()) {
    const auto *User = Use.getUser();

    if (llvm::isa<llvm::GlobalAlias>(User)) {
      if (isAddressTakenImpl(User)) {
        return true;
      }

      continue;
    }

    if (const auto *Glob = llvm::dyn_cast<llvm::GlobalVariable>(User)) {
      if (Glob->getName() == "llvm.compiler.used" ||
          Glob->getName() == "llvm.used") {
        continue;
      }

      return true;
    }

    const auto *Call = llvm::dyn_cast<llvm::CallBase>(User);
    if (!Call) {
      return true;
    }

    if (Call->isDebugOrPseudoInst()) {
      continue;
    }

    const auto *Intrinsic = llvm::dyn_cast<llvm::IntrinsicInst>(Call);
    if (Intrinsic && Intrinsic->isAssumeLikeIntrinsic()) {
      continue;
    }

    if (Call->isCallee(&Use)) {
      continue;
    }

    return true;
  }

  return false;
}

bool psr::isAddressTakenFunction(const llvm::Function *F) {
  return isAddressTakenImpl(F);
}

AddressTakenFunctions::AddressTakenFunctions(const LLVMProjectIRDB &IRDB) {
  llvm::SmallVector<const llvm::Function *> ATF;
  ATF.reserve(IRDB.getNumFunctions() / 2);
  for (const auto *F : IRDB.getAllFunctions()) {
    if (isAddressTakenImpl(F)) {
      ATF.push_back(F);
    }
  }

  auto NumATF = ATF.size();
  assert(NumATF <= UINT32_MAX);
  auto NumBytes = DataT::totalSizeToAlloc<value_type>(NumATF);
  auto *RawBytes = ::operator new(NumBytes, std::align_val_t{alignof(DataT)});

  auto *DataObj = new (RawBytes) DataT(NumATF);
  Data = DataObj;

  memcpy(DataObj->getTrailingObjects<value_type>(), ATF.data(),
         NumATF * sizeof(value_type));
}

void AddressTakenFunctions::DataT::destroyData() const noexcept {
  // All contained data is POD, so no further destructing needed
  ::operator delete((void *)this, std::align_val_t{alignof(DataT)});
}
