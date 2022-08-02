import sys

def define_visitor(file, base_name : str, types : [str]):
    file.write("\tclass {base}Visitor\n\t{{\n\tpublic:\n".format(base=base_name))

    for type in types:
        type_name = type.split(":")[0].strip()
        file.write("\t\tvirtual void Visit{derived}{base}({derived}& {param}) = 0;\n".format(derived=type_name, base=base_name, param=base_name.lower()))

    file.write("\t};\n\n")

def define_type(file, base_name : str, class_name : str, fields : str):
    #Class header
    file.write("\n\tclass {derived} : public {base}\n\t{{\n\tpublic:".format(derived=class_name, base=base_name))

    #Write constructor
    constructor_init = ""
    if fields:
        file.write("\n\t\t{derived}({field_list}) :\n".format(derived=class_name, field_list=fields))

        for field in fields.split(", "):
            field_name = field.split(" ")[1]
            constructor_init += "{member}({field}), ".format(member=field_name.capitalize(), field=field_name)
        constructor_init = constructor_init[:len(constructor_init) - 2]
        file.write("\t\t\t{initialiser_list}\n\t\t{{ }}\n".format(initialiser_list=constructor_init))

    #Write accept implementation
    file.write("\n\t\tvoid Accept ({base}Visitor& visitor) override\n\t\t{{\n\t\t\treturn visitor.Visit{derived}{base}(*this);\n\t\t}}\n\n".format(derived=class_name, base=base_name))

    #Write GetType implementation
    enum_name = base_name + "Type"
    file.write("\t\t{enum} GetType() const {{ return {enum}::{type}; }}\n".format(enum=enum_name, type=class_name))

    #Define members
    if fields:
        file.write("\tpublic:\n")
        for field in fields.split(", "):
            type = field.split(" ")[0]
            name = field.split(" ")[1]
            file.write("\t\t{field_type} {field_name};\n".format(field_type=type, field_name=name.capitalize()))

    file.write("\t};\n")

def define_AST(output_dir : str, base_name : str, types : [str], additional_includes : [str], additional_using : [str]):
    path = output_dir + "/" + base_name + ".h"

    with open(path, "w") as file:
        generated_warning = "/*\n* This file is generated, please do not edit\n*/\n"
        includes = "#include <memory>\n#include \"Token.h\"\n" + additional_includes
        #write file header
        file.write("{comment}\n#pragma once\n{include_list}\n\nnamespace Lox\n{{\n\ttemplate <typename T> using shared_ptr = std::shared_ptr<T>;{using}\n\n".format(comment=generated_warning, include_list=includes, using=additional_using))

        #forward declares
        for type in types:
            class_name = type.split(":")[0].strip()
            file.write("\tclass {derived};\n".format(derived=class_name))

        file.write("\n")
        define_visitor(file, base_name, types)

        #write RTTI enum
        file.write("\tenum class {base}Type\n\t{{\n".format(base=base_name))
        for type in types:
            class_name = type.split(":")[0].strip()
            file.write("\t\t{enum_val},\n".format(enum_val=class_name.capitalize()))
        file.write("\t};\n\n")

        #write base class
        enum_name = base_name + "Type"
        functions = "\tvirtual void Accept({base}Visitor& visitor) = 0;\n\t\tvirtual {enum} GetType() const = 0;".format(base=base_name, enum=enum_name)
        file.write("\tclass {base_class_name} \n\t{{\n\tpublic:\n\t{funcs}\n\t}};\n".format(base_class_name = base_name, funcs=functions))

        #write derived types
        for type in types:
            class_name = type.split(":")[0].strip()
            fields = type.split(":")[1].strip()
            define_type(file, base_name, class_name, fields)

        file.write("}") #end namespace scope

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: generate_ast <output directory>")
        sys.exit(64)

     #Unfortunately, some c++ implementation detail is sneaking in
     #These have to be defined in terms of the C++ implementation to allow the code generation to compile without extra nasty hacks

    output_dir = sys.argv[1]
    define_AST(output_dir, "Expr", [
        "Assign     : Token name, shared_ptr<Expr> value",
        "Binary     : shared_ptr<Expr> left, Token op, shared_ptr<Expr> right",
        "Call       : shared_ptr<Expr> callee, Token paren, vector<shared_ptr<Expr>> arguments",
        "Get        : shared_ptr<Expr> object, Token name",
        "Grouping   : shared_ptr<Expr> expression",
        "Literal    : shared_ptr<Object> value",
        "Logical    : shared_ptr<Expr> left, Token op, shared_ptr<Expr> right",
        "Set        : shared_ptr<Expr> object, Token name, shared_ptr<Expr> value",
        "Super      : Token keyword, Token method",
        "This       : Token keyword",
        "Unary      : Token op, shared_ptr<Expr> right",
        "Variable   : Token name",
    ], "#include \"Object.h\"\n#include <vector>", "\n\ttemplate <typename T> using vector = std::vector<T>;")

    define_AST(output_dir, "Stmt", [
        "Block      : vector<shared_ptr<Stmt>> statements",
        "Expression : shared_ptr<Expr> subjectExpression",
        "Function   : Token name, vector<Token> params, vector<shared_ptr<Stmt>> body",
        "Class      : Token name, shared_ptr<Variable> superclass, vector<shared_ptr<Function>> methods",
        "If         : shared_ptr<Expr> condition, shared_ptr<Stmt> thenBranch, shared_ptr<Stmt> elseBranch",
        "Print      : shared_ptr<Expr> subjectExpression",
        "Return     : Token keyword, shared_ptr<Expr> value",
        "Var        : Token name, shared_ptr<Expr> initialiser",
        "While      : shared_ptr<Expr> condition, shared_ptr<Stmt> body",
        "For        : shared_ptr<Stmt> initialise, shared_ptr<Expr> condition, shared_ptr<Expr> increment, shared_ptr<Stmt> body",
        "Break      : Token keyword",
        "Continue   : Token keyword"
    ], "#include \"Expr.h\"\n#include <vector>", "\n\ttemplate <typename T> using vector = std::vector<T>;")