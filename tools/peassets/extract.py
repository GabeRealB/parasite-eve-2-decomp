import logging
import shutil
from argparse import ArgumentParser, FileType
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import BinaryIO
from zlib import crc32


@dataclass
class FolderListEntry:
    folder_id: int
    folder_size: int


@dataclass
class FileListEntry:
    file_id: int
    file_offset: int  # offset in .CDF (from begin of current folder)


class FileChunkType(IntEnum):
    RoomPkg = 0x0  # .pe2pkg
    Image = 0x1  # .pe2img
    Clut = 0x2  # .pe2clut
    Cap2 = 0x4  # .pe2cap2
    RoomBackground = 0x5  # .bs
    Music = 0x6  # .spk
    Ascii = 0x7  # .txt

    def get_name(self) -> str:
        return {
            FileChunkType.RoomPkg: "Room package ",
            FileChunkType.Image: "Image",
            FileChunkType.Clut: "Color lookup table",
            FileChunkType.Cap2: "Dialogue data",
            FileChunkType.RoomBackground: "PSX MDEX bitstream image",
            FileChunkType.Music: "SPK/MPK music program",
            FileChunkType.Ascii: "Ascii text",
        }[self]

    def get_extension(self) -> str:
        return {
            FileChunkType.RoomPkg: ".pe2pkg",
            FileChunkType.Image: ".pe2img",
            FileChunkType.Clut: ".pe2clut",
            FileChunkType.Cap2: ".pe2cap2",
            FileChunkType.RoomBackground: ".bs",
            FileChunkType.Music: ".spk",
            FileChunkType.Ascii: ".txt",
        }[self]

    def is_compressed(self) -> bool:
        return {
            FileChunkType.RoomPkg: True,
            FileChunkType.Image: True,
            FileChunkType.Clut: True,
            FileChunkType.Cap2: False,
            FileChunkType.RoomBackground: False,
            FileChunkType.Music: False,
            FileChunkType.Ascii: False,
        }[self]


class FileChunkEndFlag(IntEnum):
    Continue = 0x01
    End = 0xFF


@dataclass
class FileChunkHeader:
    chunk_type: FileChunkType
    end_flag: FileChunkEndFlag
    unknown1: int
    chunk_size: int
    unknown2: int


@dataclass
class FileChunk:
    header: FileChunkHeader
    data: bytes


@dataclass
class File:
    chunks: list[FileChunk]


class StreamingListStreamType(IntEnum):
    Movie = 0x1
    Audio = 0x2


@dataclass
class StreamingListMovieEntry:
    stream_type: StreamingListStreamType
    unknown1: int
    offset_folder: int  # offset in current Folder of .CDF file (if movie_number == 0)
    offset_inter: int  # offset in INTERx.STR file (if movie_number > 0)
    unknown2: int
    stream_id: int
    stream_sub_id: int
    picture_width: int
    picture_height: int
    unknown3: int
    unknown4: int
    unknown5: int
    unknown6: int
    unknown7: int
    movie_number: int
    unknown8: int


@dataclass
class StreamingListAudioEntry:
    stream_type: StreamingListStreamType
    unknown1: int
    offset_stage: int  # offset in STAGEx.CDF file (increasing offsets)
    unknown2: int
    stage_number: int
    stream_id: int
    stream_sub_id: int
    unknown3: int
    unknown4: int
    unknown5: int
    unknown6: int
    unknown7: int


FOLDER_LIST_ENTRY_SIZE = 0x8
FILE_LIST_ENTRY_SIZE = 0x8
FILE_CHUNK_HEADER_SIZE = 0x10
STREAMING_LIST_ENTRY_SIZE = 0x28

