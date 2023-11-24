import argparse
import os
import string
import collections
import hashlib

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
            static_filepath = os.path.relpath(filepath, root_path).replace("\\", "/")
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

FileEntry = collections.namedtuple("FileEntry", ["filepath", "offset", "size", "mime_type_index", "sha1_hash"])
def combine_files(files):
    combined_data = bytearray([])
    file_entries = []
    curr_offset_byte = 0
    for filepath, data in files:
        total_bytes = len(data)
        mime_type_index = get_mime_type_index(filepath)
        sha1_hash = hashlib.sha1(data).hexdigest()
        file_entries.append(FileEntry(filepath, curr_offset_byte, total_bytes, mime_type_index, sha1_hash))
        combined_data.extend(data)
        curr_offset_byte += total_bytes
        # add extra null terminator
        combined_data.extend(b'\0')
        curr_offset_byte += 1
    return combined_data, file_entries

def find_file_entry(file_entries, name):
    for entry in file_entries:
        if entry.filepath == name:
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
        kwargs = index_html_entry._asdict();
        kwargs["filepath"] = ""
        root_entry = FileEntry(**kwargs)
        file_entries.append(root_entry)
    
    print(f"Adding {len(file_entries)} files")
    for entry in file_entries:
        mime_type = MIME_TYPES[entry.mime_type_index]
        print(f"  filepath='/{entry.filepath}',offset={entry.offset},size={entry.size},mime_type='{mime_type[0]}',sha1='{entry.sha1_hash}'")

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
                 "  const char *sha1_hash;\n"
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

        def file_entry_to_string(e):
            return f'{{"/{e.filepath}",{e.offset},{e.size},{e.mime_type_index},"{e.sha1_hash}"}}'

        file_entries_string = ',\n '.join(map(file_entry_to_string, file_entries))
        fp.write(f'static const file_entry_t file_entries[{len(file_entries)}] = {{\n {file_entries_string}\n}};\n')

        fp.write("const file_entry_t* get_array_file_entries(void) { return file_entries; }\n") 
        fp.write("const uint8_t* get_files_data(void) { return files_data; }\n")
        fp.write("const char** get_mime_types(void) { return mime_types; }\n")

if __name__ == "__main__":
    main()
