#define DEBUG
#undef DEBUG

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include "config.hh"
#include "ASTheader.hh"
#include "FDMJAST.hh"
#include "treep.hh"
#include "temp.hh"
#include "ast2tree.hh"
#include <cassert>

using namespace std;
//using namespace fdmj;
//using namespace tree;


tree::Program* generate_a_testIR_ast2tree(); //forward decl

// you need to code this function!
tree::Program* ast2tree(fdmj::Program* prog, AST_Semant_Map* semant_map) {
    // return generate_a_testIR_ast2tree();
    ASTToTreeVisitor visitor;
    visitor.semant_map = semant_map;
    visitor.nm = semant_map->getNameMaps();
    prog->accept(visitor);
    return dynamic_cast<tree::Program*>(visitor.getTree());
}

//this is only for a test
tree::Program* generate_a_testIR_ast2tree() {
    Temp_map *tm = new Temp_map();
    tree::Label *entry_label = tm->newlabel();
    tree::LabelStm *label_stm = new tree::LabelStm(entry_label);
    tree::Move *move = new tree::Move(new tree::TempExp(tree::Type::INT, tm->newtemp()), new tree::Const(1));

    vector<tree::Stm*> *sl1 = new vector<tree::Stm*>(); sl1->push_back(move);

    vector<tree::Exp*> *args = new vector<tree::Exp*>();
    tree::Eseq *eseq = new tree::Eseq(tree::Type::INT, new tree::Seq(sl1), new tree::Const(19));
    args->push_back(eseq);

    tree::ExtCall *call = new tree::ExtCall(tree::Type::INT, "putchar", args);
    tree::Return *ret = new tree::Return(new tree::Const(100));
    vector<tree::Stm*> *sl = new vector<tree::Stm*>();
    sl->push_back(label_stm);
    sl->push_back(new tree::ExpStm(call));
    sl->push_back(ret);
    tree::Block *block = new tree::Block(entry_label, nullptr, sl);
    vector<tree::Block*> *bl = new vector<tree::Block*>();
    bl->push_back(block);
    tree::FuncDecl *fd = new tree::FuncDecl("_^main^_main", nullptr, bl, tree::Type::INT, 100, 100);
    vector<tree::FuncDecl*> *fdl = new vector<tree::FuncDecl*>();
    fdl->push_back(fd);
    return new tree::Program(fdl);
}

Class_table* generate_class_table(AST_Semant_Map* semant_map) {
    Class_table* class_table = new Class_table();
    Name_Maps* nm = semant_map->getNameMaps();
    int offset = 0;
    // 遍历所有类
    for (string class_name : *nm->get_class_list()) {
        // 遍历类的变量
        set<string>* class_vars = nm->get_class_var_list(class_name);
        for (string var_ : *class_vars) {
            fdmj::VarDecl* var_decl = nm->get_class_var(class_name, var_);
            if (var_decl) {
                if(class_table->var_pos_map.find(var_) == class_table->var_pos_map.end()){
                    class_table->var_pos_map[var_] = offset * Compiler_Config::get("int_length");
                    offset++;
                }
            }
        }
    }
    for (string class_name : *nm->get_class_list()) {
        if(class_name == "_^main^_")
            continue;
        // 遍历类的方法
        set<string>* method_list = nm->get_method_list(class_name);
        for (string method : *method_list) {
            // 非重载函数才会记录
            if(class_table->method_pos_map.find(method) == class_table->method_pos_map.end()){
                class_table->method_pos_map[method] = offset * Compiler_Config::get("int_length");
                offset++;
            }
        }
    }
    // 最后添加main函数
    class_table->method_pos_map["main"] = offset * Compiler_Config::get("int_length");

    return class_table;
}

Method_var_table* generate_method_var_table(string class_name, string method_name, 
                                          Name_Maps* nm, Temp_map* tm) {
    Method_var_table* mvt = new Method_var_table();
    
    // Get method information from name maps
    vector<string>* formals = nm->get_method_formal_list(class_name, method_name);
    string return_var_name = "_^return^_" + method_name;

    // Add This value
    if(class_name != "_^main^_"){
        mvt->var_temp_map->emplace("_^THIS^_", tm->newtemp());
        mvt->var_type_map->emplace("_^THIS^_", tree::Type::PTR);
    }
    
    // Process formal parameters
    for (auto formal_name : *formals) {
        if(formal_name == return_var_name) {
            continue;
        }
        tree::Temp* temp = tm->newtemp();
        mvt->var_temp_map->emplace(formal_name, temp);

        fdmj::Formal* f = nm->get_method_formal(class_name, method_name, formal_name);
        if(f->type->typeKind == fdmj::TypeKind::INT) {
            mvt->var_type_map->emplace(formal_name, tree::Type::INT);
        } else {
            mvt->var_type_map->emplace(formal_name, tree::Type::PTR);
        } 
    }

    // Process local variables(overriding formals if same name)
    set<string>* locals = nm->get_method_var_list(class_name, method_name);
    for (auto local : *locals) {
        tree::Temp* temp = tm->newtemp();
        mvt->var_temp_map->emplace(local, temp);

        // Will override formal if same name exists
        fdmj::VarDecl* vdl = nm->get_method_var(class_name, method_name, local);
        if(vdl->type->typeKind == fdmj::TypeKind::INT) {
            mvt->var_type_map->emplace(local, tree::Type::INT);
        } else {
            mvt->var_type_map->emplace(local, tree::Type::PTR);
        }
    }

    // Add return value temp
    tree::Temp* return_temp = tm->newtemp();
    mvt->var_temp_map->emplace(return_var_name, return_temp);
    fdmj::Formal* return_formal = nm->get_method_formal(class_name, method_name, return_var_name);
    if(return_formal->type->typeKind == fdmj::TypeKind::INT) {
        mvt->var_type_map->emplace(return_var_name, tree::Type::INT);
    } else {
        mvt->var_type_map->emplace(return_var_name, tree::Type::PTR);
    }
    
    return mvt;
}