PKG_LIST = {
    r"stage0\file0\1.pe2pkg": "1",
    r"stage0\file1\5.pe2pkg": "2",
    r"stage0\file100300\3.pe2pkg": "3",
    r"stage0\file100400\3.pe2pkg": "4",
    r"stage0\file100500\3.pe2pkg": "5",
    r"stage0\file100700\3.pe2pkg": "6",
    r"stage0\file101000\3.pe2pkg": "7",
    r"stage0\file101100\2.pe2pkg": "8",
    r"stage0\file101200\3.pe2pkg": "9",
    r"stage0\file101300\3.pe2pkg": "10",
    r"stage0\file101500\3.pe2pkg": "11",
    r"stage0\file101600\3.pe2pkg": "12",
    r"stage0\file101900\3.pe2pkg": "13",
    r"stage0\file10200\2.pe2pkg": "14",
    r"stage0\file102000\3.pe2pkg": "15",
    r"stage0\file102100\3.pe2pkg": "16",
    r"stage0\file102300\3.pe2pkg": "17",
    r"stage0\file102400\3.pe2pkg": "18",
    r"stage0\file102500\3.pe2pkg": "19",
    r"stage0\file102600\3.pe2pkg": "20",
    r"stage0\file10300\2.pe2pkg": "21",
    r"stage0\file10301\3.pe2pkg": "22",
    r"stage0\file10302\3.pe2pkg": "23",
    r"stage0\file10303\3.pe2pkg": "24",
    r"stage0\file10304\3.pe2pkg": "25",
    r"stage0\file10305\3.pe2pkg": "26",
    r"stage0\file10306\2.pe2pkg": "27",
    r"stage0\file10307\2.pe2pkg": "28",
    r"stage0\file10308\2.pe2pkg": "29",
    r"stage0\file10309\3.pe2pkg": "30",
    r"stage0\file10310\2.pe2pkg": "31",
    r"stage0\file10311\2.pe2pkg": "32",
    r"stage0\file10312\2.pe2pkg": "33",
    r"stage0\file10313\2.pe2pkg": "34",
    r"stage0\file10314\2.pe2pkg": "35",
    r"stage0\file10315\2.pe2pkg": "36",
    r"stage0\file10316\3.pe2pkg": "37",
    r"stage0\file10317\3.pe2pkg": "38",
    r"stage0\file10318\2.pe2pkg": "39",
    r"stage0\file10319\3.pe2pkg": "40",
    r"stage0\file10320\3.pe2pkg": "41",
    r"stage0\file10321\3.pe2pkg": "42",
    r"stage0\file10322\3.pe2pkg": "43",
    r"stage0\file10323\2.pe2pkg": "44",
    r"stage0\file10324\2.pe2pkg": "45",
    r"stage0\file10325\3.pe2pkg": "46",
    r"stage0\file10326\3.pe2pkg": "47",
    r"stage0\file10327\2.pe2pkg": "48",
    r"stage0\file10328\3.pe2pkg": "49",
    r"stage0\file10329\3.pe2pkg": "50",
    r"stage0\file10330\3.pe2pkg": "51",
    r"stage0\file10331\3.pe2pkg": "52",
    r"stage0\file10332\3.pe2pkg": "53",
    r"stage0\file103700\3.pe2pkg": "54",
    r"stage0\file103800\3.pe2pkg": "55",
    r"stage0\file10400\2.pe2pkg": "56",
    r"stage0\file104000\3.pe2pkg": "57",
    r"stage0\file104100\3.pe2pkg": "58",
    r"stage0\file104400\2.pe2pkg": "59",
    r"stage0\file104500\2.pe2pkg": "60",
    r"stage0\file104600\3.pe2pkg": "61",
    r"stage0\file104900\3.pe2pkg": "62",
    r"stage0\file10500\2.pe2pkg": "63",
    r"stage0\file105100\3.pe2pkg": "64",
    r"stage0\file105300\3.pe2pkg": "65",
    r"stage0\file105400\3.pe2pkg": "66",
    r"stage0\file105500\3.pe2pkg": "67",
    r"stage0\file105600\3.pe2pkg": "68",
    r"stage0\file105700\3.pe2pkg": "69",
    r"stage0\file107000\3.pe2pkg": "70",
    r"stage0\file107200\3.pe2pkg": "71",
    r"stage0\file107500\3.pe2pkg": "72",
    r"stage0\file107600\6.pe2pkg": "73",
    r"stage0\file109300\2.pe2pkg": "74",
    r"stage0\file110300\4.pe2pkg": "75",
    r"stage0\file110600\3.pe2pkg": "76",
    r"stage0\file110700\3.pe2pkg": "77",
    r"stage0\file110800\2.pe2pkg": "78",
    r"stage0\file111800\3.pe2pkg": "79",
    r"stage0\file113000\2.pe2pkg": "80",
    r"stage0\file113100\2.pe2pkg": "81",
    r"stage0\file120300\4.pe2pkg": "82",
    r"stage0\file120400\2.pe2pkg": "83",
    r"stage0\file120500\2.pe2pkg": "84",
    r"stage0\file121300\2.pe2pkg": "85",
    r"stage0\file123000\3.pe2pkg": "86",
    r"stage0\file123200\2.pe2pkg": "87",
    r"stage0\file123400\3.pe2pkg": "88",
    r"stage0\file135000\2.pe2pkg": "89",
    r"stage0\file135400\2.pe2pkg": "90",
    r"stage0\file135600\2.pe2pkg": "91",
    r"stage0\file136100\4.pe2pkg": "92",
    r"stage0\file136300\1.pe2pkg": "93",
    r"stage0\file140400\2.pe2pkg": "94",
    r"stage0\file141000\4.pe2pkg": "95",
    r"stage0\file142600\0.pe2pkg": "96",
    r"stage0\file142900\0.pe2pkg": "97",
    r"stage0\file143000\1.pe2pkg": "98",
    r"stage0\file143400\0.pe2pkg": "99",
    r"stage0\file143900\2.pe2pkg": "100",
    r"stage0\file146000\0.pe2pkg": "101",
    r"stage0\file146300\2.pe2pkg": "102",
    r"stage0\file150400\3.pe2pkg": "103",
    r"stage0\file151000\2.pe2pkg": "104",
    r"stage0\file160600\2.pe2pkg": "105",
    r"stage0\file160700\2.pe2pkg": "106",
    r"stage0\file160900\0.pe2pkg": "107",
    r"stage0\file161500\4.pe2pkg": "108",
    r"stage0\file200300\3.pe2pkg": "109",
    r"stage0\file200400\3.pe2pkg": "110",
    r"stage0\file200700\3.pe2pkg": "111",
    r"stage0\file20100\0.pe2pkg": "112",
    r"stage0\file201000\3.pe2pkg": "113",
    r"stage0\file201100\2.pe2pkg": "114",
    r"stage0\file201200\3.pe2pkg": "115",
    r"stage0\file201300\3.pe2pkg": "116",
    r"stage0\file201500\3.pe2pkg": "117",
    r"stage0\file201600\3.pe2pkg": "118",
    r"stage0\file201800\3.pe2pkg": "119",
    r"stage0\file201900\3.pe2pkg": "120",
    r"stage0\file20200\0.pe2pkg": "121",
    r"stage0\file202000\3.pe2pkg": "122",
    r"stage0\file202100\3.pe2pkg": "123",
    r"stage0\file202300\3.pe2pkg": "124",
    r"stage0\file202400\3.pe2pkg": "125",
    r"stage0\file202500\3.pe2pkg": "126",
    r"stage0\file202600\3.pe2pkg": "127",
    r"stage0\file202900\2.pe2pkg": "128",
    r"stage0\file203700\3.pe2pkg": "129",
    r"stage0\file203800\3.pe2pkg": "130",
    r"stage0\file204000\3.pe2pkg": "131",
    r"stage0\file204100\3.pe2pkg": "132",
    r"stage0\file204400\2.pe2pkg": "133",
    r"stage0\file204600\3.pe2pkg": "134",
    r"stage0\file204900\3.pe2pkg": "135",
    r"stage0\file205200\1.pe2pkg": "136",
    r"stage0\file205500\3.pe2pkg": "137",
    r"stage0\file205600\3.pe2pkg": "138",
    r"stage0\file205700\3.pe2pkg": "139",
    r"stage0\file20600\0.pe2pkg": "140",
    r"stage0\file206100\3.pe2pkg": "141",
    r"stage0\file207000\3.pe2pkg": "142",
    r"stage0\file207200\3.pe2pkg": "143",
    r"stage0\file20900\4.pe2pkg": "144",
    r"stage0\file21000\6.pe2pkg": "145",
    r"stage0\file210600\2.pe2pkg": "146",
    r"stage0\file210700\2.pe2pkg": "147",
    r"stage0\file213000\2.pe2pkg": "148",
    r"stage0\file213100\2.pe2pkg": "149",
    r"stage0\file215100\2.pe2pkg": "150",
    r"stage0\file223600\3.pe2pkg": "151",
    r"stage0\file260400\3.pe2pkg": "152",
    r"stage0\file260500\3.pe2pkg": "153",
    r"stage0\file300300\3.pe2pkg": "154",
    r"stage0\file300400\3.pe2pkg": "155",
    r"stage0\file300700\3.pe2pkg": "156",
    r"stage0\file30100\2.pe2pkg": "157",
    r"stage0\file301000\3.pe2pkg": "158",
    r"stage0\file30102\2.pe2pkg": "159",
    r"stage0\file301100\2.pe2pkg": "160",
    r"stage0\file301200\3.pe2pkg": "161",
    r"stage0\file301300\3.pe2pkg": "162",
    r"stage0\file301500\3.pe2pkg": "163",
    r"stage0\file301600\3.pe2pkg": "164",
    r"stage0\file301800\3.pe2pkg": "165",
    r"stage0\file301900\3.pe2pkg": "166",
    r"stage0\file30200\2.pe2pkg": "167",
    r"stage0\file302000\3.pe2pkg": "168",
    r"stage0\file30202\2.pe2pkg": "169",
    r"stage0\file30203\2.pe2pkg": "170",
    r"stage0\file302100\3.pe2pkg": "171",
    r"stage0\file302300\3.pe2pkg": "172",
    r"stage0\file302400\3.pe2pkg": "173",
    r"stage0\file302500\3.pe2pkg": "174",
    r"stage0\file302600\3.pe2pkg": "175",
    r"stage0\file30300\2.pe2pkg": "176",
    r"stage0\file30302\2.pe2pkg": "177",
    r"stage0\file30303\2.pe2pkg": "178",
    r"stage0\file303600\0.pe2pkg": "179",
    r"stage0\file303700\3.pe2pkg": "180",
    r"stage0\file303800\3.pe2pkg": "181",
    r"stage0\file30400\2.pe2pkg": "182",
    r"stage0\file304000\3.pe2pkg": "183",
    r"stage0\file30402\2.pe2pkg": "184",
    r"stage0\file30403\2.pe2pkg": "185",
    r"stage0\file30404\2.pe2pkg": "186",
    r"stage0\file30405\2.pe2pkg": "187",
    r"stage0\file30406\2.pe2pkg": "188",
    r"stage0\file304100\3.pe2pkg": "189",
    r"stage0\file304400\2.pe2pkg": "190",
    r"stage0\file304600\3.pe2pkg": "191",
    r"stage0\file304900\3.pe2pkg": "192",
    r"stage0\file30500\2.pe2pkg": "193",
    r"stage0\file30502\2.pe2pkg": "194",
    r"stage0\file30503\2.pe2pkg": "195",
    r"stage0\file305500\3.pe2pkg": "196",
    r"stage0\file305600\3.pe2pkg": "197",
    r"stage0\file305700\3.pe2pkg": "198",
    r"stage0\file307000\3.pe2pkg": "199",
    r"stage0\file307200\3.pe2pkg": "200",
    r"stage0\file310100\0.pe2pkg": "201",
    r"stage0\file310600\4.pe2pkg": "202",
    r"stage0\file311500\3.pe2pkg": "203",
    r"stage0\file311900\2.pe2pkg": "204",
    r"stage0\file312200\2.pe2pkg": "205",
    r"stage0\file317000\2.pe2pkg": "206",
    r"stage0\file323000\2.pe2pkg": "207",
    r"stage0\file323300\2.pe2pkg": "208",
    r"stage0\file323400\2.pe2pkg": "209",
    r"stage0\file335800\4.pe2pkg": "210",
    r"stage0\file341300\0.pe2pkg": "211",
    r"stage0\file341700\0.pe2pkg": "212",
    r"stage0\file341900\0.pe2pkg": "213",
    r"stage0\file342000\0.pe2pkg": "214",
    r"stage0\file342100\0.pe2pkg": "215",
    r"stage0\file342200\0.pe2pkg": "216",
    r"stage0\file342400\2.pe2pkg": "217",
    r"stage0\file350500\2.pe2pkg": "218",
    r"stage0\file350700\4.pe2pkg": "219",
    r"stage0\file356100\2.pe2pkg": "220",
    r"stage0\file361100\2.pe2pkg": "221",
    r"stage0\file400100\3.pe2pkg": "222",
    r"stage0\file400500\3.pe2pkg": "223",
    r"stage0\file400600\2.pe2pkg": "224",
    r"stage0\file401000\3.pe2pkg": "225",
    r"stage0\file401300\3.pe2pkg": "226",
    r"stage0\file401800\3.pe2pkg": "227",
    r"stage0\file402200\2.pe2pkg": "228",
    r"stage0\file403000\5.pe2pkg": "229",
    r"stage0\file403100\5.pe2pkg": "230",
    r"stage0\file403200\5.pe2pkg": "231",
    r"stage0\file403600\0.pe2pkg": "232",
    r"stage0\file403900\2.pe2pkg": "233",
    r"stage0\file405800\3.pe2pkg": "234",
    r"stage0\file407500\3.pe2pkg": "235",
    r"stage0\file420700\4.pe2pkg": "236",
    r"stage0\file421600\3.pe2pkg": "237",
    r"stage0\file443500\2.pe2pkg": "238",
    r"stage0\file444000\5.pe2pkg": "239",
    r"stage0\file450200\2.pe2pkg": "240",
    r"stage0\file450800\2.pe2pkg": "241",
    r"stage0\file450900\0.pe2pkg": "242",
    r"stage0\file451100\2.pe2pkg": "243",
    r"stage0\file460200\2.pe2pkg": "244",
    r"stage0\file461800\2.pe2pkg": "245",
    r"stage0\file50100\1.pe2pkg": "246",
    r"stage0\file50104\1.pe2pkg": "247",
    r"stage0\file50107\1.pe2pkg": "248",
    r"stage0\file50110\1.pe2pkg": "249",
    r"stage0\file50113\1.pe2pkg": "250",
    r"stage0\file50116\1.pe2pkg": "251",
    r"stage0\file50119\1.pe2pkg": "252",
    r"stage0\file50122\1.pe2pkg": "253",
    r"stage0\file50125\1.pe2pkg": "254",
    r"stage0\file50128\1.pe2pkg": "255",
    r"stage0\file50131\1.pe2pkg": "256",
    r"stage0\file50134\1.pe2pkg": "257",
    r"stage0\file50146\1.pe2pkg": "258",
    r"stage0\file50149\1.pe2pkg": "259",
    r"stage0\file50152\1.pe2pkg": "260",
    r"stage0\file503500\7.pe2pkg": "261",
    r"stage0\file510900\4.pe2pkg": "262",
    r"stage0\file511000\4.pe2pkg": "263",
    r"stage0\file521100\4.pe2pkg": "264",
    r"stage0\file535700\2.pe2pkg": "265",
    r"stage0\file548100\0.pe2pkg": "266",
    r"stage0\file560800\4.pe2pkg": "267",
    r"stage0\file640400\2.pe2pkg": "268",
    r"stage0\file800100\0.pe2pkg": "269",
    r"stage0\file800101\5.pe2pkg": "270",
    r"stage0\file800102\5.pe2pkg": "271",
    r"stage0\file800103\5.pe2pkg": "272",
    r"stage0\file800104\5.pe2pkg": "273",
    r"stage0\file800200\3.pe2pkg": "274",
    r"stage0\file800300\4.pe2pkg": "275",
    r"stage0\file900000\6.pe2pkg": "276",
    r"stage0\file900002\6.pe2pkg": "277",
    r"stage0\file900003\7.pe2pkg": "278",
    r"stage0\file900004\2.pe2pkg": "279",
    r"stage0\file900005\2.pe2pkg": "280",
    r"stage1\1001\file0\4.pe2pkg": "281",
    r"stage1\101\file0\3.pe2pkg": "282",
    r"stage1\1101\file0\6.pe2pkg": "283",
    r"stage1\1201\file0\7.pe2pkg": "284",
    r"stage1\1301\file0\7.pe2pkg": "285",
    r"stage1\1401\file0\6.pe2pkg": "286",
    r"stage1\1501\file0\3.pe2pkg": "287",
    r"stage1\1601\file0\6.pe2pkg": "288",
    r"stage1\1701\file0\7.pe2pkg": "289",
    r"stage1\1801\file0\6.pe2pkg": "290",
    r"stage1\1901\file0\7.pe2pkg": "291",
    r"stage1\2001\file0\5.pe2pkg": "292",
    r"stage1\201\file0\3.pe2pkg": "293",
    r"stage1\2101\file0\1.pe2pkg": "294",
    r"stage1\301\file0\5.pe2pkg": "295",
    r"stage1\401\file0\8.pe2pkg": "296",
    r"stage1\501\file0\5.pe2pkg": "297",
    r"stage1\601\file0\5.pe2pkg": "298",
    r"stage1\701\file0\5.pe2pkg": "299",
    r"stage1\801\file0\5.pe2pkg": "300",
    r"stage1\901\file0\5.pe2pkg": "301",
    r"stage2\101\file0\3.pe2pkg": "302",
    r"stage2\1101\file0\3.pe2pkg": "303",
    r"stage2\1201\file0\3.pe2pkg": "304",
    r"stage2\1301\file0\3.pe2pkg": "305",
    r"stage2\1401\file0\3.pe2pkg": "306",
    r"stage2\1501\file0\3.pe2pkg": "307",
    r"stage2\1601\file0\5.pe2pkg": "308",
    r"stage2\1701\file0\3.pe2pkg": "309",
    r"stage2\1801\file0\5.pe2pkg": "310",
    r"stage2\1901\file0\3.pe2pkg": "311",
    r"stage2\2001\file0\5.pe2pkg": "312",
    r"stage2\201\file0\5.pe2pkg": "313",
    r"stage2\2101\file0\6.pe2pkg": "314",
    r"stage2\2201\file0\5.pe2pkg": "315",
    r"stage2\2301\file0\5.pe2pkg": "316",
    r"stage2\2401\file0\3.pe2pkg": "317",
    r"stage2\2501\file0\3.pe2pkg": "318",
    r"stage2\2601\file0\5.pe2pkg": "319",
    r"stage2\2701\file0\5.pe2pkg": "320",
    r"stage2\2801\file0\3.pe2pkg": "321",
    r"stage2\2901\file0\3.pe2pkg": "322",
    r"stage2\3001\file0\3.pe2pkg": "323",
    r"stage2\301\file0\3.pe2pkg": "324",
    r"stage2\3101\file0\3.pe2pkg": "325",
    r"stage2\3201\file0\5.pe2pkg": "326",
    r"stage2\3401\file0\5.pe2pkg": "327",
    r"stage2\3801\file0\5.pe2pkg": "328",
    r"stage2\401\file0\1.pe2pkg": "329",
    r"stage2\501\file0\3.pe2pkg": "330",
    r"stage2\601\file0\3.pe2pkg": "331",
    r"stage2\701\file0\3.pe2pkg": "332",
    r"stage2\801\file0\5.pe2pkg": "333",
    r"stage2\901\file0\3.pe2pkg": "334",
    r"stage3\101\file0\5.pe2pkg": "335",
    r"stage3\1101\file0\5.pe2pkg": "336",
    r"stage3\1201\file0\5.pe2pkg": "337",
    r"stage3\1301\file0\5.pe2pkg": "338",
    r"stage3\1401\file0\5.pe2pkg": "339",
    r"stage3\1501\file0\5.pe2pkg": "340",
    r"stage3\1601\file0\5.pe2pkg": "341",
    r"stage3\1701\file0\5.pe2pkg": "342",
    r"stage3\1801\file0\6.pe2pkg": "343",
    r"stage3\1901\file0\3.pe2pkg": "344",
    r"stage3\2001\file0\5.pe2pkg": "345",
    r"stage3\201\file0\5.pe2pkg": "346",
    r"stage3\2101\file0\5.pe2pkg": "347",
    r"stage3\2201\file0\5.pe2pkg": "348",
    r"stage3\2301\file0\5.pe2pkg": "349",
    r"stage3\2401\file0\5.pe2pkg": "350",
    r"stage3\2501\file0\3.pe2pkg": "351",
    r"stage3\2601\file0\5.pe2pkg": "352",
    r"stage3\2701\file0\5.pe2pkg": "353",
    r"stage3\2801\file0\5.pe2pkg": "354",
    r"stage3\2901\file0\6.pe2pkg": "355",
    r"stage3\3001\file0\5.pe2pkg": "356",
    r"stage3\301\file0\3.pe2pkg": "357",
    r"stage3\3101\file0\6.pe2pkg": "358",
    r"stage3\3201\file0\5.pe2pkg": "359",
    r"stage3\3401\file0\5.pe2pkg": "360",
    r"stage3\3801\file0\5.pe2pkg": "361",
    r"stage3\501\file0\5.pe2pkg": "362",
    r"stage3\601\file0\3.pe2pkg": "363",
    r"stage3\701\file0\3.pe2pkg": "364",
    r"stage3\801\file0\3.pe2pkg": "365",
    r"stage3\901\file0\3.pe2pkg": "366",
    r"stage4\1001\file0\3.pe2pkg": "367",
    r"stage4\101\file0\5.pe2pkg": "368",
    r"stage4\1101\file0\3.pe2pkg": "369",
    r"stage4\1201\file0\3.pe2pkg": "370",
    r"stage4\1301\file0\3.pe2pkg": "371",
    r"stage4\1401\file0\6.pe2pkg": "372",
    r"stage4\1501\file0\5.pe2pkg": "373",
    r"stage4\1601\file0\8.pe2pkg": "374",
    r"stage4\1701\file0\3.pe2pkg": "375",
    r"stage4\1801\file0\5.pe2pkg": "376",
    r"stage4\1901\file0\3.pe2pkg": "377",
    r"stage4\2001\file0\6.pe2pkg": "378",
    r"stage4\201\file0\7.pe2pkg": "379",
    r"stage4\2101\file0\5.pe2pkg": "380",
    r"stage4\2201\file0\9.pe2pkg": "381",
    r"stage4\2301\file0\5.pe2pkg": "382",
    r"stage4\2401\file0\3.pe2pkg": "383",
    r"stage4\2501\file0\3.pe2pkg": "384",
    r"stage4\2601\file0\5.pe2pkg": "385",
    r"stage4\2701\file0\3.pe2pkg": "386",
    r"stage4\2801\file0\3.pe2pkg": "387",
    r"stage4\2901\file0\3.pe2pkg": "388",
    r"stage4\3001\file0\3.pe2pkg": "389",
    r"stage4\301\file0\5.pe2pkg": "390",
    r"stage4\3101\file0\6.pe2pkg": "391",
    r"stage4\3201\file0\6.pe2pkg": "392",
    r"stage4\3301\file0\5.pe2pkg": "393",
    r"stage4\3401\file0\5.pe2pkg": "394",
    r"stage4\3501\file0\5.pe2pkg": "395",
    r"stage4\3601\file0\8.pe2pkg": "396",
    r"stage4\3701\file0\1.pe2pkg": "397",
    r"stage4\3801\file0\1.pe2pkg": "398",
    r"stage4\3901\file0\8.pe2pkg": "399",
    r"stage4\4001\file0\7.pe2pkg": "400",
    r"stage4\401\file0\5.pe2pkg": "401",
    r"stage4\4101\file0\3.pe2pkg": "402",
    r"stage4\4201\file0\3.pe2pkg": "403",
    r"stage4\4301\file0\5.pe2pkg": "404",
    r"stage4\4401\file0\5.pe2pkg": "405",
    r"stage4\4501\file0\6.pe2pkg": "406",
    r"stage4\4601\file0\5.pe2pkg": "407",
    r"stage4\4701\file0\5.pe2pkg": "408",
    r"stage4\4801\file0\5.pe2pkg": "409",
    r"stage4\4901\file0\2.pe2pkg": "410",
    r"stage4\501\file0\5.pe2pkg": "411",
    r"stage4\601\file0\6.pe2pkg": "412",
    r"stage4\701\file0\7.pe2pkg": "413",
    r"stage4\801\file0\5.pe2pkg": "414",
    r"stage4\901\file0\5.pe2pkg": "415",
    r"stage5\1001\file0\3.pe2pkg": "416",
    r"stage5\101\file0\3.pe2pkg": "417",
    r"stage5\1101\file0\5.pe2pkg": "418",
    r"stage5\1201\file0\5.pe2pkg": "419",
    r"stage5\1301\file0\5.pe2pkg": "420",
    r"stage5\1401\file0\5.pe2pkg": "421",
    r"stage5\1501\file0\5.pe2pkg": "422",
    r"stage5\1601\file0\3.pe2pkg": "423",
    r"stage5\1701\file0\3.pe2pkg": "424",
    r"stage5\1801\file0\3.pe2pkg": "425",
    r"stage5\1901\file0\3.pe2pkg": "426",
    r"stage5\2001\file0\3.pe2pkg": "427",
    r"stage5\201\file0\5.pe2pkg": "428",
    r"stage5\2101\file0\5.pe2pkg": "429",
    r"stage5\2201\file0\6.pe2pkg": "430",
    r"stage5\2301\file0\5.pe2pkg": "431",
    r"stage5\2401\file0\3.pe2pkg": "432",
    r"stage5\2501\file0\5.pe2pkg": "433",
    r"stage5\2601\file0\4.pe2pkg": "434",
    r"stage5\2701\file0\5.pe2pkg": "435",
    r"stage5\2801\file0\4.pe2pkg": "436",
    r"stage5\2901\file0\5.pe2pkg": "437",
    r"stage5\3001\file0\5.pe2pkg": "438",
    r"stage5\301\file0\3.pe2pkg": "439",
    r"stage5\3101\file0\1.pe2pkg": "440",
    r"stage5\3201\file0\3.pe2pkg": "441",
    r"stage5\3301\file0\3.pe2pkg": "442",
    r"stage5\401\file0\5.pe2pkg": "443",
    r"stage5\501\file0\3.pe2pkg": "444",
    r"stage5\601\file0\3.pe2pkg": "445",
    r"stage5\701\file0\6.pe2pkg": "446",
    r"stage5\801\file0\3.pe2pkg": "447",
    r"stage5\901\file0\3.pe2pkg": "448",
}


