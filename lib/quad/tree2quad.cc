#define DEBUG
#undef DEBUG

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include "treep.hh"
#include "quad.hh"
#include "tree2quad.hh"

using namespace std;
using namespace tree;
using namespace quad;


/*
We use an instruction selection method (pattern matching) to convert the IR tree to Quad.
The Quad is a simplified tree node/substructure, each Quad is a tree pattern:
Move:  temp <- temp
Load:  temp <- mem(temp)
Store: mem(temp) <- temp
MoveBinop: temp <- temp op temp
Call:  ExpStm(call) //ignore the result
ExtCall: ExpStm(extcall) //ignore the result
MoveCall: temp <- call
MoveExtCall: temp <- extcall
Label: label
Jump: jump label
CJump: cjump relop temp, temp, label, label
Phi:  temp <- list of {temp, label} //same as the Phi in the tree
*/

QuadProgram* tree2quad(Program* prog) {
#ifdef DEBUG
    cout << "in Tree2Quad::Converting IR to Quad" << endl;
#endif
    Tree2Quad visitor;
    prog->accept(visitor);
    return visitor.quadprog;
}

//You need to write all the visit functions below. Now they are all "dummies".
void Tree2Quad::visit(Program* prog) {
#ifdef DEBUG
    cout << "Converting to Quad: Program" << endl;
#endif
    vector<QuadFuncDecl*> *funcDecls = new vector<QuadFuncDecl*>();
    quadprog = new QuadProgram(prog, funcDecls);
    
    for (auto func : *(prog->funcdecllist)) {
        func->accept(*this);
    }
    
    visit_result = nullptr;
    output_term = nullptr;
}

void Tree2Quad::visit(FuncDecl* node) {
#ifdef DEBUG
    cout << "Converting to Quad: FunctionDeclaration" << endl;
#endif
    temp_map = new Temp_map();
    temp_map->next_temp = node->last_temp_num + 1;
    temp_map->next_label = node->last_label_num + 1;
    vector<QuadBlock*> *quadblocklist = new vector<QuadBlock*>();
    vector<Temp*> *params = new vector<Temp*>();
    QuadFuncDecl *quadFuncDecl = new QuadFuncDecl(node, node->name, node->args, quadblocklist, 0, 0);
    quadprog->quadFuncDeclList->push_back(quadFuncDecl);

    for (auto block : *(node->blocks)) {
        block->accept(*this);
    }
    quadprog->quadFuncDeclList->back()->last_label_num = temp_map->next_label - 1;
    quadprog->quadFuncDeclList->back()->last_temp_num = temp_map->next_temp - 1;
    visit_result = nullptr;
}

void Tree2Quad::visit(Block *node) {
#ifdef DEBUG
    cout << "Converting to Quad: Block" << endl;
#endif
    visit_result = new vector<QuadStm*>();

    for (auto stm : *(node->sl)) {
        stm->accept(*this);
    }

    QuadBlock *quadBlock = new QuadBlock(node, visit_result, node->entry_label, node->exit_labels);
    quadprog->quadFuncDeclList->back()->quadblocklist->push_back(quadBlock);
    visit_result = nullptr;
}

void Tree2Quad::visit(Jump* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Jump" << endl;
#endif
    if (node == nullptr) {
        visit_result = nullptr;
        return;
    }
    set<Temp*> *def = new set<Temp*>();
    set<Temp*> *use = new set<Temp*>();
    visit_result->push_back(new QuadJump(node, node->label, def, use));
    output_term = nullptr;
}

void Tree2Quad::visit(Cjump* node) {
#ifdef DEBUG
    cout << "Converting to Quad: CJump" << endl;
#endif
//only one tile pattern matches this CJump
    if (node == nullptr || node->right == nullptr || node->left == nullptr) {
        visit_result = nullptr;
        return;
    }
    set<Temp*> *def = new set<Temp*>(); 
    set<Temp*> *use = new set<Temp*>();
    node->left->accept(*this);
    QuadTerm *left = output_term;
    if(output_term->kind == QuadTermKind::TEMP) {
        use->insert(output_term->get_temp()->temp);
    }
    node->right->accept(*this);
    QuadTerm *right = output_term;
    if(output_term->kind == QuadTermKind::TEMP) {
        use->insert(output_term->get_temp()->temp);
    }
    QuadCJump *quadCJump = new QuadCJump(node, node->relop, left, right, node->t, node->f, def, use);
    visit_result->push_back(quadCJump);
    output_term = nullptr;
}