// Visitor implementation
void ASTToTreeVisitor::visit(fdmj::Program* node) {
    #ifdef DEBUG
    cout << "Visiting Program" << endl;
    #endif
    class_table = generate_class_table(semant_map);
    // class_table->print();

    vector<tree::FuncDecl*>* fdl = new vector<tree::FuncDecl*>();
    // Visit main method
    current_class = "_^main^_";
    current_method = "main";
    node->main->accept(*this);
    current_class.clear();
    current_method.clear();

    tree::FuncDecl *fd = dynamic_cast<tree::FuncDecl*>(visit_tree_result);
    fdl->push_back(fd);

    // Visit all methods in the class declarations
    if(node->cdl){
        for (auto cls : *node->cdl) {
            current_class = cls->id->id;
            for (auto method : *cls->mdl) {
                current_method = method->id->id;
                method->accept(*this);
                fdl->push_back(dynamic_cast<tree::FuncDecl*>(visit_tree_result));
                current_method.clear();
            }
            current_class.clear();
        }   
    } 

    visit_tree_result = new tree::Program(fdl);
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::MainMethod* node) {
    #ifdef DEBUG
    cout << "Visiting MainMethod" << endl;
    #endif
    tm = new Temp_map();
    mvt = generate_method_var_table(current_class, current_method, nm, tm);
    string func_name = current_class + "^" + current_method;
    // mvt->print(func_name);
    vector<tree::Temp*>* args = new vector<tree::Temp*>();
    vector<tree::Block*>* blocks = new vector<tree::Block*>();

    // Create entry and exit labels
    vector<tree::Label*>* exit_labels = new vector<tree::Label*>();
    
    // Process statements
    vector<tree::Stm*>* stmts = new vector<tree::Stm*>();

    set<string>* var_list = nm->get_method_var_list(current_class, current_method);
    for(auto var : *var_list) {
        VarDecl* var_decl = nm->get_method_var(current_class, current_method, var);
        var_decl->accept(*this);
        if(visit_tree_result != nullptr) {
            if(visit_tree_result->getTreeKind() == tree::Kind::SEQ) {
                vector<tree::Stm*>* seq_stmts = dynamic_cast<tree::Seq*>(visit_tree_result)->sl;
                for(auto stmt : *seq_stmts) {
                    stmts->push_back(stmt);
                }
            } else {
                stmts->push_back(dynamic_cast<tree::Stm*>(visit_tree_result));
            }
        }
    }

    for (auto stmt : *node->sl) {
        stmt->accept(*this);
        stmts->push_back(dynamic_cast<tree::Stm*>(visit_tree_result));
    }

    // Add entry label in the front
    tree::Label* entry_label = tm->newlabel();
    stmts->insert(stmts->begin(), new tree::LabelStm(entry_label));
    
    // Create block
    tree::Block* block = new tree::Block(entry_label, exit_labels, stmts);
    blocks->push_back(block);
    
    visit_tree_result = new tree::FuncDecl(func_name, args, blocks, tree::Type::INT, 
                                         tm->next_temp-1, tm->next_label-1);
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::ClassDecl* node) {
    #ifdef DEBUG
    cout << "Visiting ClassDecl" << endl;
    #endif
}

void ASTToTreeVisitor::visit(fdmj::MethodDecl* node) {
    #ifdef DEBUG
    cout << "Visiting MethodDecl" << endl;
    #endif
    tm = new Temp_map();
    mvt = generate_method_var_table(current_class, current_method, nm, tm); 
    string func_name = current_class + "^" + current_method;
    // mvt->print(func_name);

    // 添加参数
    vector<tree::Temp*>* args = new vector<tree::Temp*>();
    args->push_back(mvt->get_var_temp("_^THIS^_"));
    if(node->fl){
        for(auto formal : *node->fl){
            args->push_back(mvt->get_var_temp(formal->id->id));
        }
    }
    vector<tree::Block*>* blocks = new vector<tree::Block*>();
    vector<tree::Label*>* exit_labels = new vector<tree::Label*>();
    
    // Process statements
    vector<tree::Stm*>* stmts = new vector<tree::Stm*>();
    for(auto var_decl : *node->vdl) {
        var_decl->accept(*this);
        if(visit_tree_result != nullptr) {
            if(visit_tree_result->getTreeKind() == tree::Kind::SEQ) {
                vector<tree::Stm*>* seq_stmts = dynamic_cast<tree::Seq*>(visit_tree_result)->sl;
                for(auto stmt : *seq_stmts) {
                    stmts->push_back(stmt);
                }
            } else {
                stmts->push_back(dynamic_cast<tree::Stm*>(visit_tree_result));
            }
        }
    }

    for (auto stmt : *node->sl) {
        stmt->accept(*this);
        stmts->push_back(dynamic_cast<tree::Stm*>(visit_tree_result));
    }

    // Add entry label in the front
    tree::Label* entry_label = tm->newlabel();
    stmts->insert(stmts->begin(), new tree::LabelStm(entry_label));
    
    // Create block
    tree::Block* block = new tree::Block(entry_label, exit_labels, stmts);
    blocks->push_back(block);

    tree::Type return_type = (node->type->typeKind == fdmj::TypeKind::INT) ? tree::Type::INT : tree::Type::PTR;
    
    visit_tree_result = new tree::FuncDecl(func_name, args, blocks, return_type, 
                                         tm->next_temp-1, tm->next_label-1);
    visit_exp_result = nullptr; 
}

