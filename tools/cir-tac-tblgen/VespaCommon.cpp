#include "VespaCommon.h"

#include "mlir/TableGen/Class.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

using namespace vespa;

void AbstractSwitchSource::printCodeBlock(raw_indented_ostream &os,
                                          llvm::StringRef code, int indent) {
  os.indent(indent);
  os.printReindented(code);
  os.unindent();
}

void CppProtoSerializer::dumpSwitchFunc(raw_indented_ostream &os) {
  raw_indented_ostream::DelimitedScope scope(os);
  os << formatv("{0} {1};\n", resTy.protoType, serName);
  if (preCaseBody)
    os.printReindented(preCaseBody.value());
  os << formatv("llvm::TypeSwitch<{0}>({1})\n", resTy.langType, inputName);
  for (auto c : cases) {
    os << formatv(".Case<{0}>([&]({0} {1}) {{\n", c.langType, inputName);
    printCodeBlock(os, c.caseBody);
    os << "})\n";
  }
  os << formatv(".Default([]({0} {1}) {{\n"
                "  {1}.dump();\n"
                "  llvm_unreachable(\"unknown {1} during serialization\");\n"
                "});\n",
                resTy.langType, inputName);
  if (postCaseBody)
    os.printReindented(postCaseBody.value());
  os << formatv("return {0};", serName);
}

void CppProtoDeserializer::dumpSwitchFunc(raw_indented_ostream &os) {
  raw_indented_ostream::DelimitedScope scope(os);
  if (preCaseBody)
    os.printReindented(preCaseBody.value());
  os << formatv("switch ({0}) {{\n", switchExpr);
  {
    raw_indented_ostream::DelimitedScope scope2(os);
    for (auto c : cases) {
      os << formatv("case {0}: {{\n", c.caseValue);
      printCodeBlock(os, c.caseBody);
      os << "} break;\n";
    }
    os << "default:\n";
    os << "  llvm_unreachable(\"NYI\");\n";
    os << "  break;\n";
  }
  os << "}\n";
  if (postCaseBody)
    os.printReindented(postCaseBody.value());
}

Method *CppProtoSerializer::addMethod(std::string methodName,
                                      std::string returnType,
                                      llvm::ArrayRef<MethodParameter> params) {
  return internalClass.addMethod(returnType, methodName, params);
}

Method *CppProtoSerializer::addTranslatorMethod(std::string protoType,
                                                std::string cppType,
                                                std::string methodName) {
  llvm::SmallVector<MethodParameter, 1> param{{cppType, inputName}};
  return addMethod(methodName, protoType, param);
}

Method *
CppProtoDeserializer::addMethod(std::string methodName, std::string returnType,
                                llvm::ArrayRef<MethodParameter> params) {
  std::vector<MethodParameter> staticParams = funcParams;
  for (const auto &param : params) {
    staticParams.emplace_back(param);
  }
  return internalClass.addMethod<Method::Static>(returnType, methodName,
                                                 staticParams);
}

Method *CppProtoDeserializer::addTranslatorMethod(std::string protoType,
                                                  std::string cppType,
                                                  std::string methodName) {
  llvm::SmallVector<MethodParameter, 1> param{{protoType, inputName}};
  return addMethod(methodName, cppType, param);
}

void CppSwitchSource::genCtr() {
  llvm::SmallVector<MethodParameter> ctrParams;
  for (auto field : fields) {
    if (field.isCtrParam)
      ctrParams.emplace_back(field.typ, field.name);
    internalClass.addField(field.typ, field.name);
  }

  auto *ctr = internalClass.addConstructor<Method::Inline>(ctrParams);
  for (auto field : fields) {
    ctr->addMemberInitializer(field.name, field.init);
  }
}

void CppSwitchSource::genClass() {
  if (!fields.empty())
    genCtr();

  auto &mainFuncBody =
      addTranslatorMethod(resTy.protoType, resTy.langType,
                          formatv("{0}{1}", funcName, resTy.protoType))
          ->body();

  dumpSwitchFunc(mainFuncBody.getStream());

  for (auto cas : cases) {
    auto *method =
        addTranslatorMethod(cas.protoType, cas.langType,
                            formatv("{0}{1}", funcName, cas.protoType));
    printCodeBlock(method->body().getStream(), cas.translatorBody);
  }
  internalClass.finalize();
}

const char *const serializerFuncStart = R"(
fun {0}.{1}(): {4}.{2} {{
    val {3} = {4}.{2}.newBuilder())";

const char *const serializerFuncEnd = R"(
    return {0}.build()
})";

std::string KotlinProtoSerializer::getNeededTypeName(llvm::StringRef rawName) {
  if (dropNamespace) {
    if (rawName.starts_with("MLIR"))
      rawName = rawName.drop_front(4);
    if (rawName.starts_with("CIR"))
      rawName = rawName.drop_front(3);
  }
  return rawName.str();
}

void KotlinProtoSerializer::dumpSwitchFunc(llvm::raw_ostream &os) {
  const char *const switchStart = R"(
    when (this) {)";

  const char *const switchCase = R"(
        is {0} -> {1}.set{2}(this.{3}()))";

  const char *const switchEnd = R"(
        else -> error("Unknown switch case for {0}!")
    })";

  os << formatv(serializerFuncStart, className, funcName, resTy.protoType,
                serName, subName);

  os << switchStart;
  for (auto &c : cases) {
    auto protoType = getNeededTypeName(c.protoType);
    auto snakeName = llvm::convertToSnakeFromCamelCase(protoType);
    auto casePtotoTypeAsField =
        llvm::convertToCamelFromSnakeCase(snakeName, true);
    os << formatv(switchCase, c.langType, serName, casePtotoTypeAsField,
                  funcName);
  }
  os << formatv(switchEnd, className);

  os << formatv(serializerFuncEnd, serName, className);
}

void KotlinProtoSerializer::dumpCaseFunc(llvm::raw_ostream &os,
                                         llvm::StringRef typ,
                                         llvm::StringRef body,
                                         llvm::StringRef protoTyp) {
  if (protoTyp.empty())
    protoTyp = typ;
  os << formatv(serializerFuncStart, typ, funcName, protoTyp, serName, subName);
  if (!body.empty())
    os << "\n";
  mlir::raw_indented_ostream indentOs(os);
  printCodeBlock(indentOs, body, 4);
  os << formatv(serializerFuncEnd, serName);
}

void KotlinProtoSerializer::genClassDef(llvm::raw_ostream &os) {
  dumpSwitchFunc(os);
  os << "\n";
  for (auto &m : helpers) {
    dumpCaseFunc(os, m.typ, m.body);
    os << "\n";
  }
  for (auto &c : cases) {
    dumpCaseFunc(os, c.langType, c.translatorBody, c.protoType);
    os << "\n";
  }
}
