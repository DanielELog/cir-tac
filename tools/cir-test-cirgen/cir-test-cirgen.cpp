#include <clang/CIR/Dialect/IR/CIRDialect.h>
#include <clang/CIR/Dialect/Builder/CIRBaseBuilder.h>
#include <clang/CIR/Passes.h>
#include <clang/CIR/Dialect/IR/CIRDataLayout.h>

#include <mlir/Dialect/LLVMIR/LLVMDialect.h>

#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/Dialect/LLVMIR/LLVMDialect.h>
#include <mlir/Dialect/MemRef/IR/MemRefOpsDialect.h.inc>
#include <mlir/Dialect/OpenMP/OpenMPDialect.h>

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
#include <string>

int main(int argc, char *argv[]) {
  /* context initialization */
  auto mlirCtx = new mlir::MLIRContext();
  mlir::DialectRegistry registry;
  registry.insert<cir::CIRDialect>();

  mlirCtx->appendDialectRegistry(registry);
  mlirCtx->allowUnregisteredDialects();
  mlirCtx->loadDialect<cir::CIRDialect>();

  /* builder initializaiton */
  auto builder = cir::CIRBaseBuilderTy(*mlirCtx);

  /* main module initialization */
  auto mod = mlir::ModuleOp::create(builder.getUnknownLoc());
  cir::CIRDataLayout datalayout(mod);

  std::cout << "Loaded dialects:" << std::endl;
  for (auto dia : mlirCtx->getLoadedDialects()) {
    std::cout << dia->getNamespace().data() << std::endl;
  }
  std::cout << std::endl;

  std::cout << "Module attrivute:" << std::endl;
  for (auto attr : mod.getAttributeNames()) {
    std::cout << attr.data() << std::endl;
  }
  std::cout << std::endl;

  auto lang = cir::SourceLanguageAttr::get(mlirCtx, cir::SourceLanguage::CXX);
  mod->setAttr(cir::CIRDialect::getLangAttrName(),
                     cir::LangAttr::get(mlirCtx, lang));

  /* types in use initialization */
  auto charType = builder.getSIntNTy(8);
  auto intType = builder.getSIntNTy(32);
  auto ptrInt = cir::PointerType::get(intType);
  auto shortStringType = cir::ArrayType::get(mlirCtx, charType, 4);
  auto ptrChar = cir::PointerType::get(charType);
  auto ptrPtrChar = cir::PointerType::get(ptrChar);
  auto inputTypes = llvm::ArrayRef<mlir::Type>({intType});
  auto fTy = cir::FuncType::get(inputTypes, ptrChar);

  /* 'foo' global string */
  auto fooVal = cir::ConstArrayAttr::get(shortStringType, mlir::StringAttr::get(llvm::Twine("foo"), shortStringType));
  auto fooStr = builder.create<cir::GlobalOp>(builder.getUnknownLoc(), ".str_foo", shortStringType, true,
                                              cir::GlobalLinkageKind::ExternalLinkage, cir::AddressSpaceAttr());
  fooStr.setInitialValueAttr(fooVal);

  /* 'bar' global string */
  auto barVal = cir::ConstArrayAttr::get(shortStringType, mlir::StringAttr::get(llvm::Twine("bar"), shortStringType));
  auto barStr = builder.create<cir::GlobalOp>(builder.getUnknownLoc(), ".str_bar", shortStringType, true,
                                              cir::GlobalLinkageKind::ExternalLinkage, cir::AddressSpaceAttr());
  barStr.setInitialValueAttr(barVal);

  /* function initialization */
  auto func = builder.create<cir::FuncOp>(builder.getUnknownLoc(), "foobar", fTy);

  func.setLinkageAttr(cir::GlobalLinkageKindAttr::get(
      mlirCtx, cir::GlobalLinkageKind::ExternalLinkage));
  mlir::SymbolTable::setSymbolVisibility(
      func, mlir::SymbolTable::Visibility::Private);

  // Initialize with empty dict of extra attributes.
  func.setExtraAttrsAttr(cir::ExtraFuncAttributesAttr::get(
      mlirCtx, builder.getDictionaryAttr({})));

  auto &entryBlock = *func.addEntryBlock();
  auto &ifBlock = *func.addBlock();
  auto &elseBlock = *func.addBlock();
  auto &returnBlock = *func.addBlock();

  /* function operations list */
  builder.setInsertionPointToStart(&entryBlock);

  uint64_t charAlign = datalayout.getABITypeAlign(charType).value();
  auto addrRes = builder.createAlloca(builder.getUnknownLoc(), ptrPtrChar,
                                   ptrChar, "__retval", clang::CharUnits::fromQuantity(charAlign));

  uint64_t intAlign = datalayout.getABITypeAlign(intType).value();
  auto addrNum = builder.createAlloca(builder.getUnknownLoc(), ptrInt,
                                   intType, "input", clang::CharUnits::fromQuantity(intAlign));

  auto arg0 = func.getArgument(0);

  builder.createStore(builder.getUnknownLoc(), arg0, addrNum);

  mlir::Value input = builder.createLoad(builder.getUnknownLoc(), addrNum);

  auto fiveConst = builder.create<cir::ConstantOp>(builder.getUnknownLoc(), intType,
                                                   cir::IntAttr::get(intType, 5));
  auto zeroConst = builder.create<cir::ConstantOp>(builder.getUnknownLoc(), intType,
                                                   cir::IntAttr::get(intType, 0));

  auto remByFive = builder.createBinop(input, cir::BinOpKind::Rem, fiveConst);

  mlir::Value cmpRes = builder.createCompare(builder.getUnknownLoc(), cir::CmpOpKind::eq, remByFive, zeroConst);

  builder.create<cir::BrCondOp>(builder.getUnknownLoc(), cmpRes, &ifBlock, &elseBlock);

  /* 'bar' branch operations */
  builder.setInsertionPointToStart(&ifBlock);

  mlir::Value getBarStr = builder.createGetGlobal(barStr);

  auto ptrBarStr = builder.createCast(cir::CastKind::array_to_ptrdecay, getBarStr, ptrChar);

  builder.createStore(builder.getUnknownLoc(), ptrBarStr, addrRes);

  builder.create<cir::BrOp>(builder.getUnknownLoc(), &returnBlock);

  /* 'foo' branch operations */
  builder.setInsertionPointToStart(&elseBlock);

  mlir::Value getFooStr = builder.createGetGlobal(fooStr);

  auto ptrFooStr = builder.createCast(cir::CastKind::array_to_ptrdecay, getFooStr, ptrChar);

  builder.createStore(builder.getUnknownLoc(), ptrFooStr, addrRes);

  builder.create<cir::BrOp>(builder.getUnknownLoc(), &returnBlock);

  /* rest of function's operations */
  builder.setInsertionPointToStart(&returnBlock);
  
  mlir::Value result = builder.createLoad(builder.getUnknownLoc(), addrRes);
  
  builder.create<cir::ReturnOp>(builder.getUnknownLoc(), result);

  /* adding all operations to the module */
  mod.push_back(fooStr);
  mod.push_back(barStr);
  mod.push_back(func);

  mod.print(llvm::outs());
  std::cout << (mlir::verify(mod).succeeded() ? "success" : "failure") << std::endl;
}