void ASTToTreeVisitor::visit(fdmj::Formal* node) {
    #ifdef DEBUG
    cout << "Visiting Formal" << endl;
    #endif
}

void ASTToTreeVisitor::visit(fdmj::VarDecl* node) {
    #ifdef DEBUG
    cout << "Visiting VarDecl" << endl;
    #endif
    if(node->type->typeKind == fdmj::TypeKind::INT) {
        // cout << "vardecl int" << endl;
        if(node->init.index() == 1) {
            IntExp* init_exp = std::get<IntExp*>(node->init);
            init_exp->accept(*this);
            tree::Exp* vd = visit_exp_result->unEx(tm)->exp;
            node->id->accept(*this);
            tree::Exp* tmp;
            if (visit_exp_result){
                tmp = visit_exp_result->unEx(tm)->exp;
            }else{
                tm->newtemp(); // test
                tmp = new tree::TempExp(tree::Type::INT, tm->newtemp());
                last_tmp = tmp;
            }
            tree::Stm* move_stm = new tree::Move(tmp, vd);
            visit_tree_result = new tree::Seq(new std::vector<tree::Stm*>{move_stm});
        } else {
            visit_tree_result = nullptr;
        }
    } else if(node->type->typeKind == fdmj::TypeKind::ARRAY) {
        // cout << "vardecl array" << endl;
        tree::Exp* tmpexp = new tree::TempExp(tree::Type::PTR, tm->newtemp());
        node->id->accept(*this);
        tree::Exp* vd;
        if (visit_exp_result){
            vd = visit_exp_result->unEx(tm)->exp;
        }else{
            vd = tmpexp;
            last_tmp = tmpexp;
        }

        vector<IntExp*>* init_array = std::get<vector<IntExp*>*>(node->init);
        
        node->type->arity->accept(*this);
        tree::Const* array_size = dynamic_cast<tree::Const*>(visit_exp_result->unEx(tm)->exp);
        // 如果有初始化数组，那么size取决于初始化数组的长度
        if(init_array){
            array_size = new tree::Const(init_array->size());
        }
        vector<tree::Stm*> *sl = new vector<tree::Stm*>();
        vector<tree::Exp*> *args = new vector<tree::Exp*>();
        args->push_back(new tree::Const((array_size->constVal+1)*Compiler_Config::get("int_length")));
        sl->push_back(new tree::Move(vd, new tree::ExtCall(tree::Type::PTR, "malloc", args)));
        sl->push_back(new tree::Move(new tree::Mem(tree::Type::INT, vd), array_size));
        
        if(init_array) {
            for(int i = 0; i < init_array->size(); i++) {
                IntExp* init = (*init_array)[i];
                init->accept(*this);
                sl->push_back(new tree::Move(new tree::Mem(tree::Type::INT, new tree::Binop(tree::Type::PTR, "+", vd, new tree::Const((i+1)*Compiler_Config::get("int_length")))), visit_exp_result->unEx(tm)->exp));
            }
        }
        visit_tree_result = new tree::Seq(sl);

    } else if(node->type->typeKind == fdmj::TypeKind::CLASS) {
        // cout << "vardecl class" << endl;
        node->id->accept(*this);
        tree::Exp* vd = visit_exp_result->unEx(tm)->exp;
        vector<tree::Stm*> *sl = new vector<tree::Stm*>();

        int malloc_size = class_table->get_malloc_size();

        sl->push_back(new tree::Move(vd, new tree::ExtCall(tree::Type::PTR, "malloc", new vector<tree::Exp*>{new tree::Const(malloc_size)})));

        string clsname = node->type->cid->id;
        vector<string>* ancestors = nm->get_ancestors(clsname);
        ancestors->insert(ancestors->begin(), clsname);

        set<string>* var_list = new set<string>();
        vector<string>* method_list = new vector<string>();

        for(string ancestor : *ancestors) {
            set<string>* tmp_var_list = nm->get_class_var_list(ancestor);
            for(auto t: *tmp_var_list){
                var_list->insert(t);
            }
            set<string>* tmp_method_list = nm->get_method_list(ancestor);
            for(auto t: *tmp_method_list){
                if(find(method_list->begin(), method_list->end(), t) == method_list->end())
                    method_list->push_back(t);
            }
        }

        for(auto var: *var_list){
            int pos = class_table->get_var_pos(var);
            tree::Exp* var_addr = new tree::Mem(tree::Type::PTR, new tree::Binop(tree::Type::PTR, "+", vd, new tree::Const(pos)));
            for(auto ancestor : *ancestors) {
                VarDecl* vdl = nm->get_class_var(ancestor, var);
                if(vdl){
                    if(vdl->type->typeKind != fdmj::TypeKind::CLASS){
                        vdl->accept(*this);
                        // 变量初始化
                        if(visit_tree_result != nullptr) {
                            tree::Stm* tmp_stm = dynamic_cast<tree::Stm*>(visit_tree_result);
                            tree::Exp* tmp_exp = last_tmp;
                            sl->push_back(new tree::Move(var_addr, new tree::Eseq(tree::Type::INT, tmp_stm, tmp_exp)));
                        }
                    }
                    break;
                }
            }
        }

        for(auto method: *method_list){
            int pos = class_table->get_method_pos(method);
            tree::Exp* method_addr = new tree::Mem(tree::Type::PTR, new tree::Binop(tree::Type::PTR, "+", vd, new tree::Const(pos)));
            for(auto ancestor : *ancestors) {
                if(nm->is_method(ancestor, method)){
                    tree::Exp* slabel = new tree::Name(tm->newstringlabel(ancestor+"^"+method));
                    sl->push_back(new tree::Move(method_addr, slabel));
                    break;
                }
            }
        }
        tm->newtemp(); // test
        visit_tree_result = new tree::Seq(sl);
    }
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::Type* node) {
    #ifdef DEBUG
    cout << "Visiting Type" << endl;
    #endif
}