def computeChecksum(exe: BinaryIO):
    exe.seek(0)
    return crc32(exe.read(4096))


def parse_folder_list_entry(data: bytes) -> FolderListEntry:
    assert len(data) == FOLDER_LIST_ENTRY_SIZE
    return FolderListEntry(
        folder_id=int.from_bytes(data[0x0:0x4], byteorder="little"),
        folder_size=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
    )


def parse_file_list_entry(data: bytes) -> FileListEntry:
    assert len(data) == FILE_LIST_ENTRY_SIZE
    return FileListEntry(
        file_id=int.from_bytes(data[0x0:0x4], byteorder="little"),
        file_offset=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
    )


def parse_file_chunk_header(data: bytes) -> FileChunkHeader | None:
    assert len(data) == FILE_CHUNK_HEADER_SIZE
    chunk_type = int.from_bytes(data[0x0:0x1], byteorder="little")
    match chunk_type:
        case (
            FileChunkType.RoomPkg
            | FileChunkType.Image
            | FileChunkType.Clut
            | FileChunkType.Cap2
            | FileChunkType.RoomBackground
            | FileChunkType.Music
            | FileChunkType.Ascii
        ):
            pass
        case _:
            return None

    assert int.from_bytes(data[0xC:0x10], byteorder="little") == 0
    return FileChunkHeader(
        chunk_type=FileChunkType(chunk_type),
        end_flag=FileChunkEndFlag.from_bytes(data[0x1:0x2], byteorder="little"),
        unknown1=int.from_bytes(data[0x2:0x4], byteorder="little"),
        chunk_size=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
        unknown2=int.from_bytes(data[0x8:0xC], byteorder="little"),
    )


