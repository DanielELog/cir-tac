#include "cir-tac/Deserializer.h"
#include "cir-tac/EnumsDeserializer.h"

#include <mlir/IR/Verifier.h>

using namespace protocir;

cir::StructType::RecordKind
Deserializer::deserializeRecordKind(CIRRecordKind pKind) {
  switch (pKind) {
    case protocir::CIRRecordKind::RecordKind_Class:
      return cir::StructType::Class;
    case protocir::CIRRecordKind::RecordKind_Union:
      return cir::StructType::Union;
    case protocir::CIRRecordKind::RecordKind_Struct:
      return cir::StructType::Struct;
    default:
      llvm_unreachable("NYI");
  }
}

mlir::Type Deserializer::getType(ModuleInfo &mInfo,
                                 const CIRTypeID &typeId) {
  auto typeIdStr = typeId.id();
  if (mInfo.types.find(typeIdStr) == mInfo.types.end()) {
    defineType(mInfo, mInfo.serTypes.at(typeIdStr));
  }
  auto rTy = mInfo.types.at(typeIdStr);
  return rTy;
}

void Deserializer::defineType(ModuleInfo &mInfo,
                              const CIRType &pTy) {
  auto ctx = &mInfo.ctx;
  mlir::Type rTy;
  switch (pTy.kind_case()) {
    case CIRType::KindCase::kIntType:
      {
        auto ty = pTy.int_type();
        rTy = cir::IntType::get(ctx, ty.width(), ty.is_signed());
      }
      break;
    case CIRType::KindCase::kSingleType:
      rTy = cir::SingleType::get(ctx);
      break;
    case CIRType::KindCase::kDoubleType:
      rTy = cir::DoubleType::get(ctx);
      break;
    case CIRType::KindCase::kFp16Type:
      rTy = cir::FP16Type::get(ctx);
      break;
    case CIRType::KindCase::kBf16Type:
      rTy = cir::BF16Type::get(ctx);
      break;
    case CIRType::KindCase::kFp80Type:
      rTy = cir::FP80Type::get(ctx);
      break;
    case CIRType::KindCase::kFp128Type:
      rTy = cir::FP128Type::get(ctx);
      break;
    case CIRType::KindCase::kLongDoubleType:
      {
        auto underlyingTy =
          getType(mInfo, pTy.long_double_type().underlying());
        rTy = cir::LongDoubleType::get(ctx, underlyingTy);
      }
      break;
    case CIRType::KindCase::kComplexType:
      {
        auto elementTy =
          getType(mInfo, pTy.complex_type().element_ty());
        rTy = cir::ComplexType::get(ctx, elementTy);
      }
      break;
    case CIRType::KindCase::kPointerType:
      {
        auto pointee = getType(mInfo, pTy.pointer_type().pointee());
        if (pTy.pointer_type().has_addr_space()) {
          auto addrSpace = pTy.pointer_type().addr_space();
          auto attr = cir::AddressSpaceAttr::get(ctx, addrSpace);
          rTy = cir::PointerType::get(ctx, pointee, attr);
        }
        else {
          rTy = cir::PointerType::get(ctx, pointee);
        }
      }
      break;
    case CIRType::KindCase::kDataMemberType:
      {
        auto memTy = getType(mInfo, pTy.data_member_type().member_ty());
        auto clsTy = getType(mInfo, pTy.data_member_type().cls_ty());
        assert((mlir::isa<cir::StructType>(clsTy))
                && "clsTy should be a StructType!");
        cir::DataMemberType::get(ctx, memTy,
                                 mlir::cast<cir::StructType>(clsTy));
      }
      break;
    case CIRType::KindCase::kBoolType:
      rTy = cir::BoolType::get(ctx);
      break;
    case CIRType::KindCase::kArrayType:
      {
        auto elTy = getType(mInfo, pTy.array_type().elt_ty());
        auto size = pTy.array_type().size();
        rTy = cir::ArrayType::get(ctx, elTy, size);
      }
      break;
    case CIRType::KindCase::kVectorType:
      {
        auto elTy = getType(mInfo, pTy.vector_type().elt_ty());
        auto size = pTy.vector_type().size();
        rTy = cir::VectorType::get(ctx, elTy, size);
      }
      break;
    case CIRType::KindCase::kFuncType:
      {
        std::vector<mlir::Type> vecInputTys;
        for (auto ty : pTy.func_type().inputs())
          vecInputTys.push_back(getType(mInfo, ty));
        auto inputTys = mlir::ArrayRef<mlir::Type>(vecInputTys);
        auto returnTy = getType(mInfo, pTy.func_type().return_type());
        auto isVarArg = pTy.func_type().var_arg();
        rTy = cir::FuncType::get(ctx, inputTys, returnTy, isVarArg);
      }
      break;
    case CIRType::KindCase::kMethodType:
      {
        auto memTy = getType(mInfo, pTy.method_type().member_func_ty());
        assert((mlir::isa<cir::FuncType>(memTy))
                && "memberFuncTy should be a FuncType!");
        auto clsTy = getType(mInfo, pTy.method_type().cls_ty());
        assert((mlir::isa<cir::StructType>(clsTy))
                && "clsTy should be a StructType!");
        cir::MethodType::get(ctx, mlir::cast<cir::FuncType>(memTy),
                             mlir::cast<cir::StructType>(clsTy));
      }
      break;
    case CIRType::KindCase::kExceptionInfoType:
      rTy = cir::ExceptionInfoType::get(ctx);
      break;
    case CIRType::KindCase::kVoidType:
      rTy = cir::VoidType::get(ctx);
      break;
    case CIRType::KindCase::kStructType:
      llvm_unreachable("Definition of StructTypes should not happen"
                       "inside of a generic defineType!");
    case CIRType::KindCase::KIND_NOT_SET:
      llvm_unreachable("Type kind not set!");
    default:
      llvm::outs() << "CIRType::KindCase set as " << pTy.kind_case();
      llvm_unreachable("NYI");
  }
  auto typeId = pTy.id().id();
  mInfo.types[typeId] = rTy;
}

