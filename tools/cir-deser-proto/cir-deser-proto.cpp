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

#include "cir-tac/Deserializer.h"

using namespace protocir;

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

  auto ctx = mlir::MLIRContext();
  ctx.loadDialect<cir::CIRDialect>();
  auto rModule = Deserializer::deserializeModule(ctx, pModule);
  rModule.print(llvm::outs());

  google::protobuf::ShutdownProtobufLibrary();
}
