#define DEBUG
#undef DEBUG

#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <algorithm>
#include "namemaps.hh"
#include "semant.hh"

using namespace std;
using namespace fdmj;

// TODO
AST_Semant_Map* semant_analyze(Program* node) {
    // std::cerr << "Start Semantic Analysis" << std::endl;
    if (node == nullptr) {
        return nullptr;
    }
    Name_Maps* name_maps = makeNameMaps(node);
    AST_Semant_Visitor semant_visitor(name_maps);
    semant_visitor.visit(node);
    // std::cerr << "Semantic Analysis Done" << std::endl;
    return semant_visitor.getSemantMap();
}

void AST_Semant_Visitor::visit(Program* node) {
#ifdef DEBUG
    cerr << "Program" << endl;
#endif
    if (node == nullptr) {
        return;
    }
    if (node->main) {
        current_class = "_^main^_";
        current_method = "main";
        node->main->accept(*this);
        current_class.clear();
        current_method.clear();
    }
    if (node->cdl) {
        for (auto* cl : *node->cdl) {
            cl->accept(*this);
        }
    }
}

void AST_Semant_Visitor::visit(MainMethod* node) {
#ifdef DEBUG
    cerr << "MainMethod" << endl;
#endif
    if (node->vdl) {
        for (auto* vd : *node->vdl) {
            vd->accept(*this);
        }
    }
    if (node->sl) {
        for (auto* stm : *node->sl) {
            stm->accept(*this);
        }
    }
}

void AST_Semant_Visitor::visit(ClassDecl* node) {
#ifdef DEBUG
    cerr << "ClassDecl" << endl;
#endif
    current_class = node->id->id;
    #ifdef DEBUG
    cout << "-----------class decl: " << current_class << endl;
    #endif
    // node->id->accept(*this);
    // if (node->eid) node->eid->accept(*this);
    // 遍历类中的变量声明和方法声明
    if (node->vdl) {
        for (auto* var_decl : *node->vdl) {
            var_decl->accept(*this);
        }
    }
    if (node->mdl) {
        for (auto* method_decl : *node->mdl) {
            method_decl->accept(*this);
        }
    }
    current_class.clear();
}

void AST_Semant_Visitor::visit(Type* node) {
#ifdef DEBUG
    cerr << "Type" << endl;
#endif
    // AST_Semant::Kind kind = AST_Semant::Kind::Value;
    // TypeKind typeKind = node->typeKind;
    // variant<monostate, string, int> type_par;
    // if (typeKind == TypeKind::ARRAY) {
    //     type_par = node->arity->val;
    // } else if (typeKind == TypeKind::CLASS) {
    //     type_par = node->cid->id;
    // }
    // AST_Semant* semant = new AST_Semant(kind, typeKind, type_par, false);
    // semant_map->setSemant(node, semant);
    // if (node->cid) node->cid->accept(*this);
    // if (node->arity) node->arity->accept(*this);
}

void AST_Semant_Visitor::visit(VarDecl* node) {
#ifdef DEBUG
    cerr << "VarDecl" << endl;
#endif
    // if(node->type->typeKind == TypeKind::ARRAY){
    //     cout << "array type: " << node->type->arity->val << endl;
    // }
    // node->type->accept(*this); // 先分析类型

    // AST_Semant* type_semant = semant_map->getSemant(node->type);
    // cout << "********" << AST_Semant::s_kind_string(type_semant->get_kind()) << type_kind_string(type_semant->get_type()) << endl;
    
    // if(type_semant->get_type() != TypeKind::ARRAY){
    //     AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, type_semant->get_type(), type_semant->get_type_par(), true);
    //     semant_map->setSemant(node, semant);
    // }

    // 如果有初始化表达式，分析初始化表达式
    // if (node->init.index() == 1 || node->init.index() == 2) {
    //     if (node->init.index() == 1) {
            // 初始化为单个整数
            // IntExp* init_exp = std::get<IntExp*>(node->init);
            // init_exp->accept(*this);
        // } else {
            // 初始化为数组
            // vector<IntExp*>* init_array = std::get<vector<IntExp*>*>(node->init);
            // for (auto& exp : *init_array) {
            //     exp->accept(*this);
            // }
    //     }
    // }
}


