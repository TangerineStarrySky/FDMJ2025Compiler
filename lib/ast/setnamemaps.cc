#define DEBUG
#undef DEBUG

#include <iostream>
#include <variant>
#include <map>
#include <vector>
#include <algorithm>
#include "ASTheader.hh"
#include "FDMJAST.hh"
#include "namemaps.hh"

using namespace std;
using namespace fdmj;

// TODO

void AST_Name_Map_Visitor::visit(Program* node) {
    if (node == nullptr) {
        return;
    }
    if (node->main) {
        // 添加 MainClass
        name_maps->add_class("MainClass");
        name_maps->add_method("MainClass", "main");
        Formal* return_formal = new Formal(nullptr, new Type(nullptr), new IdExp(nullptr, "return_value"));
        name_maps->add_method_formal("MainClass", "main", "return_value", return_formal);
        name_maps->add_method_formal_list("MainClass", "main", vector<string>());
        current_class = "MainClass";
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

void AST_Name_Map_Visitor::visit(MainMethod* node) {
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

void AST_Name_Map_Visitor::visit(ClassDecl* node) {
    // 添加类名
    string class_name = node->id->id;
    if (!name_maps->add_class(class_name)) {
        cerr << "Error: Duplicate class name: " << class_name << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    current_class = class_name;
    // 添加类继承关系
    if (node->eid) {
        string parent_name = node->eid->id;
        if (!name_maps->add_class_hiearchy(current_class, parent_name)) {
            cerr << "Error: Class hiearchy loop detected for class: " << current_class << "\tPos: " << node->getPos()->print() << endl;
            cerr << "exit program ..." << endl;
            exit(1);
        }
    }

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

void AST_Name_Map_Visitor::visit(Type* node) {
    return;
}

void AST_Name_Map_Visitor::visit(VarDecl* node) {
    string var_name = node->id->id;
    if (current_method.empty()) {
        // 类变量
        // 不能重复声明父类同名变量
        set<string> ancestors = name_maps->get_ancestors(current_class);
        for (auto ancestor : ancestors) {
            if (name_maps->is_class_var(ancestor, var_name)) {
                cerr << "Error: Duplicate class variable with ancestors: " << var_name << " in class " << current_class << "\tPos: " << node->getPos()->print() << endl;
                cerr << "exit program ..." << endl;
                exit(1);
            }
        }
        if (!name_maps->add_class_var(current_class, var_name, node)) {
            cerr << "Error: Duplicate class variable: " << var_name << " in class " << current_class << "\tPos: " << node->getPos()->print() << endl;
            cerr << "exit program ..." << endl;
            exit(1);
        }
    } else {
        // 方法变量
        if (!name_maps->add_method_var(current_class, current_method, var_name, node)) {
            cerr << "Error: Duplicate method variable: " << var_name << " in method " << current_method << "\tPos: " << node->getPos()->print() << endl;
            cerr << "exit program ..." << endl;
            exit(1);
        }
    }
}

void AST_Name_Map_Visitor::visit(MethodDecl* node) {
    // 添加方法名
    string method_name = node->id->id;

    // 检查和父类方法重定义 是否违反签名
    set<string> ancestors = name_maps->get_ancestors(current_class);
    for (auto ancestor : ancestors) {
        if (name_maps->is_method(ancestor, method_name)) {
            vector<Formal*>* ancestor_fl = name_maps->get_method_formal_list(ancestor, method_name);
            Formal* return_formal = name_maps->get_method_formal(ancestor, method_name, "return_value");
            if (node->fl) {
                // 检查参数数量
                if (node->fl->size() != ancestor_fl->size()) {
                    cerr << "Error: Method formal number mismatch: " << method_name << " in class " << current_class << "\tPos: " << node->getPos()->print() << endl;
                    cerr << "exit program ..." << endl;
                    exit(1);
                }
                // 检查返回值类型
                if (node->type->typeKind != return_formal->type->typeKind) {
                    cerr << "Error: Method return type mismatch: " << method_name << " in class " << current_class << "\tPos: " << node->getPos()->print() << endl;
                    cerr << "exit program ..." << endl;
                    exit(1);
                }
                // 如果是类 检查是否是同一个类
                if (node->type->typeKind == TypeKind::CLASS) {
                    if (node->type->cid->id != return_formal->type->cid->id) {
                        cerr << "Error: Method return class type mismatch: " << method_name << " in class " << current_class << "\tPos: " << node->getPos()->print() << endl;
                        cerr << "exit program ..." << endl;
                        exit(1);
                    }
                }
                // 依次检查参数类型
                for (int i = 0; i < node->fl->size(); i++) {
                    if ((*node->fl)[i]->type->typeKind != (*ancestor_fl)[i]->type->typeKind) {
                        cerr << "Error: Method formal type mismatch: " << method_name << " in class " << current_class << "\tPos: " << node->getPos()->print() << endl;
                        cerr << "exit program ..." << endl;
                        exit(1);
                    }
                    // 如果是类 检查是否是同一个类
                    if ((*node->fl)[i]->type->typeKind == TypeKind::CLASS) {
                        if ((*node->fl)[i]->type->cid->id != (*ancestor_fl)[i]->type->cid->id) {
                            cerr << "Error: Method formal class type mismatch: " << method_name << " in class " << current_class << "\tPos: " << node->getPos()->print() << endl;
                            cerr << "exit program ..." << endl;
                            exit(1);
                        }
                    }
                }
            }
            break; // 至此，允许重载
        }
    }

    if (!name_maps->add_method(current_class, method_name)) {
        cerr << "Error: Duplicate method name: " << method_name << " in class " << current_class << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }

    // 设置当前正在访问的方法名
    current_method = method_name;

    // 遍历方法中的形参和变量声明
    if (node->fl) {
        vector<string> vl;
        for (auto* formal : *node->fl) {
            vl.push_back(formal->id->id);
            formal->accept(*this);
        }
        name_maps->add_method_formal_list(current_class, current_method, vl);
        // 添加返回值在参数列表里，但不加在formal_list中 3.18
        Formal* return_formal = new Formal(nullptr, node->type, new IdExp(nullptr, "return_value"));
        name_maps->add_method_formal(current_class, current_method, "return_value", return_formal);
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
    // 重置当前正在访问的方法名
    current_method.clear();
}

void AST_Name_Map_Visitor::visit(Formal* node) {
    string formal_name = node->id->id;
    if (!name_maps->add_method_formal(current_class, current_method, formal_name, node)) {
        cerr << "Error: Duplicate formal name: " << formal_name << " in method " << current_method << "\tPos: " << node->getPos()->print() << endl;
        cerr << "exit program ..." << endl;
        exit(1);
    }
}

void AST_Name_Map_Visitor::visit(Nested* node) {
    if (node->sl) {
        for (auto* stm : *node->sl) {
            stm->accept(*this);
        }
    }
}

void AST_Name_Map_Visitor::visit(If* node) {
    if (node->exp) node->exp->accept(*this);
    if (node->stm1) node->stm1->accept(*this);
    if (node->stm2) node->stm2->accept(*this);
}

void AST_Name_Map_Visitor::visit(While* node) {
    if (node->exp) node->exp->accept(*this);
    if (node->stm) node->stm->accept(*this);
}

void AST_Name_Map_Visitor::visit(Assign* node) {
    if (node->left) node->left->accept(*this);
    if (node->exp) node->exp->accept(*this);
}

void AST_Name_Map_Visitor::visit(CallStm* node) {
    if (node->obj) node->obj->accept(*this);
    if (node->par) {
        for (auto* arg : *node->par) {
            arg->accept(*this);
        }
    }
}

void AST_Name_Map_Visitor::visit(Continue* node) {
    return;
}

void AST_Name_Map_Visitor::visit(Break* node) {
    return;
}

void AST_Name_Map_Visitor::visit(Return* node) {
    if (node->exp) node->exp->accept(*this);
}

void AST_Name_Map_Visitor::visit(PutInt* node) {
    if (node->exp) node->exp->accept(*this);
}

void AST_Name_Map_Visitor::visit(PutCh* node) {
    if (node->exp) node->exp->accept(*this);
}

void AST_Name_Map_Visitor::visit(PutArray* node) {
    if (node->n) node->n->accept(*this);
    if (node->arr) node->arr->accept(*this);
}

void AST_Name_Map_Visitor::visit(Starttime* node) {
    return;
}

void AST_Name_Map_Visitor::visit(Stoptime* node) {
    return;
}

void AST_Name_Map_Visitor::visit(BinaryOp* node) {
    if (node->left) node->left->accept(*this);
    if (node->right) node->right->accept(*this);
}

void AST_Name_Map_Visitor::visit(UnaryOp* node) {
    if (node->exp) node->exp->accept(*this);
}

void AST_Name_Map_Visitor::visit(ArrayExp* node) {
    if (node->arr) node->arr->accept(*this);
    if (node->index) node->index->accept(*this);
}

void AST_Name_Map_Visitor::visit(CallExp* node) {
    if (node->obj) node->obj->accept(*this);
    if (node->par) {
        for (auto* arg : *node->par) {
            arg->accept(*this);
        }
    }
}

void AST_Name_Map_Visitor::visit(ClassVar* node) {
    if (node->obj) node->obj->accept(*this);
}

void AST_Name_Map_Visitor::visit(BoolExp* node) {
    return;
}

void AST_Name_Map_Visitor::visit(This* node) {
    return;
}

void AST_Name_Map_Visitor::visit(Length* node) {
    if (node->exp) node->exp->accept(*this);
}

void AST_Name_Map_Visitor::visit(Esc* node) {
    if (node->exp) node->exp->accept(*this);
    if (node->sl) {
        for (auto* stm : *node->sl) {
            stm->accept(*this);
        }
    }
}

void AST_Name_Map_Visitor::visit(GetInt* node) {
    return;
}

void AST_Name_Map_Visitor::visit(GetCh* node) {
    return;
}

void AST_Name_Map_Visitor::visit(GetArray* node) {
    if (node->exp) node->exp->accept(*this);
}

void AST_Name_Map_Visitor::visit(IdExp* node) {
    return;
}

void AST_Name_Map_Visitor::visit(IntExp* node) {
    return;
}

void AST_Name_Map_Visitor::visit(OpExp* node) {
    return;
}

//rest of the visit functions here