void ASTToTreeVisitor::visit(fdmj::Nested* node) {
    #ifdef DEBUG
    cout << "Visiting Nested" << endl;
    #endif
    vector<tree::Stm*>* stmts = new vector<tree::Stm*>();
    if(node->sl){
        for (auto stmt : *node->sl) {
            stmt->accept(*this);
            if(visit_tree_result != nullptr) {
                stmts->push_back(dynamic_cast<tree::Stm*>(visit_tree_result));
            }
        }
    }
    visit_tree_result = new tree::Seq(stmts);
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::If* node) {
    #ifdef DEBUG
    cout << "Visiting If" << endl;
    #endif
    node->exp->accept(*this);
    Tr_Exp* cond_exp = visit_exp_result;
    Tr_cx* cond = cond_exp->unCx(tm);
    
    node->stm1->accept(*this);
    tree::Stm* then_stm = dynamic_cast<tree::Stm*>(visit_tree_result);
    
    tree::Label* true_label = tm->newlabel();
    tree::Label* false_label = tm->newlabel();
    tree::Label* join_label = tm->newlabel();
    
    vector<tree::Stm*>* stmts = new vector<tree::Stm*>();
    // 条件跳转
    stmts->push_back(cond->stm);
    cond->true_list->patch(true_label);
    cond->false_list->patch(false_label);
    
    // then部分
    stmts->push_back(new tree::LabelStm(true_label));
    stmts->push_back(then_stm);
    stmts->push_back(new tree::Jump(join_label));
    
    // else部分
    stmts->push_back(new tree::LabelStm(false_label));
    if (node->stm2) {
        node->stm2->accept(*this);
        stmts->push_back(dynamic_cast<tree::Stm*>(visit_tree_result));
    }
    // 出口
    stmts->push_back(new tree::LabelStm(join_label));
    
    visit_tree_result = new tree::Seq(stmts);
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::While* node) {
    #ifdef DEBUG
    cout << "Visiting While" << endl;
    #endif
    // 处理条件表达式
    node->exp->accept(*this);
    Tr_Exp* cond_exp = visit_exp_result;
    Tr_cx* cond = cond_exp->unCx(tm);

    // 创建标签
    tree::Label* test_label = tm->newlabel();
    tree::Label* body_label = tm->newlabel();
    tree::Label* done_label = tm->newlabel();

    // 填充条件表达式的label
    cond->true_list->patch(body_label);  // 条件为真跳转到循环体
    cond->false_list->patch(done_label); // 条件为假跳转到结束

    // 记录循环入口和出口标签
    loop_entry_labels.push(test_label);
    loop_exit_labels.push(done_label);
    
    // 处理循环体
    node->stm->accept(*this);
    tree::Stm* body_stm = dynamic_cast<tree::Stm*>(visit_tree_result);
    
    // 构建控制流
    vector<tree::Stm*>* stmts = new vector<tree::Stm*>();
    stmts->push_back(new tree::LabelStm(test_label));
    stmts->push_back(cond->stm);
    stmts->push_back(new tree::LabelStm(body_label));
    stmts->push_back(body_stm);
    stmts->push_back(new tree::Jump(test_label));
    stmts->push_back(new tree::LabelStm(done_label));

    // 清理循环标签栈
    loop_entry_labels.pop();
    loop_exit_labels.pop();
    
    visit_tree_result = new tree::Seq(stmts);
    visit_exp_result = nullptr; 
}

