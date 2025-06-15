#define DEBUG
#undef DEBUG
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include "quad.hh"
#include "flowinfo.hh"
#include "color.hh"
#include "quad2rpi.hh"

static string current_funcname = "";

//This is to convert the names of the functions to a format that is acceptable by the assembler
string normalizeName(string name) {
    // Normalize the name by replacing special characters with underscores
    if (name == "_^main^_^main") { //speial case for main
        return "main";
    }
    for (char& c : name) {
        if (!isalnum(c)) {
            c = '$';
        }
    }
    return name;
}

bool rpi_isMachineReg(int n) {
    // Check if a node is a machine register
    return (n >= 0 && n <= 15);
}

string temp2str(TempExp *temp, Color *color, int reg=9) {
    Temp *t = temp->temp;
    if (rpi_isMachineReg(t->num)) {
        // If the temp is a machine register, return it directly
        return "r" + to_string(t->num);
    } else if (color->is_spill(t->num)) {
        return "r" + to_string(reg);
    }
    return "r" + to_string(color->color_of(t->num));
}

string term2str(QuadTerm *term, Color *color, int reg=9) {
    string result;
    if (term->kind == QuadTermKind::TEMP) {
        result = temp2str(term->get_temp(), color, reg);
    } else if (term->kind == QuadTermKind::CONST) {
        result = "#" + to_string(term->get_const());
    } else if (term->kind == QuadTermKind::MAME) {
        result = "@" + term->get_name();
    } else {
        cerr << "Error: Unknown term kind" << endl;
        exit(EXIT_FAILURE);
    }
    return result;
}

//Always use function name to prefix a label
//Note that you should do the same for Jump and CJump labels
string convert(QuadLabel* label, Color *c, int indent) {
#ifdef DEBUG
    cout << "In convert Label" << endl;
#endif
    string result; 
    result = current_funcname + "$" + label->label->str() + ": \n";
    return result;
}

string convert(Label* label, Color *c) {
#ifdef DEBUG
    cout << "In convert Label" << endl;
#endif
    return current_funcname + "$" + label->str();
}

string load_temp(TempExp *temp, Color *color, int indent, int reg=9) {
    // Load a temp into a register
    Temp *t = temp->temp;
    if (color->is_spill(t->num)) {
        // If the temp is spilled, use the spill offset
        int offset = color->get_spill_offset(t->num);
        return string(indent, ' ') + "ldr r" + to_string(reg) + ", [fp, #-" + to_string(offset) + "]\n";
    } else {
        return "";
    }
}

string load_term(QuadTerm *term, Color *color, int indent, int reg=9) {
    // Load a term into a register
    if (term->kind == QuadTermKind::TEMP)
        return load_temp(term->get_temp(), color, indent, reg);
    return "";
}

string store_temp(TempExp *temp, Color *color, int indent, int reg=10) {
    // Store a temp from a register
    Temp *t = temp->temp;
    if (color->is_spill(t->num)) {
        // If the temp is spilled, use the spill offset
        int offset = color->get_spill_offset(t->num);
        return string(indent, ' ') + "str r" + to_string(reg) + ", [fp, #-" + to_string(offset) + "]\n";
    } else {
        return "";
    }
}

string store_term(QuadTerm *term, Color *color, int indent, int reg=10) {
    // Load a term into a register
    if (term->kind == QuadTermKind::TEMP)
        return store_temp(term->get_temp(), color, indent, reg);
    return "";
}

string prev_sentence(string text){
    int firstPos = text.rfind('\n'); // 找到最后一个换行符的位置
    int secondPos = text.rfind('\n', firstPos - 1); // 从第一个换行符位置向前找第二个换行符的位置
    string result = text.substr(secondPos + 1, firstPos - secondPos - 1); // 提取两个换行符之间的内容
    return result;
}

/*************************************************** */
/*************************************************** */
/*************************************************** */
/*************************************************** */
/*************************************************** */
/*************************************************** */

