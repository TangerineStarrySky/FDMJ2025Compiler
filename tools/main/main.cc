#include "ASTheader.hh"
#include "FDMJAST.hh"
#include "ast2xml.hh"
// #include "xml2ast.hh"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include "config.hh"
#include "temp.hh"
#include "treep.hh"
#include "namemaps.hh"
#include "semant.hh"
#include "ast2tree.hh"
#include "tree2xml.hh"

using namespace std;
using namespace fdmj;
using namespace tinyxml2;

#define with_location_info false
// false means no location info in the AST XML files

fdmj::Program *prog();

int main(int argc, const char *argv[]) {
    string file;
    const bool debug = argc > 1 && std::strcmp(argv[1], "--debug") == 0;

    if ((!debug && argc != 2) || (debug && argc != 3)) {
        cerr << "Usage: " << argv[0] << " [--debug] filename" << endl;
        return EXIT_FAILURE;
    }

    file = argv[argc - 1];

    // boilerplate output filenames (used throughout the compiler pipeline)
    string file_fmj = file + ".fmj"; // input source file
    string file_ast_semant = file + ".2-semant.ast";
    string file_irp = file + ".3.irp";

    // Parsing the fmj source file
    cout << "---0001---Parsing fmj source file: " << file_fmj << "------------" << endl;
    std::ifstream fmjfile(file_fmj);
    fdmj::Program *root = fdmjParser(fmjfile, false); // false means no debug info from parser
    if (root == nullptr) {
        std::cout << "AST is not valid!" << endl;
        return EXIT_FAILURE;
    }

    // Semantic analysis
    std::cout << "---0002---Semantic analysis..." << std::endl;
    // std::cout << "--Making Name Maps..." << endl;
    Name_Maps *name_maps = makeNameMaps(root); 
    // std::cout << "--Analyzing Semantics..." << endl;
    AST_Semant_Map *semant_map = semant_analyze(root); 
    semant_map->setNameMaps(name_maps);
    // semant_map->getNameMaps()->print();
    // cout << "Convert AST to XML with Semantic Info..." << endl;
    XMLDocument * x = ast2xml(root, semant_map, with_location_info, true);
    if (x->Error()) {
        std::cout << "AST is not valid when converting from AST with Semant Info!" << endl;
        return EXIT_FAILURE;
    }
    x->SaveFile(file_ast_semant.c_str());
    cout << "Saving AST (XML) with Semantic Info to: " << file_ast_semant << endl;

    // converting AST to IR
    cout << "---0003---Converting AST to IR" << endl;
    // Compiler_Config::print_config();
    tree::Program *ir = ast2tree(root, semant_map);
    cout << "Saving IR (XML) to: " << file_irp << endl;
    x = tree2xml(ir);
    x->SaveFile(file_irp.c_str());



    cout << "-----Done---" << endl;

    return EXIT_SUCCESS;
}