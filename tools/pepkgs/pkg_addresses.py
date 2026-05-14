import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(
        description="Prints out a summary of all pkg addresses"
    )
    parser.add_argument("dir", type=Path, help="Assets directory")
    args = parser.parse_args()

    map_path = args.dir / "OVR" / "map.json"
    with open(map_path, "r") as f:
        map_data = json.load(f)

    address_map = {}
    for name, entry in map_data.items():
        ovr_name = entry["ovr_name"]
        load_address = entry["load_address"]
        info = {
            "name": name,
            "ovr_name": ovr_name,
        }

        if load_address not in address_map:
            address_map[load_address] = [info]
        else:
            address_map[load_address].append(info)

    for load_address, entries in address_map.items():
        print(load_address)
        for entry in entries:
            print(f"\t{entry['name']} (ovr: {entry['ovr_name']})")
        print()


if __name__ == "__main__":
    main()