void ASTToTreeVisitor::visit(fdmj::Assign* node) {
    #ifdef DEBUG
    cout << "Visiting Assign" << endl;
    #endif
    node->left->accept(*this);
    Tr_Exp* left_exp = visit_exp_result;
    node->exp->accept(*this);
    Tr_Exp* right_exp = visit_exp_result;
    
    visit_tree_result = new tree::Move(left_exp->unEx(tm)->exp, right_exp->unEx(tm)->exp);
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::CallStm* node) {
    #ifdef DEBUG
    cout << "Visiting CallStm" << endl;
    #endif
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    // 将对象指针加入args
    node->obj->accept(*this);
    tree::Exp* obj_exp = visit_exp_result->unEx(tm)->exp;
    args->push_back(obj_exp);
    vector<fdmj::Exp*>* params = node->par;
    if(params){
        for (auto param : *params) {
            param->accept(*this);
            Tr_Exp* arg_exp = visit_exp_result;
            args->push_back(arg_exp->unEx(tm)->exp);
        }
    }

    // 获取函数方法地址
    string method_name = node->name->id;
    int method_offset = class_table->get_method_pos(method_name);
    tree::Exp* method_addr = new tree::Mem(tree::Type::PTR, new tree::Binop(tree::Type::PTR, "+", obj_exp, new tree::Const(method_offset)));
    
    // 获取返回值类型
    AST_Semant* obj_semant = semant_map->getSemant(node->obj);
    string method_cls = std::get<string>(obj_semant->get_type_par());

    // 在继承链中找到这个变量的类型
    Formal* cfl;
    vector<string>* ancestors = nm->get_ancestors(method_cls);
    ancestors->insert(ancestors->begin(), method_cls);
    for(auto ancestor: *ancestors){
        cfl = nm->get_method_formal(ancestor, method_name, "_^return^_"+method_name);
        if(cfl) break;
    }
    tree::Type return_type = (cfl->type->typeKind == fdmj::TypeKind::INT)?tree::Type::INT:tree::Type::PTR;

    visit_tree_result = new ExpStm(new tree::Call(return_type, node->name->id, method_addr, args));
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::Return* node) {
    #ifdef DEBUG
    cout << "Visiting Return" << endl;
    #endif
    node->exp->accept(*this);
    visit_tree_result = new tree::Return(visit_exp_result->unEx(tm)->exp);
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::BinaryOp* node) {
    string op = node->op->op;
    node->left->accept(*this);
    Tr_Exp* left_exp = visit_exp_result;
    node->right->accept(*this);
    Tr_Exp* right_exp = visit_exp_result;
    
    if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        #ifdef DEBUG
        cout << "Visiting Relational Operation" << endl;
        #endif
        // 关系运算转换为条件跳转
        tree::Label* true_label = tm->newlabel();
        tree::Label* false_label = tm->newlabel();
        
        Patch_list* true_list = new Patch_list();
        Patch_list* false_list = new Patch_list();
        
        true_list->add_patch(true_label); 
        false_list->add_patch(false_label);
        
        visit_exp_result = new Tr_cx(
            true_list, 
            false_list,
            new tree::Cjump(
                op,
                left_exp->unEx(tm)->exp,
                right_exp->unEx(tm)->exp, 
                true_label, // 临时true目标
                false_label  // 临时false目标
            )
        );
    } 
    else if (op == "&&" || op == "||") {
        #ifdef DEBUG
        cout << "Visiting Logical Operation" << endl;
        #endif
        // 逻辑运算处理（短路求值）
        Patch_list* true_list = new Patch_list();
        Patch_list* false_list = new Patch_list();
        tree::Label* right_label = tm->newlabel();

        Tr_cx* left_cx = left_exp->unCx(tm);
        Tr_cx* right_cx = right_exp->unCx(tm);

        if (op == "&&") {
            false_list->add(left_cx->false_list);       // 左假则整个假 
            left_cx->true_list->patch(right_label);     // 左真则取决于右
        } else {
            true_list->add(left_cx->true_list);         // 左真则整个真 
            left_cx->false_list->patch(right_label);    // 左假则取决于右
        }

        // 处理右操作数
        false_list->add(right_cx->false_list);
        true_list->add(right_cx->true_list);

        // 构建控制流
        vector<tree::Stm*>* stmts = new vector<tree::Stm*>();
        stmts->push_back(left_cx->stm);
        stmts->push_back(new tree::LabelStm(right_label));
        stmts->push_back(right_cx->stm);

        visit_exp_result = new Tr_cx(true_list, false_list, new tree::Seq(stmts));
    }
    else {
        #ifdef DEBUG
        cout << "Visiting Arithmetic Operation" << endl;    
        #endif
        // 普通算术运算
        if(left_exp->unEx(tm)->exp->type == tree::Type::INT){
            visit_exp_result = new Tr_ex(new tree::Binop(tree::Type::INT, op, left_exp->unEx(tm)->exp, right_exp->unEx(tm)->exp));
        }else if(left_exp->unEx(tm)->exp->type == tree::Type::PTR){
            vector<tree::Stm*>* stmts = new vector<tree::Stm*>();
            // 获得两个数组长度
            tree::Exp* left_temp = left_exp->unEx(tm)->exp;
            tree::Exp* right_temp = right_exp->unEx(tm)->exp;
            tree::Exp* left_length = new TempExp(tree::Type::INT, tm->newtemp());
            tree::Exp* right_length = new TempExp(tree::Type::INT, tm->newtemp());
            stmts->push_back(new tree::Move(left_length, new tree::Mem(tree::Type::INT, left_temp)));
            stmts->push_back(new tree::Move(right_length, new tree::Mem(tree::Type::INT, right_temp)));
            
            // 检查长度是否相等
            tree::Label* error_label = tm->newlabel();
            tree::Label* check_label = tm->newlabel();
            tree::Stm* error_stm = new tree::ExpStm(new tree::ExtCall(tree::Type::INT, "exit", new vector<tree::Exp*>{new tree::Const(-1)}));
            stmts->push_back(new tree::Cjump("!=", left_length, right_length, error_label, check_label));
            stmts->push_back(new tree::LabelStm(error_label));
            stmts->push_back(error_stm);
            stmts->push_back(new tree::LabelStm(check_label));

            // 分配新数组
            tree::Exp* res_arr = new tree::TempExp(tree::Type::PTR, tm->newtemp());
            stmts->push_back(new tree::Move(res_arr, new tree::ExtCall(tree::Type::PTR, "malloc", new vector<tree::Exp*>{new tree::Binop(tree::Type::INT, "*", new tree::Binop(tree::Type::INT, "+", left_length, new tree::Const(1)), new tree::Const(Compiler_Config::get("int_length")))})));
            stmts->push_back(new tree::Move(new tree::Mem(tree::Type::PTR, res_arr), left_length));
            
            // 偏移量
            tree::Exp* bias = new tree::TempExp(tree::Type::INT, tm->newtemp());
            tree::Exp* total_bias = new tree::TempExp(tree::Type::INT, tm->newtemp());
            stmts->push_back(new tree::Move(bias, new tree::Const(Compiler_Config::get("int_length"))));
            stmts->push_back(new tree::Move(total_bias, new tree::Binop(tree::Type::INT, "*", new tree::Binop(tree::Type::INT, "+", left_length, new tree::Const(1)), new tree::Const(Compiler_Config::get("int_length")))));
            
            // 开始循环 
            tree::Label* loop_label = tm->newlabel();
            tree::Label* loop_body_label = tm->newlabel();
            tree::Label* loop_end_label = tm->newlabel();
            stmts->push_back(new tree::LabelStm(loop_label));
            stmts->push_back(new tree::Cjump("<", bias, total_bias, loop_body_label, loop_end_label));
            stmts->push_back(new tree::LabelStm(loop_body_label));

            tree::Exp* res_addr = new tree::Binop(tree::Type::PTR, "+", res_arr, bias);
            tree::Exp* left_addr = new tree::Binop(tree::Type::PTR, "+", left_temp, bias);
            tree::Exp* right_addr = new tree::Binop(tree::Type::PTR, "+", right_temp, bias);
            stmts->push_back(new tree::Move(new tree::Mem(tree::Type::PTR, res_addr), new tree::Binop(tree::Type::INT, op, new tree::Mem(tree::Type::INT, left_addr), new tree::Mem(tree::Type::INT, right_addr))));
            stmts->push_back(new tree::Move(bias, new tree::Binop(tree::Type::INT, "+", bias, new tree::Const(Compiler_Config::get("int_length")))));
            stmts->push_back(new tree::Jump(loop_label));
            stmts->push_back(new tree::LabelStm(loop_end_label));
            // 结束循环
            visit_exp_result = new Tr_ex(new tree::Eseq(tree::Type::PTR, new tree::Seq(stmts), res_arr));
        }
    }
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::UnaryOp* node) {
    string op = node->op->op;
    node->exp->accept(*this);
    tree::Exp* exp = visit_exp_result->unEx(tm)->exp;

    if(op == "!"){
        // 逻辑非运算
        tree::Label* true_label = tm->newlabel();
        tree::Label* false_label = tm->newlabel();
        
        Patch_list* true_list = new Patch_list();
        Patch_list* false_list = new Patch_list();
        
        true_list->add_patch(true_label); 
        false_list->add_patch(false_label);
        
        visit_exp_result = new Tr_cx(
            true_list, 
            false_list,
            new tree::Cjump(
                "==",
                exp,
                new tree::Const(0), 
                true_label, // 临时true目标
                false_label  // 临时false目标
            )
        );
    }else if(op == "-"){
        // 负号运算
        if(exp->type == tree::Type::INT){
            visit_exp_result = new Tr_ex(new tree::Binop(tree::Type::INT, "-", new tree::Const(0), exp));
        }else if(exp->type == tree::Type::PTR){
            vector<tree::Stm*>* stmts = new vector<tree::Stm*>();
            // 获得数组长度
            tree::Exp* arr_length = new TempExp(tree::Type::INT, tm->newtemp());
            stmts->push_back(new tree::Move(arr_length, new tree::Mem(tree::Type::INT, exp)));

            // 分配新数组
            tree::Exp* res_arr = new tree::TempExp(tree::Type::PTR, tm->newtemp());
            stmts->push_back(new tree::Move(res_arr, new tree::ExtCall(tree::Type::PTR, "malloc", new vector<tree::Exp*>{new tree::Binop(tree::Type::INT, "*", new tree::Binop(tree::Type::INT, "+", arr_length, new tree::Const(1)), new tree::Const(Compiler_Config::get("int_length")))})));
            stmts->push_back(new tree::Move(new tree::Mem(tree::Type::PTR, res_arr), arr_length));
            
            // 偏移量
            tree::Exp* bias = new tree::TempExp(tree::Type::INT, tm->newtemp());
            tree::Exp* total_bias = new tree::TempExp(tree::Type::INT, tm->newtemp());
            stmts->push_back(new tree::Move(bias, new tree::Const(Compiler_Config::get("int_length"))));
            stmts->push_back(new tree::Move(total_bias, new tree::Binop(tree::Type::INT, "*", new tree::Binop(tree::Type::INT, "+", arr_length, new tree::Const(1)), new tree::Const(Compiler_Config::get("int_length")))));
            
            // 开始循环 
            tree::Label* loop_label = tm->newlabel();
            tree::Label* loop_body_label = tm->newlabel();
            tree::Label* loop_end_label = tm->newlabel();
            stmts->push_back(new tree::LabelStm(loop_label));
            stmts->push_back(new tree::Cjump("<", bias, total_bias, loop_body_label, loop_end_label));
            stmts->push_back(new tree::LabelStm(loop_body_label));

            tree::Exp* res_addr = new tree::Binop(tree::Type::PTR, "+", res_arr, bias);
            tree::Exp* src_addr = new tree::Binop(tree::Type::PTR, "+", exp, bias);
            stmts->push_back(new tree::Move(new tree::Mem(tree::Type::PTR, res_addr), new tree::Binop(tree::Type::INT, "-", new tree::Const(0), new tree::Mem(tree::Type::INT, src_addr))));
            stmts->push_back(new tree::Move(bias, new tree::Binop(tree::Type::INT, "+", bias, new tree::Const(Compiler_Config::get("int_length")))));
            stmts->push_back(new tree::Jump(loop_label));
            stmts->push_back(new tree::LabelStm(loop_end_label));
            // 结束循环
            visit_exp_result = new Tr_ex(new tree::Eseq(tree::Type::PTR, new tree::Seq(stmts), res_arr));
        }
    }else{
        // 其他运算(不可到达)
        visit_exp_result = new Tr_ex(new tree::Binop(tree::Type::INT, op, exp, new tree::Const(0)));
    }
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::IdExp* node) {
    if(mvt->var_temp_map->find(node->id) != mvt->var_temp_map->end()){
        visit_exp_result = new Tr_ex(new tree::TempExp(mvt->get_var_type(node->id), mvt->get_var_temp(node->id)));
    }else{
        visit_exp_result = nullptr;
    }
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::IntExp* node) {
    visit_exp_result = new Tr_ex(new tree::Const(node->val));
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::BoolExp* node) {
    visit_exp_result = new Tr_ex(new tree::Const(node->val ? 1 : 0));
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::This* node) {
    #ifdef DEBUG
    cout << "Visiting This" << endl;
    #endif
    visit_exp_result = new Tr_ex(new tree::TempExp(mvt->get_var_type("_^THIS^_"), mvt->get_var_temp("_^THIS^_")));
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::ArrayExp* node) {
    node->index->accept(*this);
    tree::Exp* index = visit_exp_result->unEx(tm)->exp;
    node->arr->accept(*this);
    tree::Exp* arr_ = visit_exp_result->unEx(tm)->exp;
    vector<tree::Stm*>* exp_stmts = new vector<tree::Stm*>();

    // 数组部分是否有符合操作
    tree::Exp* arr = nullptr;
    if(arr_->getTreeKind() == tree::Kind::CALL){
        arr = new TempExp(tree::Type::PTR, tm->newtemp());
        exp_stmts->push_back(new tree::Move(arr, arr_));
    }else{
        arr = arr_;
    }

    // 索引部分是否有符合操作
    tree::Exp* idx = nullptr;
    if(index->getTreeKind() != tree::Kind::CONST && index->getTreeKind() != tree::Kind::TEMPEXP){
        idx = new TempExp(tree::Type::INT, tm->newtemp());
        exp_stmts->push_back(new tree::Move(idx, index));
    }else{
        idx = index;
    }

    // 检查索引是否越界
    tree::Exp* length = new tree::TempExp(tree::Type::INT, tm->newtemp());
    // 处理越界错误
    tree::Label* error_label = tm->newlabel();
    tree::Label* check_label = tm->newlabel();
    tree::Stm* error_stm = new tree::ExpStm(new tree::ExtCall(tree::Type::INT, "exit", new vector<tree::Exp*>{new tree::Const(-1)}));
    vector<tree::Stm*>* stmts = new vector<tree::Stm*>();
    stmts->push_back(new tree::Move(length, new tree::Mem(tree::Type::INT, arr)));
    stmts->push_back(new tree::Cjump(">=", idx, length, error_label, check_label));
    stmts->push_back(new tree::LabelStm(error_label));
    stmts->push_back(error_stm);
    stmts->push_back(new tree::LabelStm(check_label));
    tree::Exp* checked_idx = new tree::Eseq(tree::Type::INT, new tree::Seq(stmts), idx);
    // 访问数组元素
    tree::Exp* bias = new tree::Binop(tree::Type::INT, "*", new tree::Binop(tree::Type::INT, "+", checked_idx, new tree::Const(1)), new tree::Const(Compiler_Config::get("int_length")));
    
    if(exp_stmts->size()>0){
        visit_exp_result = new Tr_ex(new tree::Eseq(tree::Type::INT, new tree::Seq(exp_stmts), new tree::Mem(tree::Type::INT, new tree::Binop(tree::Type::PTR, "+", arr, bias))));
    }else{
        visit_exp_result = new Tr_ex(new tree::Mem(tree::Type::INT, new tree::Binop(tree::Type::PTR, "+", arr, bias)));
    }
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::CallExp* node) {
    #ifdef DEBUG
    cout << "Visiting CallExp" << endl;
    #endif
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    // 将对象指针加入args
    node->obj->accept(*this);
    tree::Exp* obj_exp = visit_exp_result->unEx(tm)->exp;
    args->push_back(obj_exp);
    vector<fdmj::Exp*>* params = node->par;
    if(params){
        for (auto param : *params) {
            param->accept(*this);
            Tr_Exp* arg_exp = visit_exp_result;
            args->push_back(arg_exp->unEx(tm)->exp);
        }
    }

    // 获取函数方法地址
    string method_name = node->name->id;
    int method_offset = class_table->get_method_pos(method_name);
    tree::Exp* method_addr = new tree::Mem(tree::Type::PTR, new tree::Binop(tree::Type::PTR, "+", obj_exp, new tree::Const(method_offset)));

    // 获取返回值类型
    AST_Semant* obj_semant = semant_map->getSemant(node->obj);
    string method_cls = std::get<string>(obj_semant->get_type_par());

    // 在继承链中找到这个变量的类型
    Formal* cfl;
    vector<string>* ancestors = nm->get_ancestors(method_cls);
    ancestors->insert(ancestors->begin(), method_cls);
    for(auto ancestor: *ancestors){
        cfl = nm->get_method_formal(ancestor, method_name, "_^return^_"+method_name);
        if(cfl) break;
    }
    tree::Type return_type = (cfl->type->typeKind == fdmj::TypeKind::INT)?tree::Type::INT:tree::Type::PTR;

    visit_exp_result = new Tr_ex(new tree::Call(return_type, node->name->id, method_addr, args));
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::ClassVar* node) {
    #ifdef DEBUG
    cout << "Visiting ClassVar" << endl;
    #endif
    node->obj->accept(*this);
    tree::Exp* obj_exp = visit_exp_result->unEx(tm)->exp;
    // 获取函数方法地址
    string var_name = node->id->id;
    int var_offset = class_table->get_var_pos(var_name);
    
    // 获取变量类型
    AST_Semant* obj_semant = semant_map->getSemant(node->obj);
    string var_cls = std::get<string>(obj_semant->get_type_par());
    // 在继承链中找到这个变量的类型
    VarDecl* cvdl;
    vector<string>* ancestors = nm->get_ancestors(var_cls);
    ancestors->insert(ancestors->begin(), var_cls);
    for(auto ancestor: *ancestors){
        cvdl = nm->get_class_var(ancestor, var_name);
        if(cvdl) break;
    }
    tree::Type cvdl_type = (cvdl->type->typeKind == fdmj::TypeKind::INT)?tree::Type::INT:tree::Type::PTR;
    
    visit_exp_result = new Tr_ex(new tree::Mem(cvdl_type, new tree::Binop(tree::Type::PTR, "+", obj_exp, new tree::Const(var_offset))));
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::Length* node) {
    #ifdef DEBUG
    cout << "Visiting Length" << endl;
    #endif
    node->exp->accept(*this);
    tree::Exp* arr = visit_exp_result->unEx(tm)->exp;
    tree::TempExp* arr_length = new tree::TempExp(tree::Type::INT, tm->newtemp());
    tree::Exp* length_exp = new tree::Eseq(tree::Type::INT, new tree::Move(arr_length, new tree::Mem(tree::Type::INT, arr)), arr_length);
    visit_exp_result = new Tr_ex(length_exp);
    // visit_exp_result = new Tr_ex(new tree::ExtCall(tree::Type::INT, "length", new vector<tree::Exp*>{arr}));
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::Esc* node) {
    #ifdef DEBUG
    cout << "Visiting Esc" << endl;
    #endif
    // 处理stms
    vector<tree::Stm*>* seq = new vector<tree::Stm*>();
    for(auto s: *node->sl){
        s->accept(*this);
        tree::Stm* stm = dynamic_cast<tree::Stm*>(visit_tree_result);
        seq->push_back(stm);
    }
    // 处理exp
    node->exp->accept(*this);
    tree::Exp* exp = visit_exp_result->unEx(tm)->exp;

    visit_exp_result = new Tr_ex(new tree::Eseq(tree::Type::INT, new tree::Seq(seq), exp));  // todo
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::GetInt* node) {
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    visit_exp_result = new Tr_ex(new tree::ExtCall(tree::Type::INT, "getint", args));
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::GetCh* node) {
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    visit_exp_result = new Tr_ex(new tree::ExtCall(tree::Type::INT, "getch", args));
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::GetArray* node) {
    node->exp->accept(*this);
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    args->push_back(visit_exp_result->unEx(tm)->exp);
    visit_exp_result = new Tr_ex(new tree::ExtCall(tree::Type::INT, "getarray", args));
    visit_tree_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::OpExp* node) {
    #ifdef DEBUG
    cout << "Visiting OpExp" << endl;
    #endif
}

