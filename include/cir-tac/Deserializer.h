#include "proto/opgen.pb.h"
#include "proto/type.pb.h"
#include "proto/model.pb.h"

#include <clang/CIR/Dialect/Builder/CIRBaseBuilder.h>
#include <clang/CIR/Dialect/IR/CIRDataLayout.h>

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <clang/CIR/Dialect/IR/CIRTypes.h>
#include <llvm/ADT/DenseMap.h>
#include <mlir/IR/Types.h>

#include <vector>

namespace protocir {
using TypeIDCache = llvm::DenseMap<std::string, mlir::Type>;

using SerializedTypeCache = llvm::DenseMap<std::string, CIRType&>;

using BlockIDCache = llvm::DenseMap<uint64_t, mlir::Block *>;

using GlobalOPIDCache = llvm::DenseMap<std::string, cir::GlobalOp *>;

using OperationIDCache = llvm::DenseMap<uint64_t, mlir::Operation *>;

using FunctionIDCache = llvm::DenseMap<std::string, cir::FuncOp *>;

struct ModuleInfo {
  SerializedTypeCache serTypes;
  TypeIDCache types;
  GlobalOPIDCache globals;
  FunctionIDCache funcs;

  mlir::MLIRContext &ctx;
  cir::CIRBaseBuilderTy &builder;
  cir::CIRDataLayout &dataLayout;
  mlir::ModuleOp &module;

  ModuleInfo(mlir::MLIRContext &ctx,
             cir::CIRBaseBuilderTy &builder,
             cir::CIRDataLayout &dataLayout,
             mlir::ModuleOp &module) :
             ctx(ctx), builder(builder), dataLayout(dataLayout),
             module(module) {}
};

struct FunctionInfo {
  BlockIDCache blocks;

  ModuleInfo &owner;

  FunctionInfo(ModuleInfo &owner) : owner(owner) {}
};

class Deserializer {
private:
  static mlir::Type getType(ModuleInfo &mInfo,
                            const CIRTypeID &typeId);

  static void defineType(ModuleInfo &mInfo,
                         const CIRType &pTy);

  static void aggregateTypes(ModuleInfo &mInfo,
                             const CIRModule &pModule);

  static void defineIncompleteStruct(ModuleInfo &mInfo,
                                     const CIRType &pTy);
                                                    
  static void defineCompleteStruct(ModuleInfo &mInfo,
                                   const CIRType &pTy);

  static void deserializeBlock(FunctionInfo &fInfo,
                               const CIRBlock &pBlock);

  static void deserializeFunc(ModuleInfo &mInfo, 
                              const CIRFunction &pFunc);

  static void deserializeGlobal(ModuleInfo &mInfo, 
                                const CIRGlobal &pGlobal);

  static mlir::Operation deserializeOp(FunctionInfo &fInfo,
                                       const CIROp &pOp);

  static cir::FuncOp deserializeFuncOp(ModuleInfo &mInfo,
                                       const CIRFuncOp &pFuncOp);
public:
  static cir::StructType::RecordKind
  deserializeRecordKind(CIRRecordKind pKind);

  static mlir::ModuleOp deserializeModule(const CIRModule &pModule);
};
}
