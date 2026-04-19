import os
import sys

# Extend the .bss segment

USA_SIZE = 0x00059800

if __name__ == "__main__":
    match sys.argv[1]:
        case "build/USA/out/SLUS_010.42":
            usa_path = "build/USA/out/SLUS_010.42"
            if os.path.isfile(usa_path):
                with open(usa_path, "r+b") as file:
                    file.seek(USA_SIZE - 1)
                    file.write(b"\0")
