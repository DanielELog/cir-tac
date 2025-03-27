#!/usr/bin/python3

import os

from enum import Enum
from dataclasses import dataclass


class CodeType(Enum):
    Proto = 1
    Decl = 2
    Src = 3


def get_proto_path(file_name):
    return os.path.join("proto", file_name)


def get_decl_path(file_name):
    return os.path.join("include", "cir-tac", file_name)


def get_src_path(file_name):
    return os.path.join("src", file_name)


MAP_TYPE_TO_PATH_GETTER = {
    CodeType.Proto: get_proto_path,
    CodeType.Decl: get_decl_path,
    CodeType.Src: get_src_path,
}


@dataclass
class TblgenFileInfo:
    subcmd: str
    td: str
    name: str
    typ: CodeType
    path: str = MAP_TYPE_TO_PATH_GETTER[typ](name)


def get_subcmd_file_infos(subcmd, td_file, name, no_deser=False) -> list[TblgenFileInfo]:
    cpp_name = str.capitalize(name)

    files = []
    files.append(
        TblgenFileInfo(
            subcmd,
            td_file,
            "{0}.proto".format(name),
            CodeType.Proto,
        )
    )
    files.append(
        TblgenFileInfo(
            "{0}-serializer-header".format(subcmd),
            td_file,
            "{0}Serializer.h".format(cpp_name),
            CodeType.Decl,
        )
    )
    files.append(
        TblgenFileInfo(
            "{0}-serializer-source".format(subcmd),
            td_file,
            "{0}Serializer.cpp".format(cpp_name),
            CodeType.Src,
        )
    )

    if no_deser:
        return files

    files.append(
        TblgenFileInfo(
            "{0}-deserializer-header".format(subcmd),
            td_file,
            "{0}Deserializer.h".format(cpp_name),
            CodeType.Decl
        )
    )
    files.append(
        TblgenFileInfo(
            "{0}-deserializer-source".format(subcmd),
            td_file,
            "{0}Deserializer.cpp".format(cpp_name),
            CodeType.Src
        )
    )
    return files


def get_tblgen_file_infos() -> list[TblgenFileInfo]:
    file_infos = []
    file_infos += get_subcmd_file_infos("gen-op-proto", "CIROps.td", "op")
    file_infos += get_subcmd_file_infos("gen-enum-proto", "CIROps.td", "enum")
    file_infos += get_subcmd_file_infos(
        "gen-type-proto", "MLIRCIRTypes.td", "type", no_deser=True
    )
    file_infos += get_subcmd_file_infos("gen-attr-proto", "MLIRCIRAttrs.td", "attr")
    return file_infos