void Deserializer::defineIncompleteStruct(ModuleInfo &mInfo,
                                          const CIRType &pTy) {
  assert(pTy.has_struct_type() && "pTy must be of StructType");
  auto nameAttr = mlir::StringAttr::get(&mInfo.ctx, pTy.struct_type().name());
  auto recordKind = deserializeRecordKind(pTy.struct_type().kind());
  auto incompleteStruct =
    cir::StructType::get(&mInfo.ctx, nameAttr, recordKind);
  mInfo.types[pTy.id().id()] = incompleteStruct;
}

void Deserializer::defineCompleteStruct(ModuleInfo &mInfo,
                                        const CIRType &pTy) {
  assert(pTy.has_struct_type() && "pTy must be of StructType");
  auto strName = pTy.struct_type().name();
  auto attrName = mlir::StringAttr::get(&mInfo.ctx, strName);
  auto pRecordKind = pTy.struct_type().kind();
  auto recordKind = deserializeRecordKind(pRecordKind);
  auto packed = pTy.struct_type().packed();
  std::vector<mlir::Type> vecMemberTys;
  for (auto ty : pTy.struct_type().members())
    vecMemberTys.push_back(getType(mInfo, ty));
  auto memberTys = mlir::ArrayRef<mlir::Type>(vecMemberTys);
  auto fullStruct = cir::StructType::get(&mInfo.ctx, memberTys, attrName,
                                         packed, recordKind);
  /* completion will fail if the data is mismatched with preexisting one */
  fullStruct.complete(memberTys, packed);
  mInfo.types[pTy.id().id()] = fullStruct;
}

void Deserializer::aggregateTypes(ModuleInfo &mInfo,
                                  const CIRModule &pModule) {
  auto types = pModule.types();

  for (auto ty : types) {
    mInfo.serTypes[ty.id().id()] = ty;
    /* initiating incomplete structs beforehand to resolve recursive cases */
    if (ty.kind_case() == CIRType::KindCase::kStructType) {
      defineIncompleteStruct(mInfo, ty);
    }
  }
  
  for (auto ty : types) {
    /* getType will define the type for us if it isn't yet */
    std::ignore = getType(mInfo, ty.id());
  }

  for (auto ty : types) {
    /* completing the definition of structs */
    if (ty.kind_case() == CIRType::KindCase::kStructType) {
      defineCompleteStruct(mInfo, ty);
    }
  }
}