string convert(QuadFuncDecl* func, DataFlowInfo *dfi, Color *color, int indent) {
    string result; 
    current_funcname = normalizeName(func->funcname);
    // Iterate through Quads in the function and convert
    // one statement at a time (sometimes two statemetns can be combined into one, try that!)
    // 1) Set up the frame stack correctly at beginning of the function, and return
    // 2) Replace temp with register using the color map
    // 3) Take care of the spilled temps
    // 4) and other details

    result += "@ Here's function: " + func->funcname + "\n";
    result += "\n.balign 4\n";
    result += ".global " + current_funcname + "\n";
    result += ".section .text\n\n";
    result += current_funcname + ": \n";
    result += string(indent, ' ') + "push {r4-r10, fp, lr}\n";
    result += string(indent, ' ') + "add fp, sp, #32\n"; 
    if(color->spills.size() > 0)
        result += string(indent, ' ') + "sub sp, sp, #" + to_string(color->spills.size() * Compiler_Config::get("int_length")) + "\n";
    // cout << func->quadblocklist->size() << " blocks in function " << current_funcname << endl;
    // cout << (*func->quadblocklist)[0]->quadlist->size() << " stms in function " << endl;
    for (QuadStm* quadstm : *(*func->quadblocklist)[0]->quadlist) {
        if (quadstm == nullptr) continue;
        if (quadstm->kind == QuadKind::LABEL) {
            // Convert label
            QuadLabel *label = static_cast<QuadLabel*>(quadstm);
            // If the last line jumps to this label, remove it
            string last_instruction = prev_sentence(result);  // b main$L110
            string sub = convert(label->label, color);  // main$L110
            // cout << last_instruction << endl;
            if(last_instruction.find(sub) != string::npos) {
                size_t pos = result.rfind(last_instruction);
                result.erase(pos, last_instruction.length()+1); // +1 for \n
            }
            result += convert(label, color, indent);
        } else if (quadstm->kind == QuadKind::JUMP) {
            // Convert jump
            QuadJump *jump = static_cast<QuadJump*>(quadstm);
            result += string(indent, ' ') + "b " + convert(jump->label, color) + "\n";
        } else if (quadstm->kind == QuadKind::CJUMP) {
            // Convert conditional jump
            QuadCJump *cjump = static_cast<QuadCJump*>(quadstm);
            result += load_term(cjump->left, color, indent);
            result += load_term(cjump->right, color, indent, 10);
            result += string(indent, ' ') + "cmp " + term2str(cjump->left, color) + ", " + term2str(cjump->right, color, 10) + "\n";
            if (cjump->relop == "==") {
                result += string(indent, ' ') + "beq " + convert(cjump->t, color) + "\n";
            } else if (cjump->relop == "!=") {
                result += string(indent, ' ') + "bne " + convert(cjump->t, color) + "\n";
            } else if (cjump->relop == "<") {
                result += string(indent, ' ') + "blt " + convert(cjump->t, color) + "\n";
            } else if (cjump->relop == "<=") {
                result += string(indent, ' ') + "ble " + convert(cjump->t, color) + "\n";
            } else if (cjump->relop == ">") {
                result += string(indent, ' ') + "bgt " + convert(cjump->t, color) + "\n";
            } else if (cjump->relop == ">=") {
                result += string(indent, ' ') + "bge " + convert(cjump->t, color) + "\n";
            }
            result += string(indent, ' ') + "b " + convert(cjump->f, color) + "\n"; // Unconditional jump to false label
        } else if (quadstm->kind == QuadKind::MOVE) {
            // Convert move
            QuadMove *move = static_cast<QuadMove*>(quadstm);
            if(temp2str(move->dst, color, 10) == term2str(move->src, color)) {
                continue;
            }
            result += load_term(move->src, color, indent);
            result += string(indent, ' ') + "mov " + temp2str(move->dst, color, 10) + ", " + term2str(move->src, color) + "\n";
            result += store_temp(move->dst, color, indent);
        } else if (quadstm->kind == QuadKind::LOAD) {
            // Convert load
            QuadLoad *load = static_cast<QuadLoad*>(quadstm);
            string tmp;
            if (load->src->kind == QuadTermKind::MAME) 
                tmp = string(indent, ' ') + "ldr " + temp2str(load->dst, color, 10) + ", =" + normalizeName(load->src->get_name()) + "\n";
            else tmp = string(indent, ' ') + "ldr " + temp2str(load->dst, color, 10) + ", [" + term2str(load->src, color) + "]\n";
            
            string last_instruction = prev_sentence(result);  // add r0, r9, #0
            istringstream iss(last_instruction);
            string opcode, destReg, srcReg1, srcReg2;
            iss >> opcode;
            if(opcode == "add")
            {
                iss >> destReg >> srcReg1 >> srcReg2;
                destReg.erase(destReg.size() - 1);
                srcReg1.erase(srcReg1.size() - 1);
                // cout << opcode << " " << destReg << " " << srcReg1 << " " << srcReg2 << endl;
                string load_src = term2str(load->src, color);
                if(destReg == load_src && srcReg1[0] == 'r' && srcReg2[0] == '#') {
                    result.erase(result.rfind(last_instruction), last_instruction.length()+1); // +1 for \n
                    tmp = string(indent, ' ') + "ldr " + temp2str(load->dst, color, 10) + ", [" + srcReg1 + ", " + srcReg2 + "]\n";
                }
            }
            
            result += load_term(load->src, color, indent); 
            result += tmp;
            result += store_temp(load->dst, color, indent);
        } else if (quadstm->kind == QuadKind::STORE) {
            // Convert store
            QuadStore *store = static_cast<QuadStore*>(quadstm);
            string tmp = string(indent, ' ') + "str " + term2str(store->src, color) + ", [" + term2str(store->dst, color, 10) + "]\n";
            
            string last_instruction = prev_sentence(result);  // add r0, r9, #0
            istringstream iss(last_instruction);
            string opcode, destReg, srcReg1, srcReg2;
            iss >> opcode;
            if(opcode == "add")
            {
                iss >> destReg >> srcReg1 >> srcReg2;
                destReg.erase(destReg.size() - 1);
                srcReg1.erase(srcReg1.size() - 1);
                // cout << opcode << " " << destReg << " " << srcReg1 << " " << srcReg2 << endl;
                string store_dest = term2str(store->dst, color);
                if(destReg == store_dest && srcReg1[0] == 'r' && srcReg2[0] == '#') {
                    result.erase(result.rfind(last_instruction), last_instruction.length()+1); // +1 for \n
                    tmp = string(indent, ' ') + "str " + term2str(store->src, color) + ", [" + srcReg1 + ", " + srcReg2 + "]\n";
                }
            }
            
            result += load_term(store->src, color, indent);
            result += load_term(store->dst, color, indent, 10);
            result += tmp;
        } else if (quadstm->kind == QuadKind::MOVE_BINOP) {
            // Convert binary operation move
            QuadMoveBinop *movebinop = static_cast<QuadMoveBinop*>(quadstm);
            string rpi_op;
            if (movebinop->binop == "+") {
                rpi_op = "add ";
            } else if( movebinop->binop == "-") {
                rpi_op = "sub ";
            } else if (movebinop->binop == "*") {
                rpi_op = "mul ";
            } else if (movebinop->binop == "/") {
                rpi_op = "sdiv ";
            }
            result += load_term(movebinop->left, color, indent); 
            result += load_term(movebinop->right, color, indent, 10);
            result += string(indent, ' ') + rpi_op + temp2str(movebinop->dst, color, 10) + ", " 
                    + term2str(movebinop->left, color) + ", " + term2str(movebinop->right, color, 10) + "\n";
            result += store_temp(movebinop->dst, color, indent);
        } else if (quadstm->kind == QuadKind::CALL) {
            // Convert call
            QuadCall *call = static_cast<QuadCall*>(quadstm);
            result += load_term(call->obj_term, color, indent);
            for (QuadTerm *arg : *call->args) {
                result += load_term(arg, color, indent);
            }
            result += string(indent, ' ') + "blx " + term2str(call->obj_term, color) + "\n"; // Call the function
        } else if (quadstm->kind == QuadKind::MOVE_CALL) {
            // Convert call
            QuadMoveCall *call = static_cast<QuadMoveCall*>(quadstm);
            result += load_term(call->call->obj_term, color, indent);
            for (QuadTerm *arg : *call->call->args) {
                result += load_term(arg, color, indent);
            }
            result += string(indent, ' ') + "blx " + term2str(call->call->obj_term, color) + "\n";
            // result += string(indent, ' ') + "mov " + temp2str(call->dst, color) + ", r0\n"; // Assume return value in r0
            result += store_temp(call->dst, color, indent); // Store the result in the temp
        } else if (quadstm->kind == QuadKind::EXTCALL) {
            // Convert external call
            QuadExtCall *extcall = static_cast<QuadExtCall*>(quadstm);
            for (QuadTerm *arg : *extcall->args) {
                result += load_term(arg, color, indent);
            }
            // Call the external function
            result += string(indent, ' ') + "bl " + extcall->extfun + "\n";
        } else if (quadstm->kind == QuadKind::MOVE_EXTCALL) {
            // Convert move external call
            QuadMoveExtCall *move_extcall = static_cast<QuadMoveExtCall*>(quadstm);
            for (QuadTerm *arg : *move_extcall->extcall->args) {
                result += load_term(arg, color, indent);
            }
            result += string(indent, ' ') + "bl " + move_extcall->extcall->extfun + "\n";
            // result += string(indent, ' ') + "mov " + temp2str(move_extcall->dst, color) + ", r0\n"; // Assume return value in r0
            result += store_temp(move_extcall->dst, color, indent); // Store the result in the temp
        } else if (quadstm->kind == QuadKind::RETURN) {
            // Convert return
            QuadReturn *ret = static_cast<QuadReturn*>(quadstm);
            result += load_term(ret->value, color, indent);
            // result += string(indent, ' ') + "mov r0, " + term2str(ret->value, color) + "\n"; // Assume return value in r0
            result += string(indent, ' ') + "sub sp, fp, #32\n"; // Restore stack pointer
            result += string(indent, ' ') + "pop {r4-r10, fp, pc}\n"; // Restore registers and return
        } else if (quadstm->kind == QuadKind::PHI) {
            // Convert phi (not needed in RPI)
        } 
    }


    return result;
}