void AST_Semant_Visitor::visit(MethodDecl* node) {
#ifdef DEBUG
    cerr << "MethodDecl" << endl;
#endif
    current_method = node->id->id;
    // node->type->accept(*this);
    // node->id->accept(*this);
    // 遍历方法中的形参和变量声明
    if (node->fl) {
        for (auto* formal : *node->fl) {
            formal->accept(*this);
        }
    }
    if (node->vdl) {
        for (auto* var_decl : *node->vdl) {
            var_decl->accept(*this);
        }
    }
    if (node->sl) {
        for (auto* stm : *node->sl) {
            stm->accept(*this);
        }
    }
    current_method.clear();
}

void AST_Semant_Visitor::visit(Formal* node) {
#ifdef DEBUG
    cerr << "Formal" << endl;
#endif
    // node->type->accept(*this); // 分析类型
    // AST_Semant* type_semant = semant_map->getSemant(node->type);
    // AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, type_semant->get_type(), type_semant->get_type_par(), true);
    // semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(Nested* node) {
#ifdef DEBUG
    cerr << "Nested" << endl;
#endif
    if (node->sl) {
        for (auto* stm : *node->sl) {
            stm->accept(*this);
        }
    }
}

void AST_Semant_Visitor::visit(If* node) {
    #ifdef DEBUG
    cerr << "If" << endl;
#endif
    node->exp->accept(*this);
    node->stm1->accept(*this);
    if (node->stm2) {
        node->stm2->accept(*this);
    }
    AST_Semant* exp_semant = semant_map->getSemant(node->exp);
    if(!exp_semant){
        cerr << "Error: Parameter not defined!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (exp_semant->get_type() != TypeKind::INT) {
        cerr << "Error: Condition in if statement must be of type int" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
}

void AST_Semant_Visitor::visit(While* node) {
    #ifdef DEBUG
    cerr << "While" << endl;
#endif
    node->exp->accept(*this); // 分析条件表达式

    AST_Semant* exp_semant = semant_map->getSemant(node->exp);
    if(!exp_semant){
        cerr << "Error: Parameter not defined!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (exp_semant->get_type() != TypeKind::INT) {
        cerr << "Error: Condition in while loop must be of type int" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    in_a_while_loop++; // 进入 while 循环
    if (node->stm) {
        node->stm->accept(*this);
    }
    in_a_while_loop--; // 离开 while 循环
}

void AST_Semant_Visitor::visit(Assign* node) {
    #ifdef DEBUG
    cerr << "Assign" << endl;
#endif
    node->left->accept(*this);
    node->exp->accept(*this);
    AST_Semant* left_semant = semant_map->getSemant(node->left);
    AST_Semant* right_semant = semant_map->getSemant(node->exp);
    if(!left_semant){
        cerr << "Error: Left-hand side of assignment not defined!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if(!right_semant){
        cerr << "Error: Right-hand side of assignment not defined!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    if (!left_semant->is_lvalue()) {
        cerr << "Error: Left-hand side of assignment is not an lvalue" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    TypeKind ltype = left_semant->get_type();
    TypeKind rtype = right_semant->get_type();
    if (ltype != rtype) {
        cerr << "Error: Type mismatch in assignment" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    // 如果都是类，检查继承关系
    if (ltype == TypeKind::CLASS){
        string lclass = std::get<string>(left_semant->get_type_par());
        string rclass = std::get<string>(right_semant->get_type_par());
        set<string> ancestors = name_maps->get_ancestors_ast(rclass);
        ancestors.insert(rclass);
        if (ancestors.find(lclass) == ancestors.end()) {
            cerr << "Error: inheritance relationship violated in assignment" << "\tPos: " << node->getPos()->print() << endl;
            cerr << "exit program ..." << endl;
            exit(1);
        }
    } else if (ltype == TypeKind::ARRAY) {
        // int lsize = std::get<int>(left_semant->get_type_par());
        // int rsize = std::get<int>(right_semant->get_type_par());
        // #ifdef DEBUG
        // cout << "in Array Assignment: lsize: " << lsize << " rsize: " << rsize << endl;
        // #endif
        // // 将右值size赋值给左值 
        // AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::ARRAY, right_semant->get_type_par(), true);
        // semant_map->setSemant(node->left, semant);
        // // 还要在namemap中更新长度
        // // 转成idexp
        // if(node->left->getASTKind() != ASTKind::IdExp){
        //     cerr << "Error: Array Assignment: left value is not an id" << endl;
        //     cerr << "exit program ..." << endl;
        //     exit(1);
        // }
        // IdExp* idExp = dynamic_cast<IdExp*>(node->left);
        // string var_name = idExp->id;
        // // cout << var_name << endl;
        // if(name_maps->is_method_var(current_class, current_method, var_name)){
        //     name_maps->get_method_var(current_class, current_method, var_name)->type->arity->val = rsize;
        // }else if(name_maps->is_class_var(current_class, var_name)){
        //     name_maps->get_class_var(current_class, var_name)->type->arity->val = rsize;
        // }else if(name_maps->is_method_formal(current_class, current_method, var_name)){
        //     name_maps->get_method_formal(current_class, current_method, var_name)->type->arity->val = rsize;
        // }else{
        //     cerr << "Error: Array Assignment: left value not found in class var, formal var or method var" << endl;
        //     cerr << "exit program ..." << endl;
        //     exit(1);
        // }
    }
}

void AST_Semant_Visitor::visit(CallStm* node) {
#ifdef DEBUG
    cerr << "CallStm" << endl;
#endif
    node->obj->accept(*this);
    node->name->accept(*this);
    if (node->par){
        for (auto* arg : *node->par) {
            arg->accept(*this);
        }
    }
    AST_Semant* obj_semant = semant_map->getSemant(node->obj);
    if(!obj_semant){
        cerr << "Error: Object not defined in CallStm!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (obj_semant->get_kind() != AST_Semant::Kind::Value) {
        cerr << "Error: CallStm object is not a value." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (obj_semant->get_type() != TypeKind::CLASS) {
        cerr << "Error: CallStm object is not a class instance." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    // 获取调用对象的类名
    string obj_class_name = std::get<string>(obj_semant->get_type_par());
    set<string> ancestors = name_maps->get_ancestors_ast(obj_class_name);
    ancestors.insert(obj_class_name);
    for(auto ancestor_class: ancestors){
        // 假设 name_maps 中有方法的签名信息
        if(name_maps->is_method(ancestor_class, node->name->id)) {
            vector<Formal*>* formalList = name_maps->get_method_formal_list_ast(ancestor_class, node->name->id);
            if (node->par && formalList->size() != node->par->size()) {
                cerr << "Error: Parameter count mismatch in method call." << "\tPos: " << node->getPos()->print() << endl;
                cerr << "exit program ..." << endl;
                exit(1);
            }
            // 检查参数类型是否匹配
            for(int i = 0; i < formalList->size(); i++){
                AST_Semant* param = semant_map->getSemant((*node->par)[i]);
                if(!param) {
                    cerr << "Error: Parameter not defined in CallStm!" << "\tPos: " << node->getPos()->print() << endl;
                    cerr << "exit program ..." << endl;
                    exit(1);
                }
                if((*formalList)[i]->type->typeKind != param->get_type()){
                    cerr << "Error: Parameter type mismatch in method call." << "\tPos: " << node->getPos()->print() << endl;
                    cerr << "exit program ..." << endl;
                    exit(1);
                }
            }

            variant<monostate, string, int> name_type_par;
            AST_Semant* name_semant = new AST_Semant(AST_Semant::Kind::MethodName, TypeKind::INT, name_type_par, false);
            semant_map->setSemant(node->name, name_semant);
            return;
        }
    }
    cerr << "Error: Method '" << node->name->id << "' not found in class '" << obj_class_name << "'." << "\tPos: " << node->getPos()->print() << endl;
    cerr << "exit program ..." << endl;
    exit(1);
}

void AST_Semant_Visitor::visit(Continue* node) {
    #ifdef DEBUG
    cerr << "Continue" << endl;
#endif
    if (in_a_while_loop == 0) {
        cerr << "Error: Continue statement not within a while loop" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
}

void AST_Semant_Visitor::visit(Break* node) {
    #ifdef DEBUG
    cerr << "Break" << endl;
#endif
    if (in_a_while_loop == 0) {
        cerr << "Error: Break statement not within a while loop" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
}

void AST_Semant_Visitor::visit(Return* node) {
    #ifdef DEBUG
    cerr << "Return" << endl;
#endif
    if (node->exp) {
        node->exp->accept(*this);
        AST_Semant* exp_semant = semant_map->getSemant(node->exp);
        if(!exp_semant){
            cerr << "Error: Return value not defined!" << "\tPos: " << node->getPos()->print() << endl;
            cerr << "exit program ..." << endl;
            exit(1);
        }
        // 做返回值类型检验
        TypeKind exp_type = exp_semant->get_type();
        Formal* return_formal = name_maps->get_method_formal(current_class, current_method, "_^return^_"+current_method);
        if(exp_type != return_formal->type->typeKind){
            cerr << "Error: Return type mismatch." << "\tPos: " << node->getPos()->print() << endl;
            cerr << "exit program ..." << endl;
            exit(1);
        }
        if(exp_type == TypeKind::CLASS){
            string lclass = return_formal->type->cid->id;
            string rclass = std::get<string>(exp_semant->get_type_par());
            set<string> ancestors = name_maps->get_ancestors_ast(rclass);
            ancestors.insert(rclass);
            if (ancestors.find(lclass) == ancestors.end()) {
                cerr << "Error: inheritance relationship violated in return statement" << "\tPos: " << node->getPos()->print() << endl;
                cerr << "exit program ..." << endl;
                exit(1);
            }
        }
        variant<monostate, string, int> type_par;
        AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::INT, type_par, false);
        semant_map->setSemant(node, semant);
    } else {
        cerr << "Error: None Return statement." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
}

void AST_Semant_Visitor::visit(PutInt* node) {
    #ifdef DEBUG
    cerr << "PutInt" << endl;
#endif
    node->exp->accept(*this); // 分析输出表达式
    AST_Semant* exp_semant = semant_map->getSemant(node->exp);
    if(!exp_semant){
        cerr << "Error: Parameter not defined in PutInt!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    if (exp_semant->get_type() != TypeKind::INT) {
        cerr << "Error: PutInt expression must be an integer." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
}

void AST_Semant_Visitor::visit(PutCh* node) {
    #ifdef DEBUG
    cerr << "PutCh" << endl;
#endif
    node->exp->accept(*this); // 分析输出表达式
    AST_Semant* exp_semant = semant_map->getSemant(node->exp);
    if(!exp_semant){
        cerr << "Error: Parameter not defined in PutCh!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    if (exp_semant->get_type() != TypeKind::INT) {
        cerr << "Error: PutCh expression must be an integer." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
}

void AST_Semant_Visitor::visit(PutArray* node) {
    #ifdef DEBUG
    cerr << "PutArray" << endl;
#endif
    node->n->accept(*this); // 分析输出大小
    node->arr->accept(*this); // 分析数组

    AST_Semant* n_semant = semant_map->getSemant(node->n);
    AST_Semant* arr_semant = semant_map->getSemant(node->arr);
    if(!n_semant){
        cerr << "Error: Parameter not defined in PutArray!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if(!arr_semant){
        cerr << "Error: Parameter not defined in PutArray!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    if (n_semant->get_type() != TypeKind::INT) {
        cerr << "Error: PutArray size must be an integer." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    if (arr_semant->get_type() != TypeKind::ARRAY) {
        cerr << "Error: PutArray expression must be an array." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
}

void AST_Semant_Visitor::visit(Starttime* node) {
    #ifdef DEBUG
    cerr << "Starttime" << endl;
#endif
    return;
}

void AST_Semant_Visitor::visit(Stoptime* node) {
    #ifdef DEBUG
    cerr << "Stoptime" << endl;
#endif
    return;
}

void AST_Semant_Visitor::visit(BinaryOp* node) { 
    #ifdef DEBUG
    cerr << "BinaryOp" << endl;
#endif
    node->left->accept(*this);
    node->right->accept(*this);
    AST_Semant* left_semant = semant_map->getSemant(node->left);
    AST_Semant* right_semant = semant_map->getSemant(node->right);
    if(!left_semant){
        cerr << "Error: Left-hand side of binary operation not defined!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if(!right_semant){
        cerr << "Error: Right-hand side of binary operation not defined!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    
    if (left_semant->get_type() != right_semant->get_type()) {
        cerr << "Error: Type mismatch in binary operation" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    if(left_semant->get_type() == TypeKind::CLASS){
        cerr << "Error: class type is not allowed in binary operation" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, left_semant->get_type(), left_semant->get_type_par(), false);
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(UnaryOp* node) {
    #ifdef DEBUG
    cerr << "UnaryOp" << endl;
#endif
    if (node->exp) {
        node->exp->accept(*this);
        AST_Semant* exp_semant = semant_map->getSemant(node->exp);
        if(!exp_semant){
            cerr << "Error: Parameter not defined in UnaryOp!" << "\tPos: " << node->getPos()->print() << endl;
            cerr << "exit program ..." << endl;
            exit(1);
        }
        if (exp_semant->get_type() == TypeKind::CLASS) {
            cerr << "Error: class type is not allowed in unary operation" << "\tPos: " << node->getPos()->print() << endl;
            cerr << "exit program ..." << endl;
            exit(1);
        }
        AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, exp_semant->get_type(), exp_semant->get_type_par(), false);
        semant_map->setSemant(node, semant);
    }
}

void AST_Semant_Visitor::visit(ArrayExp* node) {
    #ifdef DEBUG
    cerr << "ArrayExp" << endl;
#endif
    node->arr->accept(*this);
    node->index->accept(*this);
    AST_Semant* arr_semant = semant_map->getSemant(node->arr);
    AST_Semant* index_semant = semant_map->getSemant(node->index);
    if(!arr_semant){
        cerr << "Error: Array not defined in ArrayExp!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if(!index_semant){
        cerr << "Error: Index not defined in ArrayExp!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    if (arr_semant->get_type() != TypeKind::ARRAY) {
        cerr << "Error: ArrayExp base is not an array." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (index_semant->get_type() != TypeKind::INT){
        cerr << "Error: Array index is not an integer." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    variant<monostate, string, int> type_par;
    AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::INT, type_par, true);
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(CallExp* node) {
    #ifdef DEBUG
    cerr << "CallExp" << endl;
#endif
    node->obj->accept(*this);
    // cout << "***" << ASTKind_string(node->obj->getASTKind()) << endl;
    node->name->accept(*this);
    if (node->par) {
        for (auto* arg : *node->par) {
            arg->accept(*this);
        }
    }
    AST_Semant* obj_semant = semant_map->getSemant(node->obj);
    if(!obj_semant){
        cerr << "Error: Object not defined in CallExp!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (obj_semant->get_kind() != AST_Semant::Kind::Value) {
        cerr << "Error: CallExp object is not a value." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (obj_semant->get_type() != TypeKind::CLASS) {
        cerr << "Error: CallExp object is not a class instance." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    // 获取调用对象的类名
    string obj_class_name = std::get<string>(obj_semant->get_type_par());
    set<string> ancestors = name_maps->get_ancestors_ast(obj_class_name);
    ancestors.insert(obj_class_name);
    for(auto ancestor_class: ancestors){
        if(name_maps->is_method(ancestor_class, node->name->id)) {
            vector<Formal*>* formalList = name_maps->get_method_formal_list_ast(ancestor_class, node->name->id);
            if (node->par && formalList->size() != node->par->size()) {
                cerr << "Error: Parameter count mismatch in method call." << "\tPos: " << node->getPos()->print() << endl;
                cerr << "exit program ..." << endl;
                exit(1);
            }
            // 检查参数类型是否匹配
            for(int i = 0; i < formalList->size(); i++){
                AST_Semant* param = semant_map->getSemant((*node->par)[i]);
                if(!param) {
                    cerr << "Error: Parameter not defined in CallExp!" << "\tPos: " << node->getPos()->print() << endl;
                    cerr << "exit program ..." << endl;
                    exit(1);
                }
                if((*formalList)[i]->type->typeKind != param->get_type()){
                    cerr << "Error: Parameter type mismatch in method call." << "\tPos: " << node->getPos()->print() << endl;
                    cerr << "exit program ..." << endl;
                    exit(1);
                }
            }

            variant<monostate, string, int> name_type_par;
            AST_Semant* name_semant = new AST_Semant(AST_Semant::Kind::MethodName, TypeKind::INT, name_type_par, false);
            semant_map->setSemant(node->name, name_semant);

            // 比callStm多的部分 设置语义值
            Formal* return_formal = name_maps->get_method_formal(ancestor_class, node->name->id, "_^return^_"+node->name->id);
            if(return_formal->type->typeKind == TypeKind::CLASS){
                variant<monostate, string, int> type_par;
                type_par = return_formal->type->cid->id;
                AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::CLASS, type_par, false);
                semant_map->setSemant(node, semant);
            }else if(return_formal->type->typeKind == TypeKind::ARRAY){
                variant<monostate, string, int> type_par;
                type_par = return_formal->type->arity->val;
                AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::ARRAY, type_par, false);
                semant_map->setSemant(node, semant);
            }else{
                variant<monostate, string, int> type_par;
                AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::INT, type_par, false);
                semant_map->setSemant(node, semant);
            }
            return;
        }
    }
    cerr << "Error: Method '" << node->name->id << "' not found in class '" << obj_class_name << "'." << "\tPos: " << node->getPos()->print() << endl;
    cerr << "exit program ..." << endl;
    exit(1);
}

void AST_Semant_Visitor::visit(ClassVar* node) {
    #ifdef DEBUG
    cerr << "ClassVar" << endl;  // 类变量
#endif   
    node->obj->accept(*this); // 分析对象
    node->id->accept(*this);
    AST_Semant* obj_semant = semant_map->getSemant(node->obj);
    if(!obj_semant){
        cerr << "Error: Object not defined in ClassVar!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (obj_semant->get_kind() != AST_Semant::Kind::Value) {
        cerr << "Error: ClassVar object is not a value." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (obj_semant->get_type() != TypeKind::CLASS) {
        cerr << "Error: ClassVar object is not a class instance." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    // 检查类变量是否存在
    string obj_class_name = std::get<string>(obj_semant->get_type_par());
    set<string> ancestors = name_maps->get_ancestors_ast(obj_class_name);
    ancestors.insert(obj_class_name);
    AST_Semant* semant = nullptr;
    for(auto ancestor_class: ancestors){
        if(name_maps->is_class_var(ancestor_class, node->id->id)){
            VarDecl* vdl = name_maps->get_class_var(ancestor_class, node->id->id);
            // 类变量访问的结果类型为变量的类型
            TypeKind typeKind = vdl->type->typeKind;
            variant<monostate, string, int> type_par;
            if (typeKind == TypeKind::ARRAY) {
                type_par = vdl->type->arity->val;
            } else if (typeKind == TypeKind::CLASS) {
                type_par = vdl->type->cid->id;
            }
            semant = new AST_Semant(AST_Semant::Kind::Value, typeKind, type_par, true);
        }
    }
    if(semant) semant_map->setSemant(node, semant);
    else {
        cerr << "Error: Class variable '" << node->id->id << "' not found in class " << obj_class_name << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
}

void AST_Semant_Visitor::visit(BoolExp* node) {
    #ifdef DEBUG
    cerr << "BoolExp" << endl;
#endif
    variant<monostate, string, int> type_par;
    AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::INT, type_par, false);
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(This* node) {
    #ifdef DEBUG
    cerr << "This" << endl;
#endif
    variant<monostate, string, int> type_par;
    type_par = current_class;
    AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::CLASS, type_par, false);
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(Length* node) {
    #ifdef DEBUG
    cerr << "Length" << endl;
#endif
    node->exp->accept(*this);
    AST_Semant* exp_semant = semant_map->getSemant(node->exp);
    if(!exp_semant){
        cerr << "Error: Parameter not defined in Length!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (exp_semant->get_type() != TypeKind::ARRAY) {
        cerr << "Error: Length expression must be an array." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    variant<monostate, string, int> type_par;
    AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::INT, type_par, false);
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(Esc* node) {
    #ifdef DEBUG
    cerr << "Esc" << endl;
#endif
    if (node->sl) {
        for (auto* stm : *node->sl) {
            stm->accept(*this);
        }
    }
    node->exp->accept(*this);
    AST_Semant* exp_semant = semant_map->getSemant(node->exp);
    if(!exp_semant){
        cerr << "Error: Parameter not defined in Esc!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    AST_Semant* semant = new AST_Semant(exp_semant->get_kind(), exp_semant->get_type(), exp_semant->get_type_par(), exp_semant->is_lvalue());
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(GetInt* node) {
    #ifdef DEBUG
    cerr << "GetInt" << endl;
#endif
    variant<monostate, string, int> type_par;
    AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::INT, type_par, false);
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(GetCh* node) {
    #ifdef DEBUG
    cerr << "GetCh" << endl;
#endif
    variant<monostate, string, int> type_par;
    AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::INT, type_par, false);
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(GetArray* node) {
    #ifdef DEBUG
    cerr << "GetArray" << endl;
#endif
    node->exp->accept(*this);
    AST_Semant* exp_semant = semant_map->getSemant(node->exp);
    if(!exp_semant){
        cerr << "Error: Parameter not defined in GetArray!" << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
    if (exp_semant->get_type() != TypeKind::ARRAY) {
        cerr << "Error: GetArray param must be an array." << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    variant<monostate, string, int> type_par; 
    AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::INT, type_par, false);
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(IdExp* node) {
#ifdef DEBUG
    cerr << "IdExp" << endl;
#endif
    string var = node->id;
    variant<monostate, string, int> type_par;
    set<string> ancestors = name_maps->get_ancestors_ast(current_class);
    ancestors.insert(current_class);
    AST_Semant* semant = nullptr;
    for(auto ancestor_class: ancestors){
        #ifdef DEBUG
        cout << "ancestor class: " << ancestor_class << endl;
        #endif
        if (name_maps->is_class_var(ancestor_class, var)) {
            #ifdef DEBUG
            cout << "class var!\n";
            #endif
            VarDecl* vdl = name_maps->get_class_var(ancestor_class, var);
            TypeKind typeKind = vdl->type->typeKind;
            if (typeKind == TypeKind::ARRAY) {
                type_par = vdl->type->arity->val;
            } else if (typeKind == TypeKind::CLASS) {
                type_par = vdl->type->cid->id;
            }
            semant = new AST_Semant(AST_Semant::Kind::Value, typeKind, type_par, true);
        } else if (name_maps->is_method_var(ancestor_class, current_method, var)) {
            #ifdef DEBUG
                cout << "method var!\n";
            #endif
            VarDecl* vdl = name_maps->get_method_var(ancestor_class, current_method, var);
            TypeKind typeKind = vdl->type->typeKind;
            if (typeKind == TypeKind::ARRAY) {
                type_par = vdl->type->arity->val;
            } else if (typeKind == TypeKind::CLASS) {
                type_par = vdl->type->cid->id;
            }
            semant = new AST_Semant(AST_Semant::Kind::Value, typeKind, type_par, true);
        } else if(name_maps->is_method_formal(ancestor_class, current_method, var)){
            #ifdef DEBUG
            cout << "method formal var!\n";
            #endif
            Formal* formal_info = name_maps->get_method_formal(ancestor_class, current_method, var);
            TypeKind typeKind = formal_info->type->typeKind;
            if (typeKind == TypeKind::ARRAY) {
                type_par = formal_info->type->arity->val;
            } else if (typeKind == TypeKind::CLASS) {
                type_par = formal_info->type->cid->id;
            }
            semant = new AST_Semant(AST_Semant::Kind::Value, typeKind, type_par, true);
        } 
        if(semant) {
            semant_map->setSemant(node, semant);
            break;
        }
    }
}

void AST_Semant_Visitor::visit(IntExp* node) {
    #ifdef DEBUG
    cerr << "IntExp" << endl;
#endif
    variant<monostate, string, int> type_par;
    AST_Semant* semant = new AST_Semant(AST_Semant::Kind::Value, TypeKind::INT, type_par, false);
    semant_map->setSemant(node, semant);
}

void AST_Semant_Visitor::visit(OpExp* node) {
    #ifdef DEBUG
    cerr << "OpExp" << endl;
#endif
    return;
}

// rest of the code