#include "cir-tac/OpSerializer.h"
#include "cir-tac/TypeSerializer.h"
#include "cir-tac/Util.h"
#include "proto/model.pb.h"

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <clang/CIR/Passes.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/TypeSwitch.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/Dialect.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/OpImplementation.h>
#include <mlir/IR/Operation.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Parser/Parser.h>

using namespace protocir;

int main(int argc, char *argv[]) {
  mlir::MLIRContext context;
  mlir::DialectRegistry registry;
  registry.insert<cir::CIRDialect>();

  context.appendDialectRegistry(registry);
  context.allowUnregisteredDialects();

  std::filesystem::path relPath = argv[1];

  auto absPath = std::filesystem::absolute(relPath);
  if (!std::filesystem::exists(absPath)) {
    throw std::runtime_error("missing path given");
  }

  mlir::ParserConfig parseConfig(&context);
  auto module =
      mlir::parseSourceFile<mlir::ModuleOp>(relPath.c_str(), parseConfig);
  MLIRModule pModule;
  MLIRModuleID pModuleID;
  std::string moduleId = (*module).getName().value_or("").str();
  *pModuleID.mutable_id() = moduleId;
  *pModule.mutable_id() = pModuleID;

  TypeCache typeCache(pModuleID);
  auto &bodyRegion = (*module).getBodyRegion();

  for (auto &bodyBlock : bodyRegion) {
    for (auto &topOp : bodyBlock) {
      if (auto cirFunc = llvm::dyn_cast<cir::FuncOp>(topOp)) {
        CIRFunction *pFunction = pModule.add_functions();
        CIRFunctionID pFunctionID;
        pFunction->mutable_id()->mutable_module_id()->CopyFrom(pModuleID);
        std::string funcId = cirFunc.getSymName().str();
        pFunction->mutable_id()->set_id(funcId);

        BlockCache blockCache;
        OpCache opCache;
        // Populate caches
        for (auto &block : cirFunc.getFunctionBody()) {
          blockCache.getMLIRBlockID(&block);
          for (auto &inst : block) {
            opCache.getMLIROpID(&inst);
          }
        }

        TypeSerializer typeSerializer(pModuleID, typeCache);
        OpSerializer opSerializer(pModuleID, typeCache, opCache, blockCache);

        for (auto &block : cirFunc.getFunctionBody()) {
          auto pBlockID = blockCache.getMLIRBlockID(&block);
          MLIRBlock *pBlock = pFunction->mutable_blocks()->add_block();
          *pBlock->mutable_id() = pBlockID;
          for (auto argumentType : block.getArgumentTypes()) {
            auto pargumentType = typeSerializer.serializeMLIRType(argumentType);
            *pBlock->add_argument_types() = pargumentType.id();
          }
          for (auto &inst : block) {
            auto pInst = opSerializer.serializeOperation(inst);
            *pBlock->add_operations() = pInst;
          }
        }
        auto pInfo = opSerializer.serializeOperation(topOp);
        *pFunction->mutable_info() = pInfo.func_op();
      } else if (auto cirGlobal = llvm::dyn_cast<cir::GlobalOp>(topOp)) {
        CIRGlobal *pGlobal = pModule.add_globals();
        CIRGlobalID pGlobalID;
        *pGlobal->mutable_id()->mutable_module_id() = pModuleID;
        std::string globalId = cirGlobal.getSymName().str();
        pGlobal->mutable_id()->set_id(globalId);
        OpCache opCache;
        BlockCache blockCache;
        OpSerializer opSerializer(pModuleID, typeCache, opCache, blockCache);
        auto pInfo = opSerializer.serializeOperation(topOp);
        *pGlobal->mutable_info() = pInfo.global_op();
      }
    }
  }

  TypeSerializer typeSerializer(pModuleID, typeCache);

  auto typeCacheSize = 0;
  do {
    typeCacheSize = typeCache.map().size();
    auto typeCacheCopy = typeCache;
    for (auto &type : typeCacheCopy.map()) {
      typeSerializer.serializeMLIRType(type.getFirst());
    }
  } while (typeCacheSize < typeCache.map().size());

  for (auto &type : typeCache.map()) {
    auto pType = typeSerializer.serializeMLIRType(type.getFirst());
    *pModule.add_types() = pType;
  }

  std::string binary;
  pModule.SerializeToString(&binary);
  llvm::outs() << binary;
}
