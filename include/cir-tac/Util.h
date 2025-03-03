#pragma once

#include "mlir/IR/Operation.h"
#include "proto/setup.pb.h"
#include "proto/type.pb.h"

#include <clang/CIR/Dialect/Builder/CIRBaseBuilder.h>
#include <clang/CIR/Dialect/IR/CIRDataLayout.h>

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <clang/CIR/Dialect/IR/CIRTypes.h>
#include <llvm/ADT/DenseMap.h>

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/IR/Types.h>

#include <string>
#include <unordered_map>

using namespace protocir;

class TypeCache {
public:
  TypeCache(MLIRModuleID moduleID) : moduleID(moduleID) {}

  MLIRTypeID getMLIRTypeID(mlir::Type type) {
    if (!cache_.contains(type)) {
      std::string name;
      llvm::raw_string_ostream nameStream(name);
      type.print(nameStream);
      MLIRTypeID typeID;
      *typeID.mutable_id() = name;
      *typeID.mutable_module_id() = moduleID;
      cache_[type] = typeID;
    }
    return cache_.at(type);
  }

  llvm::DenseMap<mlir::Type, MLIRTypeID> &map() { return cache_; }

private:
  MLIRModuleID moduleID;
  llvm::DenseMap<mlir::Type, MLIRTypeID> cache_;
};

class OpCache {
public:
  MLIROpID getMLIROpID(mlir::Operation *operation) {
    if (!cache_.contains(operation)) {
      MLIROpID opID;
      opID.set_id(cache_.size());
      cache_[operation] = opID;
    }
    return cache_.at(operation);
  }

private:
  llvm::DenseMap<mlir::Operation *, MLIROpID> cache_;
};

class BlockCache {
public:
  MLIRBlockID getMLIRBlockID(mlir::Block *block) {
    if (!cache_.contains(block)) {
      MLIRBlockID blockID;
      blockID.set_id(cache_.size());
      cache_[block] = blockID;
    }
    return cache_.at(block);
  }

private:
  llvm::DenseMap<mlir::Block *, MLIRBlockID> cache_;
};

using TypeIDCache = std::unordered_map<std::string, mlir::Type>;

using SerializedTypeCache = std::unordered_map<std::string, MLIRType>;

using BlockIDCache = llvm::DenseMap<uint64_t, mlir::Block *>;

using GlobalOPIDCache = std::unordered_map<std::string, mlir::Operation *>;

using OperationIDCache = std::unordered_map<uint64_t, mlir::Operation *>;

using FunctionIDCache = std::unordered_map<std::string, cir::FuncOp *>;

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
  OperationIDCache ops;

  ModuleInfo &owner;

  FunctionInfo(ModuleInfo &owner) : owner(owner) {}
};

inline std::string serializeAPInt(llvm::APInt i) {
  std::string result;
  llvm::raw_string_ostream stream(result);
  i.print(stream, true);
  return result;
}

inline llvm::APInt deserializeAPInt(mlir::Type innerType, std::string pI) {
  return llvm::APInt(innerType.getIntOrFloatBitWidth(), pI, 10);
}

inline MLIRAPFloat serializeAPFloat(llvm::APFloat f) {
  MLIRAPFloat protoF;
  std::string result;
  llvm::raw_string_ostream stream(result);
  f.print(stream);
  *protoF.mutable_value() = result;
  protoF.set_semantics(llvm::APFloatBase::SemanticsToEnum(f.getSemantics()));
  return protoF;
}

inline llvm::APFloat deserializeAPFloat(MLIRAPFloat pF) {
  std::string value = pF.value();
  auto semanticsEnum =
    static_cast<llvm::APFloatBase::Semantics>(pF.semantics());
  auto &semantics = llvm::APFloatBase::EnumToSemantics(semanticsEnum);
  return llvm::APFloat(semantics, value);
}

inline std::string serializeStringRef(llvm::StringRef r) { return r.str(); }