void Tree2Quad::visit(Move* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Move" << endl;
#endif
    set<Temp*> *def = new set<Temp*>();
    set<Temp*> *use = new set<Temp*>();
    if(tree::ExtCall* node_extcall = dynamic_cast<tree::ExtCall*>(node->src)) {
        // cout << "extcall" << endl;
        vector<QuadTerm*> *args = new vector<QuadTerm*>();
        for (auto arg : *(node_extcall->args)) {
            arg->accept(*this);
            args->push_back(output_term);
            if(output_term->kind == QuadTermKind::TEMP) {
                use->insert(output_term->get_temp()->temp);
            }
        }
        node->dst->accept(*this);
        TempExp *dst = output_term->get_temp();
        def->insert(dst->temp);
        QuadExtCall *quadExtCall = new QuadExtCall(node, node_extcall->extfun, args, def, use);
        visit_result->push_back(new QuadMoveExtCall(node, dst, quadExtCall, def, use));
        if(node->dst->getTreeKind() == Kind::MEM){
            // If the destination is a memory location, we need to store the result
            // cout << "store result of call" << endl;
            tree::Mem* node_mem = dynamic_cast<tree::Mem*>(node->dst);
            QuadTerm *result = output_term;
            set<Temp*> *def_mem = new set<Temp*>();
            set<Temp*> *use_mem = new set<Temp*>();
            if(result->kind == QuadTermKind::TEMP) {
                use_mem->insert(result->get_temp()->temp);
            }
            node_mem->mem->accept(*this);
            QuadTerm *dst = output_term;
            if(output_term->kind == QuadTermKind::TEMP) {
                use_mem->insert(output_term->get_temp()->temp);
            }
            visit_result->push_back(new QuadStore(node, result, dst, def_mem, use_mem));
        }
    } else if(tree::Call* node_call = dynamic_cast<tree::Call*>(node->src)) {
        // cout << "call" << endl;
        vector<QuadTerm*> *args = new vector<QuadTerm*>();
        for (auto arg : *(node_call->args)) {
            arg->accept(*this);
            args->push_back(output_term);
            if(output_term->kind == QuadTermKind::TEMP) {
                use->insert(output_term->get_temp()->temp);
            }
        }
        node_call->obj->accept(*this);
        QuadTerm *obj_term = output_term;
        if(output_term->kind == QuadTermKind::TEMP) {
            use->insert(output_term->get_temp()->temp);
        }
        node->dst->accept(*this);
        TempExp *dst = output_term->get_temp();
        def->insert(dst->temp);
        QuadCall *quadCall = new QuadCall(node, node_call->id, obj_term, args, def, use);
        visit_result->push_back(new QuadMoveCall(node, dst, quadCall, def, use));
        if(node->dst->getTreeKind() == Kind::MEM){
            // If the destination is a memory location, we need to store the result
            // cout << "store result of call" << endl;
            tree::Mem* node_mem = dynamic_cast<tree::Mem*>(node->dst);
            QuadTerm *result = output_term;
            set<Temp*> *def_mem = new set<Temp*>();
            set<Temp*> *use_mem = new set<Temp*>();
            if(result->kind == QuadTermKind::TEMP) {
                use_mem->insert(result->get_temp()->temp);
            }
            node_mem->mem->accept(*this);
            QuadTerm *dst = output_term;
            if(output_term->kind == QuadTermKind::TEMP) {
                use_mem->insert(output_term->get_temp()->temp);
            }
            visit_result->push_back(new QuadStore(node, result, dst, def_mem, use_mem));
        }
    } else if(tree::Mem* node_mem = dynamic_cast<tree::Mem*>(node->dst)){
        // store
        // cout << "store" << endl;
        node_mem->mem->accept(*this);
        QuadTerm *dst = output_term;
        if(output_term->kind == QuadTermKind::TEMP) {
            use->insert(output_term->get_temp()->temp);
        }
        node->src->accept(*this);
        QuadTerm *src = output_term;
        if(output_term->kind == QuadTermKind::TEMP) {
            use->insert(output_term->get_temp()->temp);
        }
        visit_result->push_back(new QuadStore(node, src, dst, def, use));
    } else if(tree::Binop* node_binop = dynamic_cast<tree::Binop*>(node->src)) {
        // cout << "binop" << endl;
        node_binop->left->accept(*this);
        QuadTerm *left = output_term;
        if(output_term->kind == QuadTermKind::TEMP) {
            use->insert(output_term->get_temp()->temp);
        }
        node_binop->right->accept(*this);
        QuadTerm *right = output_term;
        if(output_term->kind == QuadTermKind::TEMP) {
            use->insert(output_term->get_temp()->temp);
        }

        node->dst->accept(*this);
        TempExp *dst = output_term->get_temp();
        def->insert(dst->temp);
        visit_result->push_back(new QuadMoveBinop(node, dst, left, node_binop->op, right, def, use));
    } else if(node->src->getTreeKind() == Kind::CONST || node->src->getTreeKind() == Kind::NAME || node->src->getTreeKind() == Kind::TEMPEXP) {
        // cout << "move const/name/temp" << endl;
        node->src->accept(*this);
        QuadTerm *src = output_term;
        if(output_term->kind == QuadTermKind::TEMP) {
            use->insert(output_term->get_temp()->temp);
        }
        node->dst->accept(*this);
        TempExp *dst = output_term->get_temp();
        def->insert(dst->temp);
        QuadMove *quadMove = new QuadMove(node, dst, src, def, use);
        visit_result->push_back(quadMove);
    } else if(tree::Mem* node_mem = dynamic_cast<tree::Mem*>(node->src)){
        // cout << "load" << endl;
        node_mem->mem->accept(*this);
        QuadTerm *src = output_term;
        if(output_term->kind == QuadTermKind::TEMP) {
            use->insert(output_term->get_temp()->temp);
        }
        node->dst->accept(*this);
        TempExp *dst = output_term->get_temp();
        def->insert(dst->temp);
        visit_result->push_back(new QuadLoad(node, dst, src, def, use));
    } else {
        cout << "move error" << endl;
    }
        
    output_term = nullptr;
}