def parse_streaming_list_entry(
    data: bytes,
) -> StreamingListMovieEntry | StreamingListAudioEntry:
    assert len(data) == STREAMING_LIST_ENTRY_SIZE
    stream_type = StreamingListStreamType.from_bytes(data[0:0x2], byteorder="little")
    match stream_type:
        case StreamingListStreamType.Movie:
            return StreamingListMovieEntry(
                stream_type=stream_type,
                unknown1=int.from_bytes(data[0x2:0x4], byteorder="little"),
                offset_folder=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
                offset_inter=int.from_bytes(data[0x8:0xC], byteorder="little") * 0x800,
                unknown2=int.from_bytes(data[0xC:0xE], byteorder="little"),
                stream_id=int.from_bytes(data[0xE:0x10], byteorder="little"),
                stream_sub_id=int.from_bytes(data[0x10:0x12], byteorder="little"),
                picture_width=int.from_bytes(data[0x12:0x14], byteorder="little"),
                picture_height=int.from_bytes(data[0x14:0x16], byteorder="little"),
                unknown3=int.from_bytes(data[0x16:0x18], byteorder="little"),
                unknown4=int.from_bytes(data[0x18:0x1A], byteorder="little"),
                unknown5=int.from_bytes(data[0x1A:0x1C], byteorder="little"),
                unknown6=int.from_bytes(data[0x1C:0x22], byteorder="little"),
                unknown7=int.from_bytes(data[0x22:0x24], byteorder="little"),
                movie_number=int.from_bytes(data[0x24:0x26], byteorder="little"),
                unknown8=int.from_bytes(data[0x26:0x28], byteorder="little"),
            )
        case StreamingListStreamType.Audio:
            return StreamingListAudioEntry(
                stream_type=stream_type,
                unknown1=int.from_bytes(data[0x2:0x4], byteorder="little"),
                offset_stage=int.from_bytes(data[0x4:0x8], byteorder="little") * 0x800,
                unknown2=int.from_bytes(data[0x8:0xC], byteorder="little"),
                stage_number=int.from_bytes(data[0xC:0xE], byteorder="little"),
                stream_id=int.from_bytes(data[0xE:0x10], byteorder="little"),
                stream_sub_id=int.from_bytes(data[0x10:0x14], byteorder="little"),
                unknown3=int.from_bytes(data[0x14:0x16], byteorder="little"),
                unknown4=int.from_bytes(data[0x16:0x18], byteorder="little"),
                unknown5=int.from_bytes(data[0x18:0x1C], byteorder="little"),
                unknown6=int.from_bytes(data[0x1C:0x20], byteorder="little"),
                unknown7=int.from_bytes(data[0x20:0x28], byteorder="little"),
            )