void Deserializer::deserializeBlock(FunctionInfo &fInfo,
                                    const CIRBlock &pBlock) {
  
}

mlir::Operation *Deserializer::deserializeOp(FunctionInfo &fInfo,
                                             const CIROp &pOp) {
  auto builder = fInfo.owner.builder;
  auto ctx = &fInfo.owner.ctx;
  
  switch (pOp.operation_case()) {
    case CIROp::OperationCase::kFuncOp:
      return deserializeFuncOp(fInfo.owner, pOp.func_op());
    default:
      llvm_unreachable("NYI");
  }
}

cir::FuncOp Deserializer::deserializeFuncOp(ModuleInfo &mInfo,
                                            const CIRFuncOp &pFuncOp) {
  auto builder = mInfo.builder;
  auto ctx = &mInfo.ctx;

  auto type = getType(mInfo, pFuncOp.function_type());
  assert((mlir::isa<cir::FuncType>(type))
    && "FunctionType is not an instance of cir::FuncType!");
  auto op = builder.create<cir::FuncOp>(builder.getUnknownLoc(), pFuncOp.sym_name(),
    mlir::cast<cir::FuncType>(type));

  op.setSymName(pFuncOp.sym_name());
  auto linkage =
    EnumsDeserializer::deserializeGlobalLinkageKind(pFuncOp.linkage());
  op.setLinkage(linkage);

  auto visibility = EnumsDeserializer::deserializeVisibilityKind(
    pFuncOp.global_visibility());
  auto visibilityAttr = cir::VisibilityAttr::get(ctx, visibility);
  op.setGlobalVisibilityAttr(visibilityAttr);

  op.setExtraAttrsAttr(cir::ExtraFuncAttributesAttr::get(
      ctx, builder.getDictionaryAttr({})));

  op.setBuiltin(pFuncOp.builtin());
  op.setCoroutine(pFuncOp.coroutine());
  op.setComdat(pFuncOp.comdat());
  op.setLambda(pFuncOp.lambda());
  op.setNoProto(pFuncOp.lambda());
  op.setDsolocal(pFuncOp.dsolocal());
  
  auto callConv =
    EnumsDeserializer::deserializeCallingConv(pFuncOp.calling_conv());
  op.setCallingConv(callConv);

  if (pFuncOp.has_sym_visibility()) {
    op.setSymVisibility(pFuncOp.sym_visibility());
  }
  if (pFuncOp.has_aliasee()) {
    op.setAliasee(pFuncOp.aliasee());
  }
  // TODO: astAttr
  return op;
}

void Deserializer::deserializeFunc(ModuleInfo &mInfo,
                                   const CIRFunction &pFunc) {
  auto funcInfo = FunctionInfo(mInfo);
  auto funcOp = deserializeFuncOp(mInfo, pFunc.info());
  /*if (pFunc.blocks_size() > 0)
    funcInfo.blocks[pFunc.blocks()[0].id().id()] = funcOp.addEntryBlock();
  for (int bbId = 1; bbId < pFunc.blocks_size(); bbId++) {
    funcInfo.blocks[pFunc.blocks()[bbId].id().id()] = funcOp.addBlock();
  }*/
  for (auto bb : pFunc.blocks()) {
    // deserializeBlock(funcInfo, bb);
  }
  mInfo.module.push_back(funcOp);
  mInfo.funcs[pFunc.id().id()] = &funcOp;
}

mlir::ModuleOp Deserializer::deserializeModule(mlir::MLIRContext &ctx,
                                               const CIRModule &pModule) {
  auto builder = cir::CIRBaseBuilderTy(ctx);
  std::cout << "Deserializing module named: " << pModule.id().id() << std::endl;
  auto newModule = mlir::ModuleOp::create(builder.getUnknownLoc(),
                                          pModule.id().id());
  cir::CIRDataLayout dataLayout(newModule);

  auto mInfo = ModuleInfo(ctx, builder, dataLayout, newModule);

  aggregateTypes(mInfo, pModule);

  for (auto pFunc : pModule.functions()) {
    deserializeFunc(mInfo, pFunc);
  }

  std::cout << "Types present:" << std::endl;
  for (auto ty : pModule.types()) {
    std::cout << ty.kind_case() << std::endl;
  }

  assert(mlir::verify(newModule).succeeded());

  return newModule;
}