void Tree2Quad::visit(Seq* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Sequence" << endl;
#endif
    for (auto stm : *(node->sl)) {
        stm->accept(*this);
    }
    output_term = nullptr;
}

void Tree2Quad::visit(LabelStm* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Label" << endl;
#endif
    if (node == nullptr) {
        visit_result = nullptr;
        return;
    }
    set<Temp*> *def = new set<Temp*>();
    set<Temp*> *use = new set<Temp*>();
    visit_result->push_back(new QuadLabel(node, node->label, def, use));
    output_term = nullptr;
}

void Tree2Quad::visit(Return* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Return" << endl;
#endif
    if (node == nullptr || node->exp == nullptr) {
        visit_result = nullptr;
        return;
    }
    set<Temp*> *def = new set<Temp*>();
    set<Temp*> *use = new set<Temp*>();
    node->exp->accept(*this);
    if(output_term->kind == QuadTermKind::TEMP) {
        TempExp *temp = output_term->get_temp();
        use->insert(temp->temp);
    }
    QuadReturn *quadReturn = new QuadReturn(node, output_term, def, use);
    visit_result->push_back(quadReturn);
    output_term = nullptr;
}

// Phi is not used in the Quad
void Tree2Quad::visit(Phi* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Phi" << endl;
#endif
    if (node == nullptr) {
        visit_result = nullptr;
        return;
    }
    visit_result = nullptr;
    output_term = nullptr;
}

void Tree2Quad::visit(ExpStm* node) {
#ifdef DEBUG
    cout << "Converting to Quad: ExpressionStatement" << endl;
#endif
    if (node == nullptr || node->exp == nullptr) {
        visit_result = nullptr;
        return;
    }
    node->exp->accept(*this);
    output_term = nullptr;
}