def read_chunk(data: BinaryIO) -> FileChunk | None:
    header = parse_file_chunk_header(data.read(FILE_CHUNK_HEADER_SIZE))
    if header is None:
        return None
    chunk_data = data.read(header.chunk_size - FILE_CHUNK_HEADER_SIZE)
    return FileChunk(header=header, data=chunk_data)


def read_file(data: BinaryIO, offset: int) -> File | None:
    current_pos = data.tell()
    data.seek(offset)

    chunks = []
    while True:
        chunk = read_chunk(data)
        if chunk is None:
            break

        chunks.append(chunk)
        if chunk.header.end_flag == FileChunkEndFlag.End:
            break

    data.seek(current_pos)
    return File(chunks=chunks)


def decode_lzss(stream: bytes) -> bytes:
    # Based on the lzss decoder by md_hyena
    DICT_SIZE = 256
    OFFSET_BITS = 8
    STRING_LEN_BITS = 4
    LIT_SIZE = 8
    DICT_COR = 1

    image_size = 0
    dict = bytearray(DICT_SIZE)
    output = bytearray()

    buf = 0
    mask = 0
    ibcar = 0  # Input buffer carriage
    dictcar = 0  # Dictionary carriage

    def get_bit(n: int) -> int:
        x = 0
        nonlocal mask
        nonlocal buf
        nonlocal ibcar
        nonlocal stream
        for _ in range(n):
            if mask == 0:
                buf = stream[ibcar]
                ibcar += 1
                mask = 128
            x <<= 1
            if buf & mask != 0:
                x += 1
            mask >>= 1

        return x

    def write_to_dict(byte: int):
        nonlocal dict
        nonlocal dictcar
        assert byte < 256
        dict[dictcar] = byte
        dictcar = (dictcar + 1) & 0xFF

    def write_to_output(byte: int):
        nonlocal output
        nonlocal image_size
        assert byte < 256
        output.append(byte)
        image_size += 1

    def is_eos() -> bool:
        nonlocal ibcar
        return len(stream) - 1 == ibcar

    def skip_zeroes() -> bool:
        if is_eos():
            return True

        nonlocal stream
        nonlocal ibcar
        while stream[ibcar] == 0 and not is_eos():
            ibcar += 1

        return False

    def unpack():
        nonlocal dict
        nonlocal dictcar
        nonlocal buf
        nonlocal mask

        for i in range(len(dict)):
            dict[i] = 0
        dictcar = 0

        buf = 0
        mask = 0

        while True:
            if get_bit(1) != 0:
                lit = get_bit(LIT_SIZE)
                write_to_dict(lit)
                write_to_output(lit)
            else:
                offset = get_bit(OFFSET_BITS)
                if offset == 0:
                    break
                length = get_bit(STRING_LEN_BITS)

                if offset != 0:
                    offset -= DICT_COR
                else:
                    offset = 255

                for _ in range(length + 2):
                    lit = dict[offset]
                    write_to_dict(lit)
                    write_to_output(lit)
                    offset = (offset + 1) & 0xFF

        nonlocal image_size
        if not skip_zeroes() and not is_eos():
            image_size -= 1
            if image_size == 32768:
                image_size = 0

            if image_size > 0:
                output.pop()
            unpack()

    unpack()
    return bytes(output)