string quad2rpi(QuadProgram* quadProgram, ColorMap *cm) {// Convert a QuadProgram to RPI format with k registers
    string result; result.reserve(10000);
    // Iterate through the function declarations in the Quad program
    result = ".section .note.GNU-stack\n\n@ Here is the RPI code\n\n";
    for (QuadFuncDecl* func : *quadProgram->quadFuncDeclList) {
        //get the data flow info for the function
        DataFlowInfo *dfi = new DataFlowInfo(func);
        dfi->computeLiveness(); //liveness useful in some cases. Has to be done before trace otherwise this func code won't work!
        trace(func); //trace it (merge all blocks into one)
        current_funcname = func->funcname; //set the global variable
        //get the color for the function
        Color *c = cm->color_map[func->funcname]; 
        int indent = 8;
        result += convert(func, dfi, c, indent) + "\n";
    }
    //put the global functions at the end
    result += ".global malloc\n";
    result +=".global getint\n";
    result += ".global putint\n";
    result += ".global putch\n";
    result += ".global putarray\n";
    result += ".global getch\n";
    result += ".global getarray\n";
    result += ".global starttime\n";
    result += ".global stoptime\n";
    return result;
}

// Print the RPI code to the output file
void quad2rpi(QuadProgram* quadProgram, ColorMap *cm, string filename) {
    ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << quad2rpi(quadProgram, cm);
        outfile.flush();
        outfile.close();
    } else {
        cerr << "Error: Unable to open file " << filename << endl;
    }
}