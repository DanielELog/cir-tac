#include "cir-tac/Deserializer.h"
#include "cir-tac/AttrDeserializer.h"
#include "cir-tac/EnumDeserializer.h"
#include "cir-tac/OpDeserializer.h"
#include "cir-tac/Util.h"
#include "proto/setup.pb.h"
#include "clang/CIR/Dialect/IR/CIRTypes.h"
#include "llvm/Support/ErrorHandling.h"

#include <mlir/IR/Verifier.h>

using namespace protocir;

mlir::Type Deserializer::getType(ModuleInfo &mInfo,
                                 const MLIRTypeID &typeId) {
  auto typeIdStr = typeId.id();
  if (mInfo.types.find(typeIdStr) == mInfo.types.end()) {
    defineType(mInfo, mInfo.serTypes.at(typeIdStr));
  }
  auto rTy = mInfo.types.at(typeIdStr);
  return rTy;
}

void Deserializer::defineType(ModuleInfo &mInfo,
                              const MLIRType &pTy) {
  auto ctx = &mInfo.ctx;
  mlir::Type rTy;
  switch (pTy.type_case()) {
    case MLIRType::TypeCase::kCirIntType:
      {
        auto ty = pTy.cir_int_type();
        rTy = cir::IntType::get(ctx, ty.width(), ty.is_signed());
      }
      break;
    case MLIRType::TypeCase::kCirSingleType:
      rTy = cir::SingleType::get(ctx);
      break;
    case MLIRType::TypeCase::kCirDoubleType:
      rTy = cir::DoubleType::get(ctx);
      break;
    case MLIRType::TypeCase::kCirfp16Type:
      rTy = cir::FP16Type::get(ctx);
      break;
    case MLIRType::TypeCase::kCirbFloat16Type:
      rTy = cir::BF16Type::get(ctx);
      break;
    case MLIRType::TypeCase::kCirfp80Type:
      rTy = cir::FP80Type::get(ctx);
      break;
    case MLIRType::TypeCase::kCirfp128Type:
      rTy = cir::FP128Type::get(ctx);
      break;
    case MLIRType::TypeCase::kCirLongDoubleType:
      {
        auto underlyingTy =
          getType(mInfo, pTy.cir_long_double_type().underlying());
        rTy = cir::LongDoubleType::get(ctx, underlyingTy);
      }
      break;
    case MLIRType::TypeCase::kCirComplexType:
      {
        auto elementTy =
          getType(mInfo, pTy.cir_complex_type().element_ty());
        rTy = cir::ComplexType::get(ctx, elementTy);
      }
      break;
    case MLIRType::TypeCase::kCirPointerType:
      {
        auto pointee = getType(mInfo, pTy.cir_pointer_type().pointee());
        if (pTy.cir_pointer_type().has_addr_space()) {
          auto addrSpace = pTy.cir_pointer_type().addr_space();
          assert(0 && "needs attr deserializer!!!");
        }
        else {
          rTy = cir::PointerType::get(ctx, pointee);
        }
      }
      break;
    case MLIRType::TypeCase::kCirDataMemberType:
      {
        auto memTy = getType(mInfo, pTy.cir_data_member_type().member_ty());
        auto clsTy = getType(mInfo, pTy.cir_data_member_type().cls_ty());
        assert((mlir::isa<cir::StructType>(clsTy))
                && "clsTy should be a StructType!");
        cir::DataMemberType::get(ctx, memTy,
                                 mlir::cast<cir::StructType>(clsTy));
      }
      break;
    case MLIRType::TypeCase::kCirBoolType:
      rTy = cir::BoolType::get(ctx);
      break;
    case MLIRType::TypeCase::kCirArrayType:
      {
        auto elTy = getType(mInfo, pTy.cir_array_type().elt_type());
        auto size = pTy.cir_array_type().size();
        rTy = cir::ArrayType::get(ctx, elTy, size);
      }
      break;
    case MLIRType::TypeCase::kCirVectorType:
      {
        auto elTy = getType(mInfo, pTy.cir_vector_type().elt_type());
        auto size = pTy.cir_vector_type().size();
        rTy = cir::VectorType::get(ctx, elTy, size);
      }
      break;
    case MLIRType::TypeCase::kCirFuncType:
      {
        std::vector<mlir::Type> vecInputTys;
        for (auto ty : pTy.cir_func_type().inputs())
          vecInputTys.push_back(getType(mInfo, ty));
        auto inputTys = mlir::ArrayRef<mlir::Type>(vecInputTys);
        auto returnTy = getType(mInfo, pTy.cir_func_type().return_type());
        auto isVarArg = pTy.cir_func_type().var_arg();
        rTy = cir::FuncType::get(ctx, inputTys, returnTy, isVarArg);
      }
      break;
    case MLIRType::TypeCase::kCirMethodType:
      {
        auto memTy = getType(mInfo, pTy.cir_method_type().member_func_ty());
        assert((mlir::isa<cir::FuncType>(memTy))
                && "memberFuncTy should be a FuncType!");
        auto clsTy = getType(mInfo, pTy.cir_method_type().cls_ty());
        assert((mlir::isa<cir::StructType>(clsTy))
                && "clsTy should be a StructType!");
        cir::MethodType::get(ctx, mlir::cast<cir::FuncType>(memTy),
                             mlir::cast<cir::StructType>(clsTy));
      }
      break;
    case MLIRType::TypeCase::kCirExceptionType:
      rTy = cir::ExceptionInfoType::get(ctx);
      break;
    case MLIRType::TypeCase::kCirVoidType:
      rTy = cir::VoidType::get(ctx);
      break;
    case MLIRType::TypeCase::kCirStructType:
      llvm_unreachable("Definition of StructTypes should not happen"
                       "inside of a generic defineType!");
    case MLIRType::TypeCase::TYPE_NOT_SET:
      llvm_unreachable("Type kind not set!");
    default:
      llvm::outs() << "MLIRType::TypeCase set as " << pTy.type_case();
      llvm_unreachable("NYI");
  }
  auto typeId = pTy.id().id();
  mInfo.types[typeId] = rTy;
}

