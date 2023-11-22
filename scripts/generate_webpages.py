import argparse
import os
import string

def get_paths(root_path):
    for filename in os.listdir(root_path):
        filepath = os.path.join(root_path, filename)
        if os.path.isdir(filepath):
            for path in get_paths(filepath):
                yield path
        elif os.path.isfile(filepath):
            yield filepath
        else:
            print(f"Ignoring '{filepath}'")

def fetch_files(root_path):
    files = []
    for filepath in get_paths(root_path):
        with open(filepath, "rb") as fp:
            static_filepath = os.path.relpath(filepath, root_path)
            files.append((static_filepath, fp.read()))
    files = sorted(files, key=lambda x: x[0])
    return files

MIME_TYPES = [
    ("bin", "application/octet-stream"), # 0 index is default for unknown types
    ("js", "application/javascript"),
    ("html", "text/html"),
    ("css", "text/css"),
    ("ico", "image/x-icon"),
]
MIME_TYPE_INDEX_MAPPING = {k:i for i,(k,_) in enumerate(MIME_TYPES)}

def get_mime_type_index(filepath):
    _, ext = os.path.splitext(filepath)
    # no extension
    if len(ext) <= 1:
        return 0
    ext = ext[1:]
    return MIME_TYPE_INDEX_MAPPING.get(ext, 0)

def combine_files(files):
    combined_data = bytearray([])
    file_entries = []
    curr_offset_byte = 0
    for filepath, data in files:
        total_bytes = len(data)
        mime_type_index = get_mime_type_index(filepath)
        file_entries.append((filepath, curr_offset_byte, total_bytes, mime_type_index))
        combined_data.extend(data)
        curr_offset_byte += total_bytes
        # add extra null terminator
        combined_data.extend(b'\0')
        curr_offset_byte += 1
    return combined_data, file_entries

def find_file_entry(file_entries, name):
    for entry in file_entries:
        filename = entry[0]
        if filename == name:
            return entry
    return None

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--static", default="./static", type=str, help="Directory of website files")
    parser.add_argument("--output", default="./components/webserver/", type=str, help="Directory to output *.c and *.h files")
    args = parser.parse_args()

    files = fetch_files(args.static)
    combined_data, file_entries = combine_files(files)
    
    # redirect "" to "index.html"
    index_html_entry = find_file_entry(file_entries, "index.html")
    if not index_html_entry is None:
        _, offset, length, mime_type_index = index_html_entry
        file_entries.append(("", offset, length, mime_type_index))

    with open(os.path.join(args.output, "src/files_data.h"), "w+") as fp:
        fp.write("// THIS IS AUTOMATICALLY GENERATED. DO NOT EDIT\n\n")
        fp.write("#pragma once\n\n")
        fp.write("#include <stddef.h>\n")
        fp.write("#include <stdint.h>\n")
        fp.write(f"#define TOTAL_FILE_ENTRIES {len(file_entries)}\n") 
        fp.write(f"#define TOTAL_BYTES_IN_COMBINED_DATA {len(combined_data)}\n")
        fp.write(f"#define TOTAL_MIME_TYPES {len(MIME_TYPES)}\n")
        fp.write("typedef struct file_entry {\n"
                 "  const char *name;\n"
                 "  size_t offset;\n"
                 "  size_t length;\n"
                 "  size_t mime_type_index;\n"
                 "} file_entry_t;\n\n")
        fp.write("const file_entry_t* get_array_file_entries(void);\n") 
        fp.write("const uint8_t* get_files_data(void);\n")
        fp.write("const char** get_mime_types(void);\n")

    with open(os.path.join(args.output, "src/files_data.c"), "w+") as fp:
        fp.write("// THIS IS AUTOMATICALLY GENERATED. DO NOT EDIT\n\n")
        fp.write('#include "files_data.h"\n\n')

        files_data_string = ','.join(map(lambda x: str(int(x)), combined_data))
        fp.write(f'static const uint8_t files_data[{len(combined_data)}] = {{{files_data_string}}};\n')

        mime_types_string = ','.join(map(lambda x: f'"{x[1]}"', MIME_TYPES))
        fp.write(f'static const char* mime_types[{len(MIME_TYPES)}] = {{{mime_types_string}}};\n')

        def file_entry_to_string(entry):
            filepath, offset, length, mime_type_index = entry
            return f'{{"/{filepath}",{offset},{length},{mime_type_index}}}'

        file_entries_string = ',\n '.join(map(file_entry_to_string, file_entries))
        fp.write(f'static const file_entry_t file_entries[{len(file_entries)}] = {{\n {file_entries_string}\n}};\n')

        fp.write("const file_entry_t* get_array_file_entries(void) { return file_entries; }\n") 
        fp.write("const uint8_t* get_files_data(void) { return files_data; }\n")
        fp.write("const char** get_mime_types(void) { return mime_types; }\n")

if __name__ == "__main__":
    main()
