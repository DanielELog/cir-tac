#!/usr/bin/python3

import os
import sys
import filecmp

import cir_tblgen_util


def main():
    argc = len(sys.argv)
    if argc != 2:
        print("Expected paths to two directories for comparison!")
        return -1
    dir1 = os.path.expanduser(sys.argv[1])
    dir2 = os.path.expanduser(sys.argv[2])

    res = 0
    for file_info in cir_tblgen_util.get_tblgen_file_infos():
        file1 = os.path.join(dir1, file_info.path)
        file2 = os.path.join(dir2, file_info.path)
        if not filecmp.cmp(file1, file2, shallow=False):
            print("[{0}] returned a difference!".format(file_info.path))
            res = 1
    return res


if __name__ == "__main__":
    exit(main())