void Deserializer::defineIncompleteStruct(ModuleInfo &mInfo,
                                          const MLIRType &pTy) {
  assert(pTy.has_cir_struct_type() && "pTy is not of StructType!");
  auto nameAttr =
    AttrDeserializer::deserializeMLIRStringAttr(mInfo, pTy.cir_struct_type().name());
  auto recordKind = deserializeRecordKind(pTy.cir_struct_type().kind());
  auto incompleteStruct =
   cir::StructType::get(&mInfo.ctx, nameAttr, recordKind);
  mInfo.types[pTy.id().id()] = incompleteStruct;
}

void Deserializer::defineCompleteStruct(ModuleInfo &mInfo,
                                        const MLIRType &pTy) {
  assert(pTy.has_cir_struct_type() && "pTy is not of StructType");
  assert(!pTy.cir_struct_type().incomplete() && "incomplete struct received!");
  auto attrName =
    AttrDeserializer::deserializeMLIRStringAttr(mInfo, pTy.cir_struct_type().name());
  auto pRecordKind = pTy.cir_struct_type().kind();
  auto recordKind = deserializeRecordKind(pRecordKind);
  auto packed = pTy.cir_struct_type().packed();
  std::vector<mlir::Type> vecMemberTys;
  for (auto ty : pTy.cir_struct_type().members())
    vecMemberTys.push_back(getType(mInfo, ty));
  auto memberTys = mlir::ArrayRef<mlir::Type>(vecMemberTys);
  auto fullStruct = cir::StructType::get(&mInfo.ctx, memberTys, attrName,
                                         packed, recordKind);
  /* completion will fail if the data is mismatched with preexisting one */
  fullStruct.complete(memberTys, packed);
  mInfo.types[pTy.id().id()] = fullStruct;
}

void Deserializer::aggregateTypes(ModuleInfo &mInfo,
                                  const MLIRModule &pModule) {
  auto types = pModule.types();

  for (auto ty : types) {
    mInfo.serTypes[ty.id().id()] = ty;
    /* initiating incomplete structs beforehand to resolve recursive cases */
    if (ty.type_case() == MLIRType::TypeCase::kCirStructType) {
      defineIncompleteStruct(mInfo, ty);
    }
  }

  for (auto ty : types) {
    /* getType will define the type for us if it isn't yet */
    std::ignore = getType(mInfo, ty.id());
  }

  for (auto ty : types) {
    /* completing the definition of structs */
    if (ty.type_case() == MLIRType::TypeCase::kCirStructType) {
      defineCompleteStruct(mInfo, ty);
    }
  }
}

