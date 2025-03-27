#!/usr/bin/python3

import os
import subprocess
import sys

import cir_tblgen_util


def get_include_args(clangir_path):
    clang_path = os.path.join(clangir_path, "clang", "include")
    mlir_path = os.path.join(clangir_path, "mlir", "include")
    return "-I {0} -I {1}".format(clang_path, mlir_path)


def get_td_path(clangir_path, td_file):
    td_path = os.path.join(
        clangir_path, "clang", "include", "clang", "CIR", "Dialect", "IR"
    )
    return os.path.join(td_path, td_file)


def run_tblgen_command(clangir_path, subcmd, td_file, result_path):
    tblgen_path = os.path.join("build", "tools", "cir-tac-tblgen", "cir-tac-tblgen")
    cmd = "{0} {1} --{2} {3} > {4}".format(
        tblgen_path,
        get_include_args(clangir_path),
        subcmd,
        get_td_path(clangir_path, td_file),
        result_path,
    )

    subprocess.run(cmd, check=True, shell=True)


def gen_file(clangir_path, result_dir, file_info: cir_tblgen_util.TblgenFileInfo):
    result_path = os.path.join(result_dir, file_info.path)
    subprocess.run("rm -f {0}".format(result_path), shell=True)
    run_tblgen_command(clangir_path, file_info.subcmd, file_info.td, result_path)


def main():
    argc = len(sys.argv)
    if not (3 <= argc <= 4):
        print("Expected paths to clangir, cir-tac directories"
              "and optionally resulting dir!")
        return -1
    clangir = os.path.expanduser(sys.argv[1])
    cir_tac = os.path.expanduser(sys.argv[2])
    result_dir = cir_tac if argc == 3 else os.path.expanduser(sys.argv[3])

    os.chdir(cir_tac)

    for file_info in cir_tblgen_util.get_tblgen_file_infos():
        gen_file(clangir, result_dir, file_info)


if __name__ == "__main__":
    exit(main())
