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
            FileChunkType.RoomPkg: False,
            FileChunkType.Image: False,
            FileChunkType.Clut: False,
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
    "stage0/file0/1.pe2pkg": "stage0_001",
    "stage0/file1/5.pe2pkg": "stage0_002",
    "stage0/file100300/3.pe2pkg": "stage0_003",
    "stage0/file100400/3.pe2pkg": "stage0_004",
    "stage0/file100500/3.pe2pkg": "stage0_005",
    "stage0/file100700/3.pe2pkg": "stage0_006",
    "stage0/file101000/3.pe2pkg": "stage0_007",
    "stage0/file101100/2.pe2pkg": "stage0_008",
    "stage0/file101200/3.pe2pkg": "stage0_009",
    "stage0/file101300/3.pe2pkg": "stage0_010",
    "stage0/file101500/3.pe2pkg": "stage0_011",
    "stage0/file101600/3.pe2pkg": "stage0_012",
    "stage0/file101900/3.pe2pkg": "stage0_013",
    "stage0/file10200/2.pe2pkg": "stage0_014",
    "stage0/file102000/3.pe2pkg": "stage0_015",
    "stage0/file102100/3.pe2pkg": "stage0_016",
    "stage0/file102300/3.pe2pkg": "stage0_017",
    "stage0/file102400/3.pe2pkg": "stage0_018",
    "stage0/file102500/3.pe2pkg": "stage0_019",
    "stage0/file102600/3.pe2pkg": "stage0_020",
    "stage0/file10300/2.pe2pkg": "stage0_021",
    "stage0/file10301/3.pe2pkg": "stage0_022",
    "stage0/file10302/3.pe2pkg": "stage0_023",
    "stage0/file10303/3.pe2pkg": "stage0_024",
    "stage0/file10304/3.pe2pkg": "stage0_025",
    "stage0/file10305/3.pe2pkg": "stage0_026",
    "stage0/file10306/2.pe2pkg": "stage0_027",
    "stage0/file10307/2.pe2pkg": "stage0_028",
    "stage0/file10308/2.pe2pkg": "stage0_029",
    "stage0/file10309/3.pe2pkg": "stage0_030",
    "stage0/file10310/2.pe2pkg": "stage0_031",
    "stage0/file10311/2.pe2pkg": "stage0_032",
    "stage0/file10312/2.pe2pkg": "stage0_033",
    "stage0/file10313/2.pe2pkg": "stage0_034",
    "stage0/file10314/2.pe2pkg": "stage0_035",
    "stage0/file10315/2.pe2pkg": "stage0_036",
    "stage0/file10316/3.pe2pkg": "stage0_037",
    "stage0/file10317/3.pe2pkg": "stage0_038",
    "stage0/file10318/2.pe2pkg": "stage0_039",
    "stage0/file10319/3.pe2pkg": "stage0_040",
    "stage0/file10320/3.pe2pkg": "stage0_041",
    "stage0/file10321/3.pe2pkg": "stage0_042",
    "stage0/file10322/3.pe2pkg": "stage0_043",
    "stage0/file10323/2.pe2pkg": "stage0_044",
    "stage0/file10324/2.pe2pkg": "stage0_045",
    "stage0/file10325/3.pe2pkg": "stage0_046",
    "stage0/file10326/3.pe2pkg": "stage0_047",
    "stage0/file10327/2.pe2pkg": "stage0_048",
    "stage0/file10328/3.pe2pkg": "stage0_049",
    "stage0/file10329/3.pe2pkg": "stage0_050",
    "stage0/file10330/3.pe2pkg": "stage0_051",
    "stage0/file10331/3.pe2pkg": "stage0_052",
    "stage0/file10332/3.pe2pkg": "stage0_053",
    "stage0/file103700/3.pe2pkg": "stage0_054",
    "stage0/file103800/3.pe2pkg": "stage0_055",
    "stage0/file10400/2.pe2pkg": "stage0_056",
    "stage0/file104000/3.pe2pkg": "stage0_057",
    "stage0/file104100/3.pe2pkg": "stage0_058",
    "stage0/file104400/2.pe2pkg": "stage0_059",
    "stage0/file104500/2.pe2pkg": "stage0_060",
    "stage0/file104600/3.pe2pkg": "stage0_061",
    "stage0/file104900/3.pe2pkg": "stage0_062",
    "stage0/file10500/2.pe2pkg": "stage0_063",
    "stage0/file105100/3.pe2pkg": "stage0_064",
    "stage0/file105300/3.pe2pkg": "stage0_065",
    "stage0/file105400/3.pe2pkg": "stage0_066",
    "stage0/file105500/3.pe2pkg": "stage0_067",
    "stage0/file105600/3.pe2pkg": "stage0_068",
    "stage0/file105700/3.pe2pkg": "stage0_069",
    "stage0/file107000/3.pe2pkg": "stage0_070",
    "stage0/file107200/3.pe2pkg": "stage0_071",
    "stage0/file107500/3.pe2pkg": "stage0_072",
    "stage0/file107600/6.pe2pkg": "stage0_073",
    "stage0/file109300/2.pe2pkg": "stage0_074",
    "stage0/file110300/4.pe2pkg": "stage0_075",
    "stage0/file110600/3.pe2pkg": "stage0_076",
    "stage0/file110700/3.pe2pkg": "stage0_077",
    "stage0/file110800/2.pe2pkg": "stage0_078",
    "stage0/file111800/3.pe2pkg": "stage0_079",
    "stage0/file113000/2.pe2pkg": "stage0_080",
    "stage0/file113100/2.pe2pkg": "stage0_081",
    "stage0/file120300/4.pe2pkg": "stage0_082",
    "stage0/file120400/2.pe2pkg": "stage0_083",
    "stage0/file120500/2.pe2pkg": "stage0_084",
    "stage0/file121300/2.pe2pkg": "stage0_085",
    "stage0/file123000/3.pe2pkg": "stage0_086",
    "stage0/file123200/2.pe2pkg": "stage0_087",
    "stage0/file123400/3.pe2pkg": "stage0_088",
    "stage0/file135000/2.pe2pkg": "stage0_089",
    "stage0/file135400/2.pe2pkg": "stage0_090",
    "stage0/file135600/2.pe2pkg": "stage0_091",
    "stage0/file136100/4.pe2pkg": "stage0_092",
    "stage0/file136300/1.pe2pkg": "stage0_093",
    "stage0/file140400/2.pe2pkg": "stage0_094",
    "stage0/file141000/4.pe2pkg": "stage0_095",
    "stage0/file142600/0.pe2pkg": "stage0_096",
    "stage0/file142900/0.pe2pkg": "stage0_097",
    "stage0/file143000/1.pe2pkg": "stage0_098",
    "stage0/file143400/0.pe2pkg": "stage0_099",
    "stage0/file143900/2.pe2pkg": "stage0_100",
    "stage0/file146000/0.pe2pkg": "stage0_101",
    "stage0/file146300/2.pe2pkg": "stage0_102",
    "stage0/file150400/3.pe2pkg": "stage0_103",
    "stage0/file151000/2.pe2pkg": "stage0_104",
    "stage0/file160600/2.pe2pkg": "stage0_105",
    "stage0/file160700/2.pe2pkg": "stage0_106",
    "stage0/file160900/0.pe2pkg": "stage0_107",
    "stage0/file161500/4.pe2pkg": "stage0_108",
    "stage0/file200300/3.pe2pkg": "stage0_109",
    "stage0/file200400/3.pe2pkg": "stage0_110",
    "stage0/file200700/3.pe2pkg": "stage0_111",
    "stage0/file20100/0.pe2pkg": "stage0_112",
    "stage0/file201000/3.pe2pkg": "stage0_113",
    "stage0/file201100/2.pe2pkg": "stage0_114",
    "stage0/file201200/3.pe2pkg": "stage0_115",
    "stage0/file201300/3.pe2pkg": "stage0_116",
    "stage0/file201500/3.pe2pkg": "stage0_117",
    "stage0/file201600/3.pe2pkg": "stage0_118",
    "stage0/file201800/3.pe2pkg": "stage0_119",
    "stage0/file201900/3.pe2pkg": "stage0_120",
    "stage0/file20200/0.pe2pkg": "stage0_121",
    "stage0/file202000/3.pe2pkg": "stage0_122",
    "stage0/file202100/3.pe2pkg": "stage0_123",
    "stage0/file202300/3.pe2pkg": "stage0_124",
    "stage0/file202400/3.pe2pkg": "stage0_125",
    "stage0/file202500/3.pe2pkg": "stage0_126",
    "stage0/file202600/3.pe2pkg": "stage0_127",
    "stage0/file202900/2.pe2pkg": "stage0_128",
    "stage0/file203700/3.pe2pkg": "stage0_129",
    "stage0/file203800/3.pe2pkg": "stage0_130",
    "stage0/file204000/3.pe2pkg": "stage0_131",
    "stage0/file204100/3.pe2pkg": "stage0_132",
    "stage0/file204400/2.pe2pkg": "stage0_133",
    "stage0/file204600/3.pe2pkg": "stage0_134",
    "stage0/file204900/3.pe2pkg": "stage0_135",
    "stage0/file205200/1.pe2pkg": "stage0_136",
    "stage0/file205500/3.pe2pkg": "stage0_137",
    "stage0/file205600/3.pe2pkg": "stage0_138",
    "stage0/file205700/3.pe2pkg": "stage0_139",
    "stage0/file20600/0.pe2pkg": "stage0_140",
    "stage0/file206100/3.pe2pkg": "stage0_141",
    "stage0/file207000/3.pe2pkg": "stage0_142",
    "stage0/file207200/3.pe2pkg": "stage0_143",
    "stage0/file20900/4.pe2pkg": "stage0_144",
    "stage0/file21000/6.pe2pkg": "stage0_145",
    "stage0/file210600/2.pe2pkg": "stage0_146",
    "stage0/file210700/2.pe2pkg": "stage0_147",
    "stage0/file213000/2.pe2pkg": "stage0_148",
    "stage0/file213100/2.pe2pkg": "stage0_149",
    "stage0/file215100/2.pe2pkg": "stage0_150",
    "stage0/file223600/3.pe2pkg": "stage0_151",
    "stage0/file260400/3.pe2pkg": "stage0_152",
    "stage0/file260500/3.pe2pkg": "stage0_153",
    "stage0/file300300/3.pe2pkg": "stage0_154",
    "stage0/file300400/3.pe2pkg": "stage0_155",
    "stage0/file300700/3.pe2pkg": "stage0_156",
    "stage0/file30100/2.pe2pkg": "stage0_157",
    "stage0/file301000/3.pe2pkg": "stage0_158",
    "stage0/file30102/2.pe2pkg": "stage0_159",
    "stage0/file301100/2.pe2pkg": "stage0_160",
    "stage0/file301200/3.pe2pkg": "stage0_161",
    "stage0/file301300/3.pe2pkg": "stage0_162",
    "stage0/file301500/3.pe2pkg": "stage0_163",
    "stage0/file301600/3.pe2pkg": "stage0_164",
    "stage0/file301800/3.pe2pkg": "stage0_165",
    "stage0/file301900/3.pe2pkg": "stage0_166",
    "stage0/file30200/2.pe2pkg": "stage0_167",
    "stage0/file302000/3.pe2pkg": "stage0_168",
    "stage0/file30202/2.pe2pkg": "stage0_169",
    "stage0/file30203/2.pe2pkg": "stage0_170",
    "stage0/file302100/3.pe2pkg": "stage0_171",
    "stage0/file302300/3.pe2pkg": "stage0_172",
    "stage0/file302400/3.pe2pkg": "stage0_173",
    "stage0/file302500/3.pe2pkg": "stage0_174",
    "stage0/file302600/3.pe2pkg": "stage0_175",
    "stage0/file30300/2.pe2pkg": "stage0_176",
    "stage0/file30302/2.pe2pkg": "stage0_177",
    "stage0/file30303/2.pe2pkg": "stage0_178",
    "stage0/file303600/0.pe2pkg": "stage0_179",
    "stage0/file303700/3.pe2pkg": "stage0_180",
    "stage0/file303800/3.pe2pkg": "stage0_181",
    "stage0/file30400/2.pe2pkg": "stage0_182",
    "stage0/file304000/3.pe2pkg": "stage0_183",
    "stage0/file30402/2.pe2pkg": "stage0_184",
    "stage0/file30403/2.pe2pkg": "stage0_185",
    "stage0/file30404/2.pe2pkg": "stage0_186",
    "stage0/file30405/2.pe2pkg": "stage0_187",
    "stage0/file30406/2.pe2pkg": "stage0_188",
    "stage0/file304100/3.pe2pkg": "stage0_189",
    "stage0/file304400/2.pe2pkg": "stage0_190",
    "stage0/file304600/3.pe2pkg": "stage0_191",
    "stage0/file304900/3.pe2pkg": "stage0_192",
    "stage0/file30500/2.pe2pkg": "stage0_193",
    "stage0/file30502/2.pe2pkg": "stage0_194",
    "stage0/file30503/2.pe2pkg": "stage0_195",
    "stage0/file305500/3.pe2pkg": "stage0_196",
    "stage0/file305600/3.pe2pkg": "stage0_197",
    "stage0/file305700/3.pe2pkg": "stage0_198",
    "stage0/file307000/3.pe2pkg": "stage0_199",
    "stage0/file307200/3.pe2pkg": "stage0_200",
    "stage0/file310100/0.pe2pkg": "stage0_201",
    "stage0/file310600/4.pe2pkg": "stage0_202",
    "stage0/file311500/3.pe2pkg": "stage0_203",
    "stage0/file311900/2.pe2pkg": "stage0_204",
    "stage0/file312200/2.pe2pkg": "stage0_205",
    "stage0/file317000/2.pe2pkg": "stage0_206",
    "stage0/file323000/2.pe2pkg": "stage0_207",
    "stage0/file323300/2.pe2pkg": "stage0_208",
    "stage0/file323400/2.pe2pkg": "stage0_209",
    "stage0/file335800/4.pe2pkg": "stage0_210",
    "stage0/file341300/0.pe2pkg": "stage0_211",
    "stage0/file341700/0.pe2pkg": "stage0_212",
    "stage0/file341900/0.pe2pkg": "stage0_213",
    "stage0/file342000/0.pe2pkg": "stage0_214",
    "stage0/file342100/0.pe2pkg": "stage0_215",
    "stage0/file342200/0.pe2pkg": "stage0_216",
    "stage0/file342400/2.pe2pkg": "stage0_217",
    "stage0/file350500/2.pe2pkg": "stage0_218",
    "stage0/file350700/4.pe2pkg": "stage0_219",
    "stage0/file356100/2.pe2pkg": "stage0_220",
    "stage0/file361100/2.pe2pkg": "stage0_221",
    "stage0/file400100/3.pe2pkg": "stage0_222",
    "stage0/file400500/3.pe2pkg": "stage0_223",
    "stage0/file400600/2.pe2pkg": "stage0_224",
    "stage0/file401000/3.pe2pkg": "stage0_225",
    "stage0/file401300/3.pe2pkg": "stage0_226",
    "stage0/file401800/3.pe2pkg": "stage0_227",
    "stage0/file402200/2.pe2pkg": "stage0_228",
    "stage0/file403000/5.pe2pkg": "stage0_229",
    "stage0/file403100/5.pe2pkg": "stage0_230",
    "stage0/file403200/5.pe2pkg": "stage0_231",
    "stage0/file403600/0.pe2pkg": "stage0_232",
    "stage0/file403900/2.pe2pkg": "stage0_233",
    "stage0/file405800/3.pe2pkg": "stage0_234",
    "stage0/file407500/3.pe2pkg": "stage0_235",
    "stage0/file420700/4.pe2pkg": "stage0_236",
    "stage0/file421600/3.pe2pkg": "stage0_237",
    "stage0/file443500/2.pe2pkg": "stage0_238",
    "stage0/file444000/5.pe2pkg": "stage0_239",
    "stage0/file450200/2.pe2pkg": "stage0_240",
    "stage0/file450800/2.pe2pkg": "stage0_241",
    "stage0/file450900/0.pe2pkg": "stage0_242",
    "stage0/file451100/2.pe2pkg": "stage0_243",
    "stage0/file460200/2.pe2pkg": "stage0_244",
    "stage0/file461800/2.pe2pkg": "stage0_245",
    "stage0/file50100/1.pe2pkg": "stage0_246",
    "stage0/file50104/1.pe2pkg": "stage0_247",
    "stage0/file50107/1.pe2pkg": "stage0_248",
    "stage0/file50110/1.pe2pkg": "stage0_249",
    "stage0/file50113/1.pe2pkg": "stage0_250",
    "stage0/file50116/1.pe2pkg": "stage0_251",
    "stage0/file50119/1.pe2pkg": "stage0_252",
    "stage0/file50122/1.pe2pkg": "stage0_253",
    "stage0/file50125/1.pe2pkg": "stage0_254",
    "stage0/file50128/1.pe2pkg": "stage0_255",
    "stage0/file50131/1.pe2pkg": "stage0_256",
    "stage0/file50134/1.pe2pkg": "stage0_257",
    "stage0/file50146/1.pe2pkg": "stage0_258",
    "stage0/file50149/1.pe2pkg": "stage0_259",
    "stage0/file50152/1.pe2pkg": "stage0_260",
    "stage0/file503500/7.pe2pkg": "stage0_261",
    "stage0/file510900/4.pe2pkg": "stage0_262",
    "stage0/file511000/4.pe2pkg": "stage0_263",
    "stage0/file521100/4.pe2pkg": "stage0_264",
    "stage0/file535700/2.pe2pkg": "stage0_265",
    "stage0/file548100/0.pe2pkg": "stage0_266",
    "stage0/file560800/4.pe2pkg": "stage0_267",
    "stage0/file640400/2.pe2pkg": "stage0_268",
    "stage0/file800100/0.pe2pkg": "stage0_269",
    "stage0/file800101/5.pe2pkg": "stage0_270",
    "stage0/file800102/5.pe2pkg": "stage0_271",
    "stage0/file800103/5.pe2pkg": "stage0_272",
    "stage0/file800104/5.pe2pkg": "stage0_273",
    "stage0/file800200/3.pe2pkg": "stage0_274",
    "stage0/file800300/4.pe2pkg": "stage0_275",
    "stage0/file900000/6.pe2pkg": "stage0_276",
    "stage0/file900002/6.pe2pkg": "stage0_277",
    "stage0/file900003/7.pe2pkg": "stage0_278",
    "stage0/file900004/2.pe2pkg": "stage0_279",
    "stage0/file900005/2.pe2pkg": "stage0_280",
    "stage1/1001/file0/4.pe2pkg": "stage1_001",
    "stage1/101/file0/3.pe2pkg": "stage1_002",
    "stage1/1101/file0/6.pe2pkg": "stage1_003",
    "stage1/1201/file0/7.pe2pkg": "stage1_004",
    "stage1/1301/file0/7.pe2pkg": "stage1_005",
    "stage1/1401/file0/6.pe2pkg": "stage1_006",
    "stage1/1501/file0/3.pe2pkg": "stage1_007",
    "stage1/1601/file0/6.pe2pkg": "stage1_008",
    "stage1/1701/file0/7.pe2pkg": "stage1_009",
    "stage1/1801/file0/6.pe2pkg": "stage1_010",
    "stage1/1901/file0/7.pe2pkg": "stage1_011",
    "stage1/2001/file0/5.pe2pkg": "stage1_012",
    "stage1/201/file0/3.pe2pkg": "stage1_013",
    "stage1/2101/file0/1.pe2pkg": "stage1_014",
    "stage1/301/file0/5.pe2pkg": "stage1_015",
    "stage1/401/file0/8.pe2pkg": "stage1_016",
    "stage1/501/file0/5.pe2pkg": "stage1_017",
    "stage1/601/file0/5.pe2pkg": "stage1_018",
    "stage1/701/file0/5.pe2pkg": "stage1_019",
    "stage1/801/file0/5.pe2pkg": "stage1_020",
    "stage1/901/file0/5.pe2pkg": "stage1_021",
    "stage2/101/file0/3.pe2pkg": "stage2_001",
    "stage2/1101/file0/3.pe2pkg": "stage2_002",
    "stage2/1201/file0/3.pe2pkg": "stage2_003",
    "stage2/1301/file0/3.pe2pkg": "stage2_004",
    "stage2/1401/file0/3.pe2pkg": "stage2_005",
    "stage2/1501/file0/3.pe2pkg": "stage2_006",
    "stage2/1601/file0/5.pe2pkg": "stage2_007",
    "stage2/1701/file0/3.pe2pkg": "stage2_008",
    "stage2/1801/file0/5.pe2pkg": "stage2_009",
    "stage2/1901/file0/3.pe2pkg": "stage2_010",
    "stage2/2001/file0/5.pe2pkg": "stage2_011",
    "stage2/201/file0/5.pe2pkg": "stage2_012",
    "stage2/2101/file0/6.pe2pkg": "stage2_013",
    "stage2/2201/file0/5.pe2pkg": "stage2_014",
    "stage2/2301/file0/5.pe2pkg": "stage2_015",
    "stage2/2401/file0/3.pe2pkg": "stage2_016",
    "stage2/2501/file0/3.pe2pkg": "stage2_017",
    "stage2/2601/file0/5.pe2pkg": "stage2_018",
    "stage2/2701/file0/5.pe2pkg": "stage2_019",
    "stage2/2801/file0/3.pe2pkg": "stage2_020",
    "stage2/2901/file0/3.pe2pkg": "stage2_021",
    "stage2/3001/file0/3.pe2pkg": "stage2_022",
    "stage2/301/file0/3.pe2pkg": "stage2_023",
    "stage2/3101/file0/3.pe2pkg": "stage2_024",
    "stage2/3201/file0/5.pe2pkg": "stage2_025",
    "stage2/3401/file0/5.pe2pkg": "stage2_026",
    "stage2/3801/file0/5.pe2pkg": "stage2_027",
    "stage2/401/file0/1.pe2pkg": "stage2_028",
    "stage2/501/file0/3.pe2pkg": "stage2_029",
    "stage2/601/file0/3.pe2pkg": "stage2_030",
    "stage2/701/file0/3.pe2pkg": "stage2_031",
    "stage2/801/file0/5.pe2pkg": "stage2_032",
    "stage2/901/file0/3.pe2pkg": "stage2_033",
    "stage3/101/file0/5.pe2pkg": "stage3_001",
    "stage3/1101/file0/5.pe2pkg": "stage3_002",
    "stage3/1201/file0/5.pe2pkg": "stage3_003",
    "stage3/1301/file0/5.pe2pkg": "stage3_004",
    "stage3/1401/file0/5.pe2pkg": "stage3_005",
    "stage3/1501/file0/5.pe2pkg": "stage3_006",
    "stage3/1601/file0/5.pe2pkg": "stage3_007",
    "stage3/1701/file0/5.pe2pkg": "stage3_008",
    "stage3/1801/file0/6.pe2pkg": "stage3_009",
    "stage3/1901/file0/3.pe2pkg": "stage3_010",
    "stage3/2001/file0/5.pe2pkg": "stage3_011",
    "stage3/201/file0/5.pe2pkg": "stage3_012",
    "stage3/2101/file0/5.pe2pkg": "stage3_013",
    "stage3/2201/file0/5.pe2pkg": "stage3_014",
    "stage3/2301/file0/5.pe2pkg": "stage3_015",
    "stage3/2401/file0/5.pe2pkg": "stage3_016",
    "stage3/2501/file0/3.pe2pkg": "stage3_017",
    "stage3/2601/file0/5.pe2pkg": "stage3_018",
    "stage3/2701/file0/5.pe2pkg": "stage3_019",
    "stage3/2801/file0/5.pe2pkg": "stage3_020",
    "stage3/2901/file0/6.pe2pkg": "stage3_021",
    "stage3/3001/file0/5.pe2pkg": "stage3_022",
    "stage3/301/file0/3.pe2pkg": "stage3_023",
    "stage3/3101/file0/6.pe2pkg": "stage3_024",
    "stage3/3201/file0/5.pe2pkg": "stage3_025",
    "stage3/3401/file0/5.pe2pkg": "stage3_026",
    "stage3/3801/file0/5.pe2pkg": "stage3_027",
    "stage3/501/file0/5.pe2pkg": "stage3_028",
    "stage3/601/file0/3.pe2pkg": "stage3_029",
    "stage3/701/file0/3.pe2pkg": "stage3_030",
    "stage3/801/file0/3.pe2pkg": "stage3_031",
    "stage3/901/file0/3.pe2pkg": "stage3_032",
    "stage4/1001/file0/3.pe2pkg": "stage4_001",
    "stage4/101/file0/5.pe2pkg": "stage4_002",
    "stage4/1101/file0/3.pe2pkg": "stage4_003",
    "stage4/1201/file0/3.pe2pkg": "stage4_004",
    "stage4/1301/file0/3.pe2pkg": "stage4_005",
    "stage4/1401/file0/6.pe2pkg": "stage4_006",
    "stage4/1501/file0/5.pe2pkg": "stage4_007",
    "stage4/1601/file0/8.pe2pkg": "stage4_008",
    "stage4/1701/file0/3.pe2pkg": "stage4_009",
    "stage4/1801/file0/5.pe2pkg": "stage4_010",
    "stage4/1901/file0/3.pe2pkg": "stage4_011",
    "stage4/2001/file0/6.pe2pkg": "stage4_012",
    "stage4/201/file0/7.pe2pkg": "stage4_013",
    "stage4/2101/file0/5.pe2pkg": "stage4_014",
    "stage4/2201/file0/9.pe2pkg": "stage4_015",
    "stage4/2301/file0/5.pe2pkg": "stage4_016",
    "stage4/2401/file0/3.pe2pkg": "stage4_017",
    "stage4/2501/file0/3.pe2pkg": "stage4_018",
    "stage4/2601/file0/5.pe2pkg": "stage4_019",
    "stage4/2701/file0/3.pe2pkg": "stage4_020",
    "stage4/2801/file0/3.pe2pkg": "stage4_021",
    "stage4/2901/file0/3.pe2pkg": "stage4_022",
    "stage4/3001/file0/3.pe2pkg": "stage4_023",
    "stage4/301/file0/5.pe2pkg": "stage4_024",
    "stage4/3101/file0/6.pe2pkg": "stage4_025",
    "stage4/3201/file0/6.pe2pkg": "stage4_026",
    "stage4/3301/file0/5.pe2pkg": "stage4_027",
    "stage4/3401/file0/5.pe2pkg": "stage4_028",
    "stage4/3501/file0/5.pe2pkg": "stage4_029",
    "stage4/3601/file0/8.pe2pkg": "stage4_030",
    "stage4/3701/file0/1.pe2pkg": "stage4_031",
    "stage4/3801/file0/1.pe2pkg": "stage4_032",
    "stage4/3901/file0/8.pe2pkg": "stage4_033",
    "stage4/4001/file0/7.pe2pkg": "stage4_034",
    "stage4/401/file0/5.pe2pkg": "stage4_035",
    "stage4/4101/file0/3.pe2pkg": "stage4_036",
    "stage4/4201/file0/3.pe2pkg": "stage4_037",
    "stage4/4301/file0/5.pe2pkg": "stage4_038",
    "stage4/4401/file0/5.pe2pkg": "stage4_039",
    "stage4/4501/file0/6.pe2pkg": "stage4_040",
    "stage4/4601/file0/5.pe2pkg": "stage4_041",
    "stage4/4701/file0/5.pe2pkg": "stage4_042",
    "stage4/4801/file0/5.pe2pkg": "stage4_043",
    "stage4/4901/file0/2.pe2pkg": "stage4_044",
    "stage4/501/file0/5.pe2pkg": "stage4_045",
    "stage4/601/file0/6.pe2pkg": "stage4_046",
    "stage4/701/file0/7.pe2pkg": "stage4_047",
    "stage4/801/file0/5.pe2pkg": "stage4_048",
    "stage4/901/file0/5.pe2pkg": "stage4_049",
    "stage5/1001/file0/3.pe2pkg": "stage5_001",
    "stage5/101/file0/3.pe2pkg": "stage5_002",
    "stage5/1101/file0/5.pe2pkg": "stage5_003",
    "stage5/1201/file0/5.pe2pkg": "stage5_004",
    "stage5/1301/file0/5.pe2pkg": "stage5_005",
    "stage5/1401/file0/5.pe2pkg": "stage5_006",
    "stage5/1501/file0/5.pe2pkg": "stage5_007",
    "stage5/1601/file0/3.pe2pkg": "stage5_008",
    "stage5/1701/file0/3.pe2pkg": "stage5_009",
    "stage5/1801/file0/3.pe2pkg": "stage5_010",
    "stage5/1901/file0/3.pe2pkg": "stage5_011",
    "stage5/2001/file0/3.pe2pkg": "stage5_012",
    "stage5/201/file0/5.pe2pkg": "stage5_013",
    "stage5/2101/file0/5.pe2pkg": "stage5_014",
    "stage5/2201/file0/6.pe2pkg": "stage5_015",
    "stage5/2301/file0/5.pe2pkg": "stage5_016",
    "stage5/2401/file0/3.pe2pkg": "stage5_017",
    "stage5/2501/file0/5.pe2pkg": "stage5_018",
    "stage5/2601/file0/4.pe2pkg": "stage5_019",
    "stage5/2701/file0/5.pe2pkg": "stage5_020",
    "stage5/2801/file0/4.pe2pkg": "stage5_021",
    "stage5/2901/file0/5.pe2pkg": "stage5_022",
    "stage5/3001/file0/5.pe2pkg": "stage5_023",
    "stage5/301/file0/3.pe2pkg": "stage5_024",
    "stage5/3101/file0/1.pe2pkg": "stage5_025",
    "stage5/3201/file0/3.pe2pkg": "stage5_026",
    "stage5/3301/file0/3.pe2pkg": "stage5_027",
    "stage5/401/file0/5.pe2pkg": "stage5_028",
    "stage5/501/file0/3.pe2pkg": "stage5_029",
    "stage5/601/file0/3.pe2pkg": "stage5_030",
    "stage5/701/file0/6.pe2pkg": "stage5_031",
    "stage5/801/file0/3.pe2pkg": "stage5_032",
    "stage5/901/file0/3.pe2pkg": "stage5_033",
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
        with open(src_path, "rb") as f:
            pkg_data = f.read()
        pkg_data = decode_lzss(pkg_data)

        with open(dst_path, "wb") as f:
            f.write(pkg_data)

    logging.info("All done!")


if __name__ == "__main__":
    main()
