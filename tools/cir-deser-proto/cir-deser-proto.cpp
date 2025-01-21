#include "proto/model.pb.h"

#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <clang/CIR/Dialect/Builder/CIRBaseBuilder.h>
#include <clang/CIR/Dialect/IR/CIRDataLayout.h>
#include <clang/CIR/Passes.h>

#include <mlir/IR/Verifier.h>
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

#include <cinttypes>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

using namespace protocir;

llvm::LogicalResult buildOp(const CIROp &pOp,
                            cir::CIRBaseBuilderTy &builder,
                            mlir::Operation *&rOp) {
  // llvm::TypeSwitch<mlir::Operation *>(&inst)
  pOp.operation_case();
  
  return llvm::success();
}

llvm::LogicalResult buildAndAddBlock(const CIRBlock &pBlock,
                               cir::CIRBaseBuilderTy &builder,
                               const cir::FuncOp &owner,
                               mlir::Block *&rBlock) {
  mlir::OpBuilder::InsertionGuard guard(builder);

  for (auto op : pBlock.operations()) {
    mlir::Operation *opNew;
    if (!llvm::succeeded(buildOp(op, builder, opNew)))
      return llvm::failure();
    
  }

  return llvm::success();
}

llvm::LogicalResult getType(CIRType &pTy,
                            mlir::MLIRContext &ctx,
                            mlir::Type &rTy) {
  switch (pTy.kind_case()) {
    case CIRType::KindCase::kIntType:
      {
        auto ty = pTy.int_type();
        rTy = cir::IntType::get(&ctx, ty.width(), ty.is_signed());
      }
      break;
    case CIRType::KindCase::kSingleType:
      rTy = cir::SingleType::get(&ctx);
      break;
    case CIRType::KindCase::kDoubleType:
      rTy = cir::DoubleType::get(&ctx);
      break;
    case CIRType::KindCase::kFp16Type:
      rTy = cir::FP16Type::get(&ctx);
      break;
    case CIRType::KindCase::kBf16Type:
      rTy = cir::BF16Type::get(&ctx);
      break;
    case CIRType::KindCase::kFp80Type:
      rTy = cir::FP80Type::get(&ctx);
      break;
    case CIRType::KindCase::kFp128Type:
      rTy = cir::FP128Type::get(&ctx);
      break;
    case CIRType::KindCase::kLongDoubleType:
    case CIRType::KindCase::kComplexType:
    case CIRType::KindCase::kPointerType:
    case CIRType::KindCase::kDataMemberType:
    case CIRType::KindCase::kBoolType:
      rTy = cir::BoolType::get(&ctx);
      break;
    case CIRType::KindCase::kArrayType:
    case CIRType::KindCase::kVectorType:
    case CIRType::KindCase::kFuncType:
    case CIRType::KindCase::kMethodType:
    case CIRType::KindCase::kExceptionInfoType:
    case CIRType::KindCase::kVoidType:
    case CIRType::KindCase::kStructType:
    case CIRType::KindCase::KIND_NOT_SET:
      llvm::outs() << "Type kind not set!";
      return llvm::failure();
    default:
      llvm::outs() << "CIRType::KindCase set as " << pTy.kind_case();
      llvm_unreachable("NIY");
  }
  return llvm::success();
}

llvm::LogicalResult buildFunction(const CIRFunction &pFunc,
                                  cir::CIRBaseBuilderTy &builder,
                                  cir::FuncOp *&rFunc) {
  
  return llvm::success();
}

llvm::LogicalResult buildModule(const CIRModule &pModule,
                                mlir::ModuleOp *&rModule) {
  auto mlirCtx = new mlir::MLIRContext();
  mlirCtx->loadDialect<cir::CIRDialect>();
  auto builder = cir::CIRBaseBuilderTy(*mlirCtx);
  auto mod = mlir::ModuleOp::create(builder.getUnknownLoc(), pModule.id().id());
  cir::CIRDataLayout datalayout(mod);

  std::vector<cir::FuncOp*> funcs;
  for (auto pFunc : pModule.functions()) {
    cir::FuncOp *fNew;
    if (!llvm::succeeded(buildFunction(pFunc, builder, fNew)))
      return llvm::failure();
    funcs.push_back(fNew);
  }

  std::cout << "Types present:" << std::endl;
  for (auto ty : pModule.types()) {
    std::cout << ty.name() << std::endl;
  }

  rModule = &mod;
  return llvm::success();
}

int main(int argc, char *argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  CIRModule pModule;

  {
    std::fstream input(argv[1], std::ios::in | std::ios::binary);
    if (!pModule.ParseFromIstream(&input)) {
      std::cerr << "Failed to parse [" << argv[1] << "] file" << std::endl;
      return -1;
    }
  }

  mlir::ModuleOp* rModule;
  if (!llvm::succeeded(buildModule(pModule, rModule)))
  {
    std::cerr << "Failed to build the module!" << std::endl;
    return -2;
  }
  rModule->print(llvm::outs());

  google::protobuf::ShutdownProtobufLibrary();
}
