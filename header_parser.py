import sys
import clang.cindex
import os

kind_names = {
    clang.cindex.CursorKind.STRUCT_DECL: "struct",
    clang.cindex.CursorKind.FIELD_DECL: "field",
    clang.cindex.CursorKind.VAR_DECL: "var",
    clang.cindex.CursorKind.CLASS_DECL: "class",
    clang.cindex.CursorKind.FUNCTION_DECL: "function",
    clang.cindex.CursorKind.CONSTRUCTOR: "constructor",
    clang.cindex.CursorKind.DESTRUCTOR: "destructor",
    clang.cindex.CursorKind.CXX_METHOD: "method",
}

kind_functions = {}

current_macros = []

current_class_name = ""

current_file = None

current_string = ""

all_type_names = []


def traverse_class_fields(node):
    if node.kind in [clang.cindex.CursorKind.FIELD_DECL, clang.cindex.CursorKind.CXX_METHOD]:
        return kind_functions[node.kind](node)
    return ""


def class_decl(node):
    #print("    class", node.spelling)
    global current_class_name
    global current_string
    global all_type_names
    current_class_name = node.spelling

    if len(current_macros) > 0 and current_macros[0]["type"] == "class":
        if current_macros[0]["class_name"] != node.spelling:
            return
        macro_line = current_macros[0]["line"]
        while(macro_line < node.location.line):
            current_macros.pop(0)
            if len(current_macros) == 0:
                return
            macro_line = current_macros[0]["line"]

        current_macros.pop(0)
        fields_data = []
        for c in node.get_children():
            res = traverse_class_fields(c)
            if res:
                fields_data.append(res)
        # print(fields_data)
        current_string += """#define __DEF_CLASS_HELPER_L_{line}() \\
    static AClassData& GetClassDataStatic() \\
    {{ \\
        static AClassData classData; \\
        static bool initialized = false; \\
        if (initialized) return classData; \\
        initialized = true; \\
            \\
        classData.Name = "{class_name}"; \\
        classData.Size = sizeof({class_name}); \\
            \\
        {fields} \\
        return classData; \\
    }} \\
    \\
    virtual const AClassData& GetClassData() const override \\
    {{ \\
        static AClassData classData; \\
        static bool initialized = false; \\
        if (initialized) return classData; \\
        initialized = true; \\
            \\
        classData.Name = "{class_name}"; \\
        classData.Size = sizeof({class_name}); \\
            \\
        {fields} \\
        return classData; \\
    }}\n""".format(line=macro_line, class_name=node.spelling, fields=" \\\n\t\t".join(fields_data))
        
        all_type_names.append(node.spelling)
    pass


def struct_decl(node):
    class_decl(node)
    pass


def field_decl(node):
    #print("        field", node.spelling)
    if len(current_macros) > 0 and current_macros[0]["type"] == "property":
        if node.location.line == current_macros[0]["line"] + 1:
            current_macros.pop(0)

            # return f"""PropertyData propData {{ "{node.spelling}", "{node.type.spelling}", offsetof({current_class_name}, {node.spelling}) }};"""
            return f"""classData.Properties.push_back({{ AName("{node.spelling}"), AName("{node.type.spelling}"), offsetof({current_class_name}, {node.spelling}) }});"""
    return ""


def method_decl(node):
    #print("        method", node.spelling)
    pass


kind_functions = {
    clang.cindex.CursorKind.STRUCT_DECL: struct_decl,
    clang.cindex.CursorKind.FIELD_DECL: field_decl,
    clang.cindex.CursorKind.VAR_DECL: "var",
    clang.cindex.CursorKind.CLASS_DECL: class_decl,
    clang.cindex.CursorKind.FUNCTION_DECL: "function",
    clang.cindex.CursorKind.CONSTRUCTOR: "constructor",
    clang.cindex.CursorKind.DESTRUCTOR: "destructor",
    clang.cindex.CursorKind.CXX_METHOD: method_decl,
}

filename = sys.argv[1]
current_filename = filename
current_gen_filename = ""

if len(sys.argv) > 2:
    working_dir = sys.argv[2]
    if working_dir:
        os.chdir(working_dir)
        # print(working_dir)


