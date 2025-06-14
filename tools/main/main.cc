#include "ASTheader.hh"
#include "FDMJAST.hh"
#include "ast2xml.hh"
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
#include "treep.hh"
#include "canon.hh"
#include "quad.hh"
#include "xml2tree.hh"
#include "tree2quad.hh"
#include "xml2quad.hh"
#include "quadssa.hh"
#include "blocking.hh"

using namespace std;
using namespace fdmj;
using namespace quad;
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
    string file_irp_canon = file + ".3-canon.irp";
    string file_quad = file + ".4.quad";
    string file_quad_block = file + ".4-block.quad";
    string file_quad_ssa = file + ".4-ssa.quad";


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
    cout << "Saving AST (XML) with Semantic Info to: " << file_ast_semant << endl;
    XMLDocument * x = ast2xml(root, semant_map, with_location_info, true);
    if (x->Error()) {
        std::cout << "AST is not valid when converting from AST with Semant Info!" << endl;
        return EXIT_FAILURE;
    }
    x->SaveFile(file_ast_semant.c_str());


    // converting AST to IR
    cout << "---0003---Converting AST to IR" << endl;
    // Compiler_Config::print_config();
    tree::Program *ir = ast2tree(root, semant_map);
    cout << "Saving IR (XML) to: " << file_irp << endl;
    x = tree2xml(ir);
    x->SaveFile(file_irp.c_str());


    // cout << "Reading IR (XML) from: " << file_irp << endl;
    // ir = xml2tree(file_irp);
    // if (ir == NULL) {
    //     cerr << "Error: " << file_irp << " not found or IR ill-formed" << endl;
    //     return EXIT_FAILURE;
    // }

    // Canonicalization of IR
    cout << "Canonicalization..." << endl;
    tree::Program *ir_canon = canon(ir);
    cout << "Saving Canonicalized IR to " << file_irp_canon << endl;
    XMLDocument *doc = tree2xml(ir_canon);
    doc->SaveFile(file_irp_canon.c_str());
    
    // Converting IR to Quad
    cout << "---0004---Converting IR to Quad" << endl;
    QuadProgram *qd = tree2quad(ir_canon);
    if (qd == nullptr) {
        cerr << "Error converting IR to Quad" << endl;
        return EXIT_FAILURE;
    }
    string temp_str; temp_str.reserve(50000);
    qd->print(temp_str, 0, true);
    cout << "Saving Quad to: " << file_quad << endl;
    ofstream qo(file_quad);
    if (!qo) {
        cerr << "Error opening file: " << file_quad << endl;
        return EXIT_FAILURE;
    }
    qo << temp_str;
    qo.flush(); qo.close();

    
    // Blocking the Quad
    cout << "Blocking the quad..." << endl;
    QuadProgram *x4 = blocking(qd);
    cout << "Saving blocked Quad to: " << file_quad_block << endl;
    temp_str.clear(); temp_str.reserve(100000);
    ofstream out_block(file_quad_block);
    if (!out_block) {
        cerr << "Error opening file: " << file_quad_block << endl;
        return EXIT_FAILURE;
    }
    x4->print(temp_str, 0, true);
    out_block << temp_str;
    out_block.flush(); out_block.close();

    // Converting Quad to Quad-SSA
    cout << "---0005---Converting Quad to Quad-SSA" << endl;
    //quad2ssa assumes blocked quad!!!
    QuadProgram *x5 = quad2ssa(x4);
    if (x5 == nullptr) {
        cerr << "Error converting Quad to Quad-SSA" << endl;
        return EXIT_FAILURE;
    }
    cout << "Saving Quad-SSA to: " << file_quad_ssa<< endl;
    ofstream out_ssa(file_quad_ssa);
    if (!out_ssa) {
        cerr << "Error opening file: " << file_quad_ssa << endl;
        return EXIT_FAILURE;
    }
    temp_str.clear(); temp_str.reserve(10000);
    x5->print(temp_str, 0, true);
    out_ssa << temp_str;
    out_ssa.flush(); out_ssa.close();
    cout << "-----Done---" << endl;

    return EXIT_SUCCESS;
}