def output_chunk(file_path: Path, chunk: FileChunk, chunk_idx: int):
    chunk_type = chunk.header.chunk_type
    chunk_path = (file_path / f"{chunk_idx}{chunk_type.get_extension()}").absolute()
    if not chunk_path.parent.exists():
        chunk_path.parent.mkdir(parents=True)

    logging.info(
        f"Extracting {chunk_type.get_name()}({chunk_type.get_extension()}) to {chunk_path}..."
    )
    with open(chunk_path, "wb") as f:
        if chunk_type.is_compressed():
            f.write(decode_lzss(chunk.data))
        else:
            f.write(chunk.data)


def output_file(stage_path: Path, entry: FileListEntry, entry_size: int, file: File):
    file_path = (stage_path / f"file{entry.file_id}").absolute()
    logging.info(f"Extracting file {entry.file_id} to {file_path}...")
    for idx, chunk in enumerate(file.chunks):
        output_chunk(file_path, chunk, idx)


def extract_stage_0(header: BinaryIO, data: BinaryIO, output: Path):
    logging.info(f"Extracting stage 0 at {header.name}")
    data.seek(0, 2)
    data_size = data.tell()

    streaming_list: list[StreamingListMovieEntry | StreamingListAudioEntry] = []
    header.seek(0x0)
    for _ in range(0x3):  # 0x0 - 0x78
        streaming_list.append(
            parse_streaming_list_entry(header.read(STREAMING_LIST_ENTRY_SIZE))
        )

    for i, entry in enumerate(streaming_list):
        logging.info(entry)
        # TODO: Extract entry
        pass

    file_list: list[FileListEntry] = []
    header.seek(0x78)
    for _ in range(0x360):  # 0x78 - 0x1B78
        file_list.append(parse_file_list_entry(header.read(FILE_LIST_ENTRY_SIZE)))

    for i, entry in enumerate(file_list):
        file = read_file(data, entry.file_offset)
        if file is None:
            continue

        if entry.file_id == 21000:
            entry_size = 0
            for stream_entry in streaming_list:
                match stream_entry.stream_type:
                    case StreamingListStreamType.Movie:
                        assert isinstance(stream_entry, StreamingListMovieEntry)
                        if stream_entry.movie_number != 0:
                            continue
                        entry_size = stream_entry.offset_folder - entry.file_offset
                        break
                    case StreamingListStreamType.Audio:
                        assert isinstance(stream_entry, StreamingListAudioEntry)
                        entry_size = stream_entry.offset_stage - entry.file_offset
                        break
                    case _:
                        assert False, f"Unknown stream type: {stream_entry.stream_type}"
        elif i < len(file_list) - 1:
            entry_size = file_list[i + 1].file_offset - entry.file_offset
        else:
            entry_size = data_size - entry.file_offset
        output_file(output, entry, entry_size, file)

    end = parse_file_list_entry(header.read(FILE_LIST_ENTRY_SIZE))
    assert end.file_id == 0xFFFFFFFF
    assert end.file_offset == 0xFFFFFFFF * 0x800


