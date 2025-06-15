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
#include "prepareregalloc.hh"
#include "regalloc.hh"

using namespace std;
using namespace fdmj;
using namespace quad;
using namespace tinyxml2;

#define with_location_info false
// false means no location info in the AST XML files

fdmj::Program *prog();

int main(int argc, const char *argv[]) {
    string file;
    int number_of_colors = 5; //default 9: r0-r8
    const bool noc = argc > 1 && std::strcmp(argv[1], "-k") == 0;

    if ((!noc && argc != 2) || (noc && argc != 4)) {
        cerr << "Usage: " << argv[0] << " [-k #colors] filename" << endl;
        return EXIT_FAILURE;
    }
    file = argv[argc - 1];
    if (argc == 4) number_of_colors = stoi(argv[2]);

    // boilerplate output filenames (used throughout the compiler pipeline)
    string file_fmj = file + ".fmj"; // input source file
    string file_ast_semant = file + ".2-semant.ast";
    string file_irp = file + ".3.irp";
    string file_irp_canon = file + ".3-canon.irp";
    string file_quad = file + ".4.quad";
    string file_quad_block = file + ".4-block.quad";
    string file_quad_ssa = file + ".5-ssa.quad";
    string file_quad_prepared = file + ".5-prepared.quad";
    string file_quad_color_xml = file + ".6-xml.clr";


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
    Name_Maps *name_maps = makeNameMaps(root); 
    AST_Semant_Map *semant_map = semant_analyze(root); 
    semant_map->setNameMaps(name_maps);
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
    cout << "Saving Quad to: " << file_quad << endl;
    quad2file(qd, file_quad.c_str(), true); //write the quad to a file

    
    // Blocking the Quad
    cout << "Blocking the quad..." << endl;
    QuadProgram *x4 = blocking(qd);
    cout << "Saving blocked Quad to: " << file_quad_block << endl;
    quad2file(x4, file_quad_block.c_str(), true); //write the blocked quad to a file


    // Converting Quad to Quad-SSA
    cout << "---0005---Converting Quad to Quad-SSA" << endl;
    QuadProgram *x5 = quad2ssa(x4);  //quad2ssa assumes blocked quad!!!
    if (x5 == nullptr) {
        cerr << "Error converting Quad to Quad-SSA" << endl;
        return EXIT_FAILURE;
    }
    cout << "Saving Quad-SSA to: " << file_quad_ssa<< endl;
    quad2file(x5, file_quad_ssa.c_str(), true); //write the quad-ssa to a file

    
    // Preparing the Quad for Register Allocation
    cout << "Preparing Quad for Register Allocation..." << endl;
    QuadProgram *x6 = prepareRegAlloc(x5); //using registers to prepare the quad for RPI
    cout << "Saving the prepared Quad Program to: " <<  file_quad_prepared << endl;
    quad2file(x6, file_quad_prepared.c_str(), true); //write the prepared quad to a file
    
    
    // Register Allocation (Coloring)
    cout << "---0006---Register Allocation (Coloring)---" << endl;
    cout << "Number of colors to use = " << number_of_colors << endl;
    XMLDocument *x7 = coloring(x6, number_of_colors, false); //coloring the quad program, print IGs along the way
    cout << "Saving regAlloc'ed (colors XML) to " << file_quad_color_xml << endl;
    x7->SaveFile(file_quad_color_xml.c_str());


    cout << "-----Done---" << endl;

    return EXIT_SUCCESS;
}