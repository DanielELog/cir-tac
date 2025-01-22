#include "cir-tac/Serializer.h"

#include "proto/model.pb.h"

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <clang/CIR/Passes.h>

#include <mlir/Dialect/DLTI/DLTIDialect.h.inc>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/Dialect.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/OpImplementation.h>
#include <mlir/IR/Operation.h>
#include <mlir/IR/Types.h>
#include <mlir/IR/Visitors.h>
#include <mlir/Parser/Parser.h>

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/TypeSwitch.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>

#include <tuple>
#include <filesystem>

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
  protocir::CIRModule pModule;
  protocir::CIRModuleID pModuleID;
  std::string moduleId = (*module).getName().value_or("").str();
  *pModuleID.mutable_id() = moduleId;
  pModule.mutable_id()->CopyFrom(pModuleID);
  TypeCache typeCache;
  auto &bodyRegion = (*module).getBodyRegion();
  for (auto &bodyBlock : bodyRegion) {
    for (auto &topOp : bodyBlock) {
      if (auto cirFunc = llvm::dyn_cast<cir::FuncOp>(topOp)) {
        protocir::CIRFunction *pFunction = pModule.add_functions();
        protocir::CIRFunctionID pFunctionID;
        pFunction->mutable_id()->mutable_module_id()->CopyFrom(pModuleID);
        std::string funcId = cirFunc.getSymName().str();
        pFunction->mutable_id()->set_id(funcId);
        BlockCache blockCache;
        OperationCache opCache;
        for (auto &block : cirFunc.getFunctionBody()) {
          std::ignore = Serializer::internBlock(blockCache, &block);
          for (auto &inst : block) {
            std::ignore = Serializer::internOperation(opCache, &inst);
          }
        }
        for (auto &block : cirFunc.getFunctionBody()) {
          unsigned long blockIdx = Serializer::internBlock(blockCache, &block);
          protocir::CIRBlock *pBlock = pFunction->add_blocks();
          for (auto argumentType : block.getArgumentTypes()) {
            auto pargumentType =
                Serializer::serializeType(argumentType, pModuleID, typeCache);
            pBlock->add_argument_types()->CopyFrom(pargumentType.id());
          }
          protocir::CIRBlockID pBlockID;
          pBlockID.set_id(blockIdx);
          for (auto &inst : block) {
            auto pInst = Serializer::serializeOperation(
                inst, pModuleID, typeCache, opCache, blockCache);
            pBlock->add_operations()->CopyFrom(pInst);
          }
        }
        auto pInfo = Serializer::serializeOperation(topOp, pModuleID, typeCache,
                                                    opCache, blockCache);
        pFunction->mutable_info()->CopyFrom(pInfo.func_op());
      } else if (auto cirGlobal = llvm::dyn_cast<cir::GlobalOp>(topOp)) {
        protocir::CIRGlobal *pGlobal = pModule.add_globals();
        protocir::CIRGlobalID pGlobalID;
        pGlobal->mutable_id()->mutable_module_id()->CopyFrom(pModuleID);
        std::string globalId = cirGlobal.getSymName().str();
        pGlobal->mutable_id()->set_id(globalId);
        BlockCache blockCache;
        OperationCache opCache;
        auto pInfo = Serializer::serializeOperation(topOp, pModuleID, typeCache,
                                                    opCache, blockCache);
        pGlobal->mutable_info()->CopyFrom(pInfo.global_op());
      }
    }
  }
  auto typeCacheSize = 0;
  do {
    typeCacheSize = typeCache.size();
    auto typeCacheCopy = typeCache;
    for (auto &type : typeCacheCopy) {
      std::ignore =
          Serializer::serializeType(type.getFirst(), pModuleID, typeCache);
    }
  } while (typeCacheSize < typeCache.size());
  auto typeCacheCopy = typeCache;
  for (auto &type : typeCacheCopy) {
    auto pType =
        Serializer::serializeType(type.getFirst(), pModuleID, typeCache);
    pModule.add_types()->CopyFrom(pType);
  }
  std::string binary;
  pModule.SerializeToString(&binary);
  llvm::outs() << binary;
}