def extract_folder(data: BinaryIO, output: Path, root_offset: int, folder_size: int):
    logging.info(f"Extracting folder at {output.stem}")
    data_pos = data.tell()

    file_list: list[FileListEntry] = []
    data.seek(root_offset + 0x0)
    for _ in range(0xA2):  # root_offset + 0x0 - root_offset + 0x510
        entry = parse_file_list_entry(data.read(FILE_LIST_ENTRY_SIZE))
        if entry.file_id == 0 and entry.file_offset == 0:
            continue
        file_list.append(entry)

    assert data.tell() == root_offset + 0x510
    assert int.from_bytes(data.read(4), "little") == 0x0

    streaming_list: list[StreamingListMovieEntry | StreamingListAudioEntry] = []
    assert data.tell() == root_offset + 0x514
    for _ in range(0x12):
        entry_data = data.read(STREAMING_LIST_ENTRY_SIZE)
        if entry_data == b"\x00" * STREAMING_LIST_ENTRY_SIZE:
            continue
        entry = parse_streaming_list_entry(entry_data)
        streaming_list.append(entry)

    assert data.tell() == root_offset + 0x7E4
    assert int.from_bytes(data.read(4), "little") == 0x0

    for i, entry in enumerate(file_list):
        file = read_file(data, root_offset + entry.file_offset)
        if file is None:
            continue

        if i < len(file_list) - 1:
            entry_size: int = file_list[i + 1].file_offset - entry.file_offset
        else:
            entry_size_opt: int | None = None
            for stream_entry in streaming_list:
                match stream_entry.stream_type:
                    case StreamingListStreamType.Movie:
                        assert isinstance(stream_entry, StreamingListMovieEntry)
                        if stream_entry.movie_number != 0:
                            continue
                        entry_size_opt = stream_entry.offset_folder - entry.file_offset
                        break
                    case StreamingListStreamType.Audio:
                        assert isinstance(stream_entry, StreamingListAudioEntry)
                        entry_size_opt = stream_entry.offset_stage - entry.file_offset
                        break
                    case _:
                        assert False, f"Unknown stream type: {stream_entry.stream_type}"

            if entry_size_opt is None:
                entry_size_opt = folder_size - entry.file_offset
            entry_size: int = entry_size_opt

        output_file(output, entry, entry_size, file)

    data.seek(data_pos)