def look_for_macros():
    global current_macros
    current_macros = []
    # Using readlines()
    file1 = open(current_filename, 'r')
    Lines = file1.readlines()

    count = 0
    # Strips the newline character
    for line in Lines:
        count += 1
        if line.strip().startswith("DEF_PROPERTY("):
            #print("Line{}: {}".format(count, line.strip()))
            current_macros.append({"line": count, "type": "property"})

        if line.strip().startswith("DEF_CLASS("):
            class_found = ""
            temp_line = count
            while class_found == "" and temp_line > 0:
                current_line = Lines[temp_line].strip()
                if current_line.startswith("class "):
                    class_found = current_line.removeprefix(
                        "class ").split(" ")[0]
                if current_line.startswith("struct "):
                    class_found = current_line.removeprefix(
                        "struct ").split(" ")[0]
                temp_line -= 1
            #print("Line{}: {}".format(count, line.strip()))
            current_macros.append(
                {"line": count, "type": "class", "class_name": class_found})

    file1.close()
    pass


def find_typerefs(node):
    # Recurse for children of this node
    global current_filename
    global current_gen_filename
    global current_file
    global current_string
    node_file = str(node.location.file)
    if current_filename.startswith("./src/") or node_file.startswith("./src/") or current_filename.startswith("./src\\") or node_file.startswith("./src\\"):
        if (node_file.startswith("./src/") or node_file.startswith("./src\\")) and node_file.endswith(".h") and not node_file.endswith(".gen.h"):
            if current_filename != node_file:
                current_filename = node_file
                print("Parsing file:", current_filename)

                dir_name = os.path.dirname(current_filename) + "/generated"
                if not os.path.exists(dir_name):
                    os.makedirs(dir_name)

                if current_file:
                    # compare current_string with the contents of current_file
                    if current_string != current_file.read():
                        current_file.close()
                        current_file = open(current_gen_filename, "w")
                        current_file.write(current_string)
                    else:
                        print("No changes in file:", current_filename)
                    
                    current_file.close()

                current_gen_filename = os.path.dirname(
                    current_filename) + "/generated/" + os.path.basename(current_filename).removesuffix(".h") + ".gen.h"

                # check if file exists
                if not os.path.exists(current_gen_filename):
                    current_file = open(current_gen_filename, "w")
                    current_file.write("")
                    current_file.close()

                current_file = open(current_gen_filename, "r")
                current_string = ""
                look_for_macros()
            if node.kind.is_declaration():
                if node.kind == clang.cindex.CursorKind.CLASS_DECL or node.kind == clang.cindex.CursorKind.STRUCT_DECL:
                    decl_type = "none"
                    if node.kind in kind_names.keys():
                        decl_type = kind_names[node.kind]
                    var_type = node.type.spelling
                    #decl_type = node.kind.name
                    # print('    Found declaration of a %s, named %s, with type %s [line=%s, col=%s]' % (
                    #    decl_type, node.spelling, var_type, node.location.line, node.location.column))
                    kind_functions[node.kind](node)
                else:
                    #print(node.spelling, node.kind.name)
                    pass
        for c in node.get_children():
            find_typerefs(c)


index = clang.cindex.Index.create()
tu = index.parse(
    filename, ["-Wall", "-I./build/_deps/raylib-src/src", "-I./src/"])
#print('Translation unit:', tu.spelling)
find_typerefs(tu.cursor)

# print(os.getcwd())

# iterate over all types and generate a header file
# the header file will contain:
# void __Generated_RegisterTypes()
# {
#   World.RegisterDefault<type>();
#   ...
# }


all_types_file = open("./src/all_types.gen.h", "r")
all_types_string = """#pragma once

#define __GENERATED_REGISTER_TYPES(World)"""
for t in all_type_names:
    all_types_string += f"\\\n    World->RegisterDefault<{t}>();"

all_types_string += "\n"

if all_types_file.read() != all_types_string:
    all_types_file.close()
    all_types_file = open("./src/all_types.gen.h", "w")
    all_types_file.write(all_types_string)
    all_types_file.close()

if current_file:
    # compare current_string with the contents of current_file
    if current_string != current_file.read():
        current_file.close()
        current_file = open(current_gen_filename, "w")
        current_file.write(current_string)
        current_string = ""
    else:
        print("No changes in file:", current_filename)
    current_file.close()
