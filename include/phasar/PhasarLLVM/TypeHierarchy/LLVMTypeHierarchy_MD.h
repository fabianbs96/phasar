
#ifndef PHASAR_PHASARLLVM_TYPEHIERARCHY_LLVMTYPEHIERARCHY_MD_H_
#define PHASAR_PHASARLLVM_TYPEHIERARCHY_LLVMTYPEHIERARCHY_MD_H_

#include "phasar/PhasarLLVM/TypeHierarchy/LLVMVFTable.h"
#include "phasar/TypeHierarchy/TypeHierarchy.h"

#include "llvm/ADT/StringRef.h"

#include "boost/graph/adjacency_list.hpp"
#include "boost/graph/graph_traits.hpp"
#include "gtest/gtest_prod.h"
#include "nlohmann/json.hpp"

#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <llvm-14/llvm/IR/DerivedTypes.h>
#include <llvm-14/llvm/IR/Module.h>

namespace llvm {
class Module;
class StructType;
class Function;
class GlobalVariable;
} // namespace llvm

namespace psr {

class LLVMProjectIRDB;
/**
 * 	@brief Owns the class hierarchy of the analyzed program.
 *
 * 	This class is responsible for constructing a inter-modular class
 * 	hierarchy graph based on the data from the %ProjectIRCompiledDB
 * 	and reconstructing the virtual method tables.
 */
class LLVMTypeHierarchy_MD
    : public TypeHierarchy<const llvm::StructType *, const llvm::Function *> {
public:
  struct VertexProperties {
    VertexProperties() = default;
    VertexProperties(const llvm::StructType *Type);

    [[nodiscard]] std::string getTypeName() const;

    const llvm::StructType *Type = nullptr;
    std::optional<LLVMVFTable> VFT = std::nullopt;
    std::set<const llvm::StructType *> ReachableTypes;
  };

  struct Graph {
    Graph() = default;

    std::vector<VertexProperties> Vertices;
    // Adjacencies between vertices are stored here by their index in the
    // 'Vertices' vector
    std::vector<std::tuple<int>> Adjacencies;
  };

  /**
   *  @brief Creates a LLVMStructTypeHierarchy based on the
   *         given ProjectIRCompiledDB.
   *  @param IRDB ProjectIRCompiledDB object.
   */
  LLVMTypeHierarchy_MD(LLVMProjectIRDB &IRDB);

  /**
   *  @brief Creates a LLVMStructTypeHierarchy based on the
   *         llvm::Module.
   *  @param M A llvm::Module.
   */
  LLVMTypeHierarchy_MD(const llvm::Module &M);
  ~LLVMTypeHierarchy_MD() override = default;

  /**
   * @brief Constructs the actual class hierarchy graph.
   * @param M LLVM module
   *
   * Extracts new information from the given module and adds new vertices
   * accordingly to the type hierarchy graph.
   */
  void constructHierarchy(const llvm::Module &M);

  [[nodiscard]] inline bool
  hasType(const llvm::StructType *Type) const override;

  [[nodiscard]] inline bool isSubType(const llvm::StructType *Type,
                                      const llvm::StructType *SubType) override;

  std::set<const llvm::StructType *>
  getSubTypes(const llvm::StructType *Type) override;

  [[nodiscard]] inline bool
  isSuperType(const llvm::StructType *Type,
              const llvm::StructType *SuperType) override;

  std::set<const llvm::StructType *>
  getSuperTypes(const llvm::StructType *Type) override;

  [[nodiscard]] const llvm::StructType *
  getType(std::string TypeName) const override;

  [[nodiscard]] std::set<const llvm::StructType *> getAllTypes() const override;

  [[nodiscard]] std::string
  getTypeName(const llvm::StructType *Type) const override;

  [[nodiscard]] bool hasVFTable(const llvm::StructType *Type) const override;

  [[nodiscard]] const LLVMVFTable *
  getVFTable(const llvm::StructType *Type) const override;

  [[nodiscard]] inline size_t size() const override;

  [[nodiscard]] inline bool empty() const override;

  void print(llvm::raw_ostream &OS = llvm::outs()) const override;

  [[nodiscard]] nlohmann::json getAsJson() const override;

  static inline constexpr llvm::StringLiteral StructPrefix = "struct.";
  static inline constexpr llvm::StringLiteral ClassPrefix = "class.";
  static inline constexpr llvm::StringLiteral VTablePrefix = "_ZTV";
  static inline constexpr llvm::StringLiteral VTablePrefixDemang =
      "vtable for ";
  static inline constexpr llvm::StringLiteral TypeInfoPrefix = "_ZTI";
  static inline constexpr llvm::StringLiteral TypeInfoPrefixDemang =
      "typeinfo for ";
  static inline constexpr llvm::StringLiteral PureVirtualCallName =
      "__cxa_pure_virtual";

protected:
  void buildLLVMTypeHierarchy(const llvm::Module &M);

private:
  Graph TypeGraph;
  // holds all modules that are included in the type hierarchy
  std::unordered_set<const llvm::Module *> VisitedModules;
  // holds all metadata notes
  std::vector<llvm::MDNode *> MetadataNotes;
  std::unordered_map<const llvm::StructType *, LLVMVFTable> TypeVFTMap;
  // helper map from clearname to type*
  std::unordered_map<std::string, const llvm::StructType *> ClearNameTypeMap;

  static std::string removeStructOrClassPrefix(const llvm::StructType &T);

  static std::string removeStructOrClassPrefix(const std::string &TypeName);

  /**
   * 	@brief Prints the class hierarchy to an ostream in dot format.
   * 	@param an outputstream
   */
  void printAsDot(llvm::raw_ostream &OS = llvm::outs()) const;

  /**
   * @brief Extracts the metadata from a LLVM function
   * @param F LLVM function
   */
  void getMetaData(llvm::Function &F);
};

} // namespace psr

#endif