void ASTToTreeVisitor::visit(fdmj::Continue* node) {
    #ifdef DEBUG
    cout << "Visiting Continue" << endl;
    #endif
    if (loop_entry_labels.empty()) {
        cerr << "Error: 'continue' not within a loop" << endl;
        exit(1);
    }
    tree::Label* loop_entry = loop_entry_labels.top();
    visit_tree_result = new tree::Jump(loop_entry);
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::Break* node) {
    #ifdef DEBUG
    cout << "Visiting Break" << endl;
    #endif
    if (loop_exit_labels.empty()) {
        cerr << "Error: 'break' not within a loop" << endl;
        exit(1);
    }
    tree::Label* loop_exit = loop_exit_labels.top();
    visit_tree_result = new tree::Jump(loop_exit);
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::PutInt* node) {
    #ifdef DEBUG
    cout << "Visiting PutInt" << endl;
    #endif
    node->exp->accept(*this);
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    args->push_back(visit_exp_result->unEx(tm)->exp);
    visit_tree_result = new tree::ExpStm(new tree::ExtCall(tree::Type::INT, "putint", args));
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::PutCh* node) {
    #ifdef DEBUG
    cout << "Visiting PutCh" << endl;
    #endif
    node->exp->accept(*this);
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    args->push_back(visit_exp_result->unEx(tm)->exp);
    visit_tree_result = new tree::ExpStm(new tree::ExtCall(tree::Type::INT, "putch", args));
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::PutArray* node) {
    node->n->accept(*this);
    tree::Exp* n = visit_exp_result->unEx(tm)->exp;
    node->arr->accept(*this);
    tree::Exp* exp = visit_exp_result->unEx(tm)->exp;
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    args->push_back(n);
    args->push_back(exp);
    visit_tree_result = new tree::ExpStm(new tree::ExtCall(tree::Type::INT, "putarray", args));
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::Starttime* node) {
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    visit_tree_result = new tree::ExpStm(new tree::ExtCall(tree::Type::INT, "starttime", args));
    visit_exp_result = nullptr;
}

void ASTToTreeVisitor::visit(fdmj::Stoptime* node) {
    vector<tree::Exp*>* args = new vector<tree::Exp*>();
    visit_tree_result = new tree::ExpStm(new tree::ExtCall(tree::Type::INT, "stoptime", args));
    visit_exp_result = nullptr;
}