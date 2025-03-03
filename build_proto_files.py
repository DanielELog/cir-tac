#!/usr/bin/python3

import os
import subprocess

from enum import Enum


def get_include_args(clangir_path):
    clang_path = os.path.join(clangir_path, "clang", "include")
    mlir_path = os.path.join(clangir_path, "mlir", "include")
    return "-I {0} -I {1}".format(clang_path, mlir_path)


def get_td_path(clangir_path, td_file):
    td_path = os.path.join(clangir_path, "clang", "include", "clang", "CIR", "Dialect", "IR")
    return os.path.join(td_path, td_file)


def run_tblgen_command(clangir_path, subcmd, td_file, result_path):
    tblgen_path = os.path.join(clangir_path, "llvm", "build", "bin", "mlir-tblgen")
    cmd = "{0} {1} --{2} {3} > {4}".format(tblgen_path, get_include_args(clangir_path),
                                           subcmd, get_td_path(clangir_path, td_file), result_path)

    subprocess.run(cmd, check=True, shell=True)


def get_proto_path(file_name):
    return os.path.join("proto", file_name)


def get_decl_path(file_name):
    return os.path.join("include", "cir-tac", file_name)


def get_src_path(file_name):
    return os.path.join("src", file_name)


class CodeType(Enum):
    Proto = 1
    Decl = 2
    Src = 3


MAP_TYPE_TO_PATH_GETTER = {
    CodeType.Proto: get_proto_path,
    CodeType.Decl: get_decl_path,
    CodeType.Src: get_src_path,
}


def gen_file(clangir_path, subcmd, td_file, file_name, file_type):
    file_path = MAP_TYPE_TO_PATH_GETTER[file_type](file_name)
    subprocess.run("rm -f {0}".format(file_path), shell=True)
    run_tblgen_command(clangir_path, subcmd, td_file, file_path)


def gen_all(clangir_path, subcmd, td_file, name):
    cpp_name = str.capitalize(name)
    gen_file(clangir_path, subcmd, td_file, "{0}.proto".format(name), CodeType.Proto)
    gen_file(clangir_path, "{0}-serializer-header".format(subcmd), td_file,
             "{0}Serializer.h".format(cpp_name), CodeType.Decl)
    gen_file(clangir_path, "{0}-serializer-source".format(subcmd), td_file,
             "{0}Serializer.cpp".format(cpp_name), CodeType.Src)
    # gen_file(clangir_path, "{0}-deserializer-header", td_file,
    #          "EnumDeserializer.h", CodeType.Decl)
    # gen_file(clangir_path, "{0}-deserializer-source", td_file,
    #          "EnumDeserializer.cpp", CodeType.Src)


def main():
    if len(sys.argv) != 3:
        print("Expected paths to clangir build and to cir-tac build directories!")
        return -1
    clangir = sys.argv[1]
    cir_tac = sys.argv[2]

    os.chdir(cir_tac)

    gen_file(clangir, "gen-enum-proto-deserializer-header", "CIROps.td",
             "EnumDeserializer.h", CodeType.Decl)
    gen_file(clangir, "gen-enum-proto-deserializer-source", "CIROps.td",
             "EnumDeserializer.cpp", CodeType.Src)

    gen_file(clangir, "gen-op-proto-deserializer-header", "CIROps.td",
             "OpDeserializer.h", CodeType.Decl)
    gen_file(clangir, "gen-op-proto-deserializer-source", "CIROps.td",
             "OpDeserializer.cpp", CodeType.Src)

    gen_file(clangir, "gen-attr-proto-deserializer-source", "MLIRCIRAttrs.td",
             "AttrDeserializer.cpp", CodeType.Src)
    gen_file(clangir, "gen-attr-proto-deserializer-header", "MLIRCIRAttrs.td",
             "AttrDeserializer.h", CodeType.Decl)

    gen_all(clangir, "gen-op-proto", "CIROps.td", "op")
    gen_all(clangir, "gen-enum-proto", "CIROps.td", "enum")
    gen_all(clangir, "gen-type-proto", "MLIRCIRTypes.td", "type")
    gen_all(clangir, "gen-attr-proto", "MLIRCIRAttrs.td", "attr")


if __name__ == "__main__":
    exit(main())