void Tree2Quad::visit(Binop* node) {
#ifdef DEBUG
    cout << "Converting to Quad: BinaryOperation" << endl;
#endif
    if (node == nullptr || node->left == nullptr || node->right == nullptr) {
        visit_result = nullptr;
        return;
    }
    set<Temp*> *def = new set<Temp*>();
    set<Temp*> *use = new set<Temp*>();
    node->left->accept(*this);
    QuadTerm *left = output_term;
    if(output_term->kind == QuadTermKind::TEMP) {
        use->insert(output_term->get_temp()->temp);
    }
    node->right->accept(*this);
    QuadTerm *right = output_term;
    if(output_term->kind == QuadTermKind::TEMP) {
        use->insert(output_term->get_temp()->temp);
    }
    TempExp* dst = new TempExp(node->type, temp_map->newtemp());
    def->insert(dst->temp);
    visit_result->push_back(new QuadMoveBinop(node, dst, left, node->op, right, def, use));
    output_term = new QuadTerm(dst);
}

//convert the memory address to a load quad
void Tree2Quad::visit(Mem* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Memory" << endl;
#endif
    if (node == nullptr || node->mem == nullptr) {
        cerr << "Error: memory address is missing!" << endl;
        visit_result = nullptr;
        return;
    }
    set<Temp*> *def = new set<Temp*>();
    set<Temp*> *use = new set<Temp*>();
    node->mem->accept(*this);
    QuadTerm *src = output_term;
    if(output_term->kind == QuadTermKind::TEMP) {
        use->insert(output_term->get_temp()->temp);
    }
    TempExp* dst = new TempExp(node->type, temp_map->newtemp());
    def->insert(dst->temp);
    visit_result->push_back(new QuadLoad(node, dst, src, def, use));
    output_term = new QuadTerm(dst);
}

void Tree2Quad::visit(TempExp* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Temp" << endl;
#endif
    if (node == nullptr) {
        cerr << "Error: Temp is null!" << endl;
        visit_result = nullptr;
        return;
    }
    output_term = new QuadTerm(node);
}

//the following is useless since IR is canon
void Tree2Quad::visit(Eseq* node) {
#ifdef DEBUG
    cout << "Converting to Quad: ESeq" << endl;
#endif
    visit_result = nullptr;
    output_term = nullptr;
}

//convert the label to a QuadTerm(name)
void Tree2Quad::visit(Name* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Name" << endl;
#endif
    if (node == nullptr) {
        visit_result = nullptr;
        return;
    }
    output_term = new QuadTerm(node);
}

void Tree2Quad::visit(Const* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Const" << endl;
#endif
    if (node == nullptr) {
        visit_result = nullptr;
        return;
    }
    output_term = new QuadTerm(node);
}

void Tree2Quad::visit(Call* node) {
#ifdef DEBUG
    cout << "Converting to Quad: Call" << endl;
#endif
    if (node == nullptr) {
        visit_result = nullptr;
        return;
    }
    set<Temp*> *def = new set<Temp*>();
    set<Temp*> *use = new set<Temp*>();
    vector<QuadTerm*> *args = new vector<QuadTerm*>();
    for (auto arg : *(node->args)) {
        arg->accept(*this);
        args->push_back(output_term);
        if(output_term->kind == QuadTermKind::TEMP) {
            use->insert(output_term->get_temp()->temp);
        }
    }
    node->obj->accept(*this);
    QuadTerm *obj_term = output_term;
    if(output_term->kind == QuadTermKind::TEMP) {
        use->insert(output_term->get_temp()->temp);
    }
    visit_result->push_back(new QuadCall(node, node->id, obj_term, args, def, use));
    output_term = nullptr;
}

void Tree2Quad::visit(ExtCall* node) {
#ifdef DEBUG
    cout << "Converting to Quad: ExtCall" << endl;
#endif
    if (node == nullptr) {
        visit_result = nullptr;
        return;
    }
    set<Temp*> *def = new set<Temp*>();
    set<Temp*> *use = new set<Temp*>();
    vector<QuadTerm*> *args = new vector<QuadTerm*>();
    for (auto arg : *(node->args)) {
        arg->accept(*this);
        args->push_back(output_term);
        if(output_term->kind == QuadTermKind::TEMP) {
            use->insert(output_term->get_temp()->temp);
        }
    }
    // TempExp* dst = new TempExp(node->type, temp_map->newtemp());
    // def->insert(dst->temp);
    visit_result->push_back(new QuadExtCall(node, node->extfun, args, def, use));
    output_term = nullptr;
}