void Deserializer::deserializeBlock(FunctionInfo &fInfo, const MLIRBlock &pBlock) {
  for (const auto &pOp : pBlock.operations()) {
    auto op = OpDeserializer::deserializeMLIROp(fInfo, fInfo.owner, pOp);
    op->setLoc(AttrDeserializer::deserializeMLIRLocation(fInfo.owner, pOp.location()));
    fInfo.ops[pOp.id().id()] = op;
  }
  for (const auto &arg : pBlock.argument_types()) {
    auto argType = getType(fInfo.owner, arg);
    fInfo.blocks[pBlock.id().id()]->addArgument(argType, fInfo.owner.builder.getUnknownLoc());
  }
}

void Deserializer::deserializeFunc(ModuleInfo &mInfo,
                                   const CIRFunction &pFunc) {
  auto funcInfo = FunctionInfo(mInfo);
  auto funcOp = OpDeserializer::deserializeCIRFuncOp(funcInfo, mInfo, pFunc.info());
  if (pFunc.blocks().block_size() > 0) {
    auto bb = pFunc.blocks().block(0);
    funcInfo.blocks[bb.id().id()] = funcOp.addEntryBlock();
    deserializeBlock(funcInfo, bb);
  }
  for (int bbId = 1; bbId < pFunc.blocks().block_size(); ++bbId) {
    auto bb = pFunc.blocks().block(bbId);
    funcInfo.blocks[bb.id().id()] = funcOp.addBlock();
    deserializeBlock(funcInfo, bb);
  }
  mInfo.module.push_back(funcOp);
  mInfo.funcs[pFunc.id().id()] = &funcOp;
}

mlir::Block *Deserializer::getBlock(FunctionInfo &fInfo, const MLIRBlockID &pBlock) {
  auto blockId = pBlock.id();
  assert(fInfo.blocks.count(blockId) && "blockId is not present in Block cache!");
  return fInfo.blocks.at(blockId);
}

mlir::Value Deserializer::deserializeValue(FunctionInfo &fInfo, const MLIRValue &pValue) {
  switch (pValue.value_case()) {
    case MLIRValue::kOpResult: {
      auto opId = pValue.op_result().owner().id();
      auto resultId = pValue.op_result().result_number();
      assert(fInfo.ops.count(opId) && "opId is not present in Operation cache!");
      return fInfo.ops.at(opId)->getOpResult(resultId);
    } break;
    case MLIRValue::kBlockArgument: {
      auto blockId = pValue.block_argument().owner().id();
      auto argId = pValue.block_argument().arg_number();
      assert(fInfo.blocks.count(blockId) && "blockId is not present in Block cache!");
      return fInfo.blocks.at(blockId)->getArgument(argId);
    } break;
    case MLIRValue::VALUE_NOT_SET:
      llvm_unreachable("Unexpected value case for MLIRValue!");
    break;
  }
}

void Deserializer::deserializeGlobal(ModuleInfo &mInfo, const CIRGlobal &pGlobal) {
  // global ops exist outside of FuncOps
  // however, OpDeserializer expects it as an argument
  // creating an empty instance to keep uniformity
  FunctionInfo emptyFuncInfo(mInfo);
  auto op = OpDeserializer::deserializeCIRGlobalOp(emptyFuncInfo, mInfo, pGlobal.info());
  mInfo.globals[pGlobal.id().id()] = op.getOperation();
}

mlir::ModuleOp Deserializer::deserializeModule(mlir::MLIRContext &ctx,
                                               const MLIRModule &pModule) {
  auto builder = cir::CIRBaseBuilderTy(ctx);
  auto newModule = mlir::ModuleOp::create(builder.getUnknownLoc(),
                                          pModule.id().id());
  cir::CIRDataLayout dataLayout(newModule);

  auto mInfo = ModuleInfo(ctx, builder, dataLayout, newModule);

  aggregateTypes(mInfo, pModule);

  for (const auto &pGlobal : pModule.globals()) {
    deserializeGlobal(mInfo, pGlobal);
  }
  for (const auto &pFunc : pModule.functions()) {
    deserializeFunc(mInfo, pFunc);
  }

  assert(mlir::verify(newModule).succeeded());

  return newModule;
}