def extract_stage_n(data: BinaryIO, output: Path):
    logging.info(f"Extracting stage at {data.name}")

    data.seek(0x0)
    folder_offset = 0x800
    for _ in range(0x100):
        entry = parse_folder_list_entry(data.read(FOLDER_LIST_ENTRY_SIZE))
        if entry.folder_id == 0 and entry.folder_size == 0:
            continue
        extract_folder(
            data,
            output / f"{entry.folder_id}",
            folder_offset,
            entry.folder_size,
        )
        folder_offset += entry.folder_size


def main():
    logging.basicConfig(level=logging.INFO)
    parser = ArgumentParser()
    parser.add_argument("--executable_disk1", "-exe_d1", type=FileType("rb"))
    parser.add_argument("--executable_disk2", "-exe_d2", type=FileType("rb"))
    parser.add_argument("--stage0_header", "-s0_hdr", type=FileType("rb"))
    parser.add_argument("--stage0_data", "-s0_dat", type=FileType("rb"))
    parser.add_argument("--stage1", "-s1", type=FileType("rb"))
    parser.add_argument("--stage2", "-s2", type=FileType("rb"))
    parser.add_argument("--stage3", "-s3", type=FileType("rb"))
    parser.add_argument("--stage4", "-s4", type=FileType("rb"))
    parser.add_argument("--stage5", "-s5", type=FileType("rb"))
    parser.add_argument("--output", "-o", type=Path, default=".")
    parser.add_argument("--checksum", "-c", action="store_true")
    args = parser.parse_args()

    executable_disk1: BinaryIO = args.executable_disk1
    executable_disk2: BinaryIO = args.executable_disk2
    checksum_disk1 = computeChecksum(executable_disk1)
    checksum_disk2 = computeChecksum(executable_disk2)

    if args.checksum:
        logging.info(f"Checksum of {executable_disk1.name}: {checksum_disk1:08X}")
        logging.info(f"Checksum of {executable_disk2.name}: {checksum_disk2:08X}")
        return
    else:
        pass

    output_path: Path = args.output
    extract_stage_0(args.stage0_header, args.stage0_data, output_path / "stage0")
    extract_stage_n(args.stage1, output_path / "stage1")
    extract_stage_n(args.stage2, output_path / "stage2")
    extract_stage_n(args.stage3, output_path / "stage3")
    extract_stage_n(args.stage4, output_path / "stage4")
    extract_stage_n(args.stage5, output_path / "stage5")

    logging.info(f"Copying main executable {executable_disk1.name}")
    shutil.copy(f"{executable_disk1.name}", f"{output_path / 'main.exe'}")

    logging.info("Copying packages")
    for src, dst in PKG_LIST.items():
        src_path = output_path / src
        dst_path = output_path / "OVR" / dst
        dst_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(src_path, dst_path)

    logging.info("All done!")


if __name__ == "__main__":
    main()
