#include <queue>
#include <set>
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iostream>
#include <type_traits>
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Pass.h"
#include "llvm/IR/Mangler.h" 
#include "llvm/IR/Module.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include "Nova.h"

using namespace llvm;
using std::stringstream;

// The MACROs below are for evaluation only, controlling the percentage of instrumented variables
//#define INSTRUMENT_30_PER
//#define INSTRUMENT_50_PER
//#define INSTRUMENT_ALL
//#define INSTRUMENT_HALF

// statistics
int define_event_count = 0;
int use_event_count = 0;

Type *m_void_ptr_type;
ConstantPointerNull* m_void_null_ptr;
Value* m_infinite_bound_ptr;
std::map<Value*, Value*> m_pointer_base;
std::map<Value*, Value*> m_pointer_bound;
Function* m_spatial_load_dereference_check;
Function* m_spatial_store_dereference_check;
Function* m_load_base_bound_func;
Function* m_metadata_load_vector_func;
Function* m_metadata_store_vector_func;

/* Function Type of the function that stores the base and bound
 * for a given pointer
 */
Function* m_store_base_bound_func;

/* Map of all functions defined by Softboundcets */
StringMap<bool> m_func_def_softbound;
StringMap<bool> m_func_wrappers_available;

/* Map of all functions for which Softboundcets Transformation must
 * be invoked
 */
StringMap<bool> m_func_softboundcets_transform;

/* Map of all functions that need to be transformed as they have as
 * they either hava pointer arguments or pointer return type and are
 * defined in the module
 */
StringMap<bool> m_func_to_transform;
std::map<GlobalVariable*, int> m_initial_globals;
std::map<Value*, int> m_present_in_original;
std::map<Value*, int> m_is_pointer;
enum { SBCETS_BITCAST, SBCETS_GEP};

std::map<Value*, Value*> m_vector_pointer_base;
std::map<Value*, Value*> m_vector_pointer_bound;

Type* m_key_type;

Function* m_memcopy_check;
Function* m_memset_check;
Function* m_copy_metadata;
Function* m_shadow_stack_allocate;
Function* m_shadow_stack_deallocate;
Function* m_shadow_stack_base_load;
Function* m_shadow_stack_bound_load;
Function* m_shadow_stack_base_store;
Function* m_shadow_stack_bound_store;
Function* m_call_dereference_func;

Constant* m_constantint64ty_zero;
bool GLOBALCONSTANTOPT = true;
bool BOUNDSCHECKOPT = true; 
bool CALLCHECKS = true;
bool INDIRECTCALLCHECKS = true;

bool Nova::runOnModule(Module &M) {
    GlobalStateRef GS;
    Function *f = NULL;

    // add function definition for constructors and checkers 
    //ConstructCheckHandlers(M);

    // initialize common constants
    //m_void_ptr_type = PointerType::getUnqual(Type::getInt8Ty(M.getContext()));
    //size_t inf_bound = (size_t) pow(2, 48);
    //ConstantInt* infinite_bound = ConstantInt::get(Type::getInt64Ty(M.getContext()), inf_bound, false);
    //m_infinite_bound_ptr = ConstantExpr::getIntToPtr(infinite_bound, m_void_ptr_type);
    //PointerType* vptrty = dyn_cast<PointerType>(m_void_ptr_type);
    //m_void_null_ptr = ConstantPointerNull::get(vptrty);

    GS = new GlobalState();
    GS->tMap = new TaintMap();
    GS->pMap = new PointsToMap();
    GS->vMap = new VisitMap();
    GS->senVarSet = new ValueSet();

    // initialize pointsto map
    InitializeGS(GS, M);

    f = M.getFunction("main");
    if (f == NULL)
        errs() << "f is NULL!" <<"\n";
    Traversal(GS, f);

    errs() << "Points To Map:\n";
    //PrintPointsToMap(GS);

    errs() << "\nTaint Map:\n";
    //PrintTaintMap(GS);

    // get initial set of sensitive variables annotated by programmer.
    GetAnnotatedVariables(M, GS);

    // enforce def-use check
    DefUseCheck(M, GS);

    errs() << "\ndefine_event_count: " << define_event_count << "\n";
    errs() << "\nuse_event_count: " << use_event_count << "\n";

    return true;
}

void Nova::ConstructCheckHandlers(Module &module){

  Type* void_ty = Type::getVoidTy(module.getContext());
  Type* void_ptr_ty = PointerType::getUnqual(Type::getInt8Ty(module.getContext()));
  Type* size_ty = Type::getInt64Ty(module.getContext());
  Type* ptr_void_ptr_ty = PointerType::getUnqual(void_ptr_ty);
  Type* ptr_size_ty = PointerType::getUnqual(size_ty);
  Type* int32_ty = Type::getInt32Ty(module.getContext());
  m_key_type = Type::getInt64Ty(module.getContext());

  m_constantint64ty_zero = 
    ConstantInt::get(Type::getInt64Ty(module.getContext()), 0);

  module.getOrInsertFunction("__softboundcets_metadata_store", 
                               void_ty, void_ptr_ty, void_ptr_ty, 
                               void_ptr_ty, NULL);

  m_store_base_bound_func = module.getFunction("__softboundcets_metadata_store");
  assert(m_store_base_bound_func && "__softboundcets_metadata_store null?");

  module.getOrInsertFunction("__softboundcets_spatial_load_dereference_check",
                             void_ty, void_ptr_ty, void_ptr_ty, 
                             void_ptr_ty, size_ty, NULL);

  module.getOrInsertFunction("__softboundcets_spatial_store_dereference_check", 
                             void_ty, void_ptr_ty, void_ptr_ty, 
                             void_ptr_ty, size_ty, NULL);
  m_spatial_load_dereference_check =
    module.getFunction("__softboundcets_spatial_load_dereference_check");
  assert(m_spatial_load_dereference_check &&
         "__softboundcets_spatial_load_dereference_check function type null?");

  m_spatial_store_dereference_check = 
    module.getFunction("__softboundcets_spatial_store_dereference_check");
  assert(m_spatial_store_dereference_check && 
         "__softboundcets_spatial_store_dereference_check function type null?");

  module.getOrInsertFunction("__softboundcets_metadata_load", 
                             void_ty, void_ptr_ty, ptr_void_ptr_ty, ptr_void_ptr_ty, NULL);

  m_load_base_bound_func = module.getFunction("__softboundcets_metadata_load");
  assert(m_load_base_bound_func && "__softboundcets_metadata_load null?");

  module.getOrInsertFunction("__softboundcets_metadata_load_vector", 
                             void_ty, void_ptr_ty, ptr_void_ptr_ty, ptr_void_ptr_ty, 
                             ptr_size_ty, ptr_void_ptr_ty, int32_ty, NULL);

  m_metadata_load_vector_func = module.getFunction("__softboundcets_metadata_load_vector");
  assert(m_metadata_load_vector_func && "__softboundcets_metadata_load_vector null?");

  module.getOrInsertFunction("__softboundcets_metadata_store_vector", 
                             void_ty, void_ptr_ty, void_ptr_ty, 
                             void_ptr_ty, size_ty, void_ptr_ty, int32_ty, NULL);

  m_metadata_store_vector_func = module.getFunction("__softboundcets_metadata_store_vector");
  assert(m_metadata_store_vector_func && "__softboundcets_metadata_store_vector null?");


    module.getOrInsertFunction("__softboundcets_memcopy_check",
                               void_ty, void_ptr_ty, void_ptr_ty, size_ty, 
                               void_ptr_ty, void_ptr_ty, void_ptr_ty, void_ptr_ty, NULL);

  m_memcopy_check = 
    module.getFunction("__softboundcets_memcopy_check");
  assert(m_memcopy_check && 
         "__softboundcets_memcopy_check function null?");

    module.getOrInsertFunction("__softboundcets_memset_check",
                               void_ty, void_ptr_ty,size_ty, 
                               void_ptr_ty, void_ptr_ty, NULL);

  m_memset_check = 
    module.getFunction("__softboundcets_memset_check");
  assert(m_memcopy_check && 
         "__softboundcets_memset_check function null?");

  module.getOrInsertFunction("__softboundcets_introspect_metadata", 
                             void_ty, void_ptr_ty, void_ptr_ty, int32_ty, NULL);
  module.getOrInsertFunction("__softboundcets_copy_metadata", 
                             void_ty, void_ptr_ty, void_ptr_ty, size_ty, NULL);

  m_copy_metadata = module.getFunction("__softboundcets_copy_metadata");
  assert(m_copy_metadata && "__softboundcets_copy_metadata NULL?");

  module.getOrInsertFunction("__softboundcets_allocate_shadow_stack_space", 
                             void_ty, int32_ty, NULL);
  module.getOrInsertFunction("__softboundcets_deallocate_shadow_stack_space", 
                             void_ty, NULL);

  m_shadow_stack_allocate = 
    module.getFunction("__softboundcets_allocate_shadow_stack_space");
  assert(m_shadow_stack_allocate && 
         "__softboundcets_allocate_shadow_stack_space NULL?");

  module.getOrInsertFunction("__softboundcets_load_base_shadow_stack", 
                             void_ptr_ty, int32_ty, NULL);
  module.getOrInsertFunction("__softboundcets_load_bound_shadow_stack", 
                             void_ptr_ty, int32_ty, NULL);

  m_shadow_stack_base_load = 
    module.getFunction("__softboundcets_load_base_shadow_stack");
  assert(m_shadow_stack_base_load && 
         "__softboundcets_load_base_shadow_stack NULL?");

  m_shadow_stack_bound_load = 
    module.getFunction("__softboundcets_load_bound_shadow_stack");
  assert(m_shadow_stack_bound_load && 
         "__softboundcets_load_bound_shadow_stack NULL?");

  module.getOrInsertFunction("__softboundcets_store_base_shadow_stack", 
                             void_ty, void_ptr_ty, int32_ty, NULL);
  module.getOrInsertFunction("__softboundcets_store_bound_shadow_stack", 
                             void_ty, void_ptr_ty, int32_ty, NULL);

  m_shadow_stack_base_store = 
    module.getFunction("__softboundcets_store_base_shadow_stack");
  assert(m_shadow_stack_base_store && 
         "__softboundcets_store_base_shadow_stack NULL?");
  
  m_shadow_stack_bound_store = 
    module.getFunction("__softboundcets_store_bound_shadow_stack");
  assert(m_shadow_stack_bound_store && 
         "__softboundcets_store_bound_shadow_stack NULL?");

  m_shadow_stack_deallocate = 
    module.getFunction("__softboundcets_deallocate_shadow_stack_space");
  assert(m_shadow_stack_deallocate && 
         "__softboundcets_deallocate_shadow_stack_space NULL?");

  module.getOrInsertFunction("__softboundcets_stack_memory_deallocation", 
                             void_ty, size_ty, NULL);

  module.getOrInsertFunction("__softboundcets_spatial_call_dereference_check",
                             void_ty, void_ptr_ty, void_ptr_ty, void_ptr_ty, NULL);

  m_call_dereference_func = 
    module.getFunction("__softboundcets_spatial_call_dereference_check");
  assert(m_call_dereference_func && 
         "__softboundcets_spatial_call_dereference_check function null??");

  Function* global_init = (Function *) module.getOrInsertFunction("__softboundcets_global_init", 
                                                                  void_ty, NULL);

  global_init->setDoesNotThrow();
  global_init->setLinkage(GlobalValue::InternalLinkage);

  BasicBlock* BB = BasicBlock::Create(module.getContext(), 
                                      "entry", global_init);
  
  Function* softboundcets_init = (Function*) module.getOrInsertFunction("__softboundcets_init", void_ty, Type::getInt32Ty(module.getContext()), NULL);

  
  SmallVector<Value*, 8> args;
  Constant * const_one = ConstantInt::get(Type::getInt32Ty(module.getContext()), 1);
  
  args.push_back(const_one);
  Instruction* ret = ReturnInst::Create(module.getContext(), BB);
  
  CallInst::Create(softboundcets_init, args, "", ret);

  Type * Int32Type = IntegerType::getInt32Ty(module.getContext());
  std::vector<Constant *> CtorInits;
  CtorInits.push_back(ConstantInt::get(Int32Type, 0));
  CtorInits.push_back(global_init);
  StructType * ST = ConstantStruct::getTypeForElements(CtorInits, false);
  Constant * RuntimeCtorInit = ConstantStruct::get(ST, CtorInits);

  //
  // Get the current set of static global constructors and add the new ctor
  // to the list.
  //
  std::vector<Constant *> CurrentCtors;
  GlobalVariable * GVCtor = module.getNamedGlobal ("llvm.global_ctors");
  if (GVCtor) {
    if (Constant * C = GVCtor->getInitializer()) {
      for (unsigned index = 0; index < C->getNumOperands(); ++index) {
        CurrentCtors.push_back (dyn_cast<Constant>(C->getOperand (index)));
      }
    }
  }
  CurrentCtors.push_back(RuntimeCtorInit);

  //
  // Create a new initializer.
  //
  ArrayType * AT = ArrayType::get (RuntimeCtorInit-> getType(),
                                   CurrentCtors.size());
  Constant * NewInit = ConstantArray::get (AT, CurrentCtors);

  //
  // Create the new llvm.global_ctors global variable and remove the old one
  // if it existed.
  //
  Value * newGVCtor = new GlobalVariable (module,
                                          NewInit->getType(),
                                          false,
                                          GlobalValue::AppendingLinkage,
                                          NewInit,
                                          "llvm.global_ctors");
  if (GVCtor) {
    newGVCtor->takeName (GVCtor);
    GVCtor->eraseFromParent ();
  }
}

void Nova::GetAnnotatedVariables(Module &M, GlobalStateRef gs) {
    Value *var0, *var1;
    ConstantArray *ca;
    ConstantStruct *cs;
    unsigned num, i;

    for (Module::global_iterator I = M.global_begin(), E = M.global_end(); I != E; ++I) {
        if (I->getName() == "llvm.global.annotations") {
            var0 = cast<Value>(I->getOperand(0));
            if (var0->getValueID() == Value::ConstantArrayVal) {
                ca = cast<ConstantArray>(var0);
                num = ca->getNumOperands();
                for (i = 0; i < num; i++) {
                    cs = cast<ConstantStruct>(ca->getOperand(i));
                    var1 = cs->getOperand(0)->getOperand(0);
                    gs->senVarSet->insert(var1);
                    errs()<<"llvm.global.annotations: var" << i << " : " << var1->getName() << "\n";
                }
            }
        }
    }

    return;
}

bool Nova::SkipStructType(Type *type) {
	StructType *st = cast<StructType>(type);

        //errs() << "struct type name: " << type->getStructName() << "\n";
	assert(type != NULL);

	if (!(st->isLiteral()) && type->getStructName().str().compare(0,10,"struct.IO_")) {
        	// errs() << "note! rely on external struct _IO_FILE/_IO_marker, 
                // only stop creating nested AliasObject for such type!\n";
		return true;
	} else
		return false;
}

void Nova::InitializeGS(GlobalStateRef gs, Module &M) {
    GlobalValue *gv;
    Type *type = NULL;
    AliasObjectRef aor = NULL;
    AliasObjectTupleRef aot = NULL;
    TupleSet *ts = NULL;
    InstSet *is = NULL;

    //errs() <<__func__<<" : \n";

    for (Module::global_iterator s = M.global_begin(), 			\
          e = M.global_end(); s != e; ++s) {
        gv = &(*s);

	// filter out .str***, stderr
	//if (SkipGlobalValue(gv))
	//	continue;

        type = gv->getType();
        //errs() << "GlobalValue gv typeID: " << type->getTypeID() << "\n";
        //errs() << "GlobalValue gv pointee typeID: " << type->getPointerElementType()->getTypeID() << "\n";

        aor = CreateAliasObject(type->getPointerElementType(), gv);

        // create alias object tuple
        aot = new struct AliasObjectTuple();
        aot->offset = 0;
        aot->ao = aor;

        // create tuple set
        ts = new TupleSet();
        ts->insert(aot);

        // add ele into points to map
        (*(gs->pMap))[gv] = ts;

        // initialize global taint map
        is = new InstSet();
        (*(gs->tMap))[gv] = is;
    }

    return;
}

void Nova::ReverseSCC(std::vector<SCCRef> &sccVector, Function *f) {
    for (scc_iterator<Function *> I = scc_begin(f), 
                                  IE = scc_end(f);
                                  I != IE; ++I) {
        const std::vector<BasicBlock *> &SCCBBs = *I;
        SCCRef tmpSCC = new SCC();

        for (SCC::const_iterator BBI = SCCBBs.begin(),
                                 BBIE = SCCBBs.end();
                                 BBI != BBIE; ++BBI) {
            tmpSCC->push_back(*BBI);
        }

        reverse(tmpSCC->begin(), tmpSCC->end());
        sccVector.push_back(tmpSCC);
    }
    
    reverse(sccVector.begin(), sccVector.end());

    return;
}

uint32_t Nova::LongestUseDefChain(SCC &scc) {
    // TODOO
    return MAX_RUNS;
}

void Nova::HandleLoop(GlobalStateRef gs, SCC &scc) {
    uint32_t i, numRuns = LongestUseDefChain(scc);

    i = 0;
    while (i < numRuns) {
        //errs() << "SCC: ";
        VisitSCC(gs, scc);
        //errs() << "\n";
        i++;
    }

    return;
}

Function *Nova::ResolveCall(GlobalStateRef gs, CallInst &I) {
    // TODOO indirect call should be handled later
    if ((*(gs->vMap))[&I])
        return NULL;
    else {
        // mark f's callsite as visited
        (*(gs->vMap))[&I] = true;
        return I.getCalledFunction();
    }
}

void Nova::InitializeFunction(GlobalStateRef gs, Function *f, CallInst &I) {
    Value *var;
    TupleSet *ts = NULL;
    InstSet *is = NULL;

    //errs() <<__func__<<" : \n";

    // record current callinst to help handle ret inst
    gs->ci = &I;

    CallInst::op_iterator argit = I.arg_begin(), argie = I.arg_end();
    Function::arg_iterator it = f->arg_begin(), ie = f->arg_end();
    for (;(argit != argie) && (it != ie); ++it, ++argit) {
        var = cast<Value>(&(*it));

        // copy args'tMap and pMap to funciton params' one by one
        if (gs->pMap->find(*argit) != gs->pMap->end()) {
            if (gs->pMap->find(*argit) == gs->pMap->end()) {
                continue;
            }
            if (gs->tMap->find(*argit) == gs->tMap->end()) {
                continue;
            }

            ts = (*(gs->pMap))[*argit];
            (*(gs->pMap))[var] = ts;

            is = (*(gs->tMap))[*argit];
            (*(gs->tMap))[var] = is;
        }
    }

    return;
}

void Nova::HandleCall(GlobalStateRef gs, CallInst &I) {
    Value *var;
    Function *f;

    f = ResolveCall(gs, I);

    if (f != NULL && f->getName() == "llvm.var.annotation") {
        //llvm.var.annotation(arg0, arg1, arg2, arg3), arg0 is the annotated variable
        var = I.getArgOperand(0);
        gs->senVarSet->insert(var);
        errs() << "llvm.var.annotation: arg0: " << var->getName() << " \n";
    } else if (f != NULL) {
        //errs() << "\ncall inst: " << I << "\n";
        //errs() << "called func: " << f->getName() << "\n";

        // handle parameters and global variables
        if (!f->isDeclaration()) {
            InitializeFunction(gs, f, I);
            Traversal(gs, f);
        }
    }
}

AliasObject *Nova::CreateAliasObject(Type *type, Value *v) {
    unsigned i;
    Type *subtype;
    AliasObjectRef loc = NULL;
    AliasObjectRef aor = NULL, ao = NULL;
    AliasObjectSet *aos = NULL;

    //errs() <<__func__<<" value: "<< *v << " name: " << v->getName() << " typeID: "<< type->getTypeID() << "\n";

    assert(type != NULL);

    // create a alias object 
    aor = new struct AliasObject();
    aor->val = v;
    aor->type = type;
    aor->isStruct = type->isStructTy();
    aor->isLocation = false;
    aor->aliasMap = new AliasMap();
    aor->taintMap = new LocalTaintMap();

    if (type->isStructTy() && SkipStructType(type)) {
    	aor->isStruct = false;
    }

    if (type->isStructTy() && !SkipStructType(type)) {

        i = 0;
        for (Type::subtype_iterator it = type->subtype_begin(), ie = type->subtype_end();
                                                                it != ie; ++it, ++i) {
            // alias object set
            aos = new AliasObjectSet();

            // recursively create aliasobject for struct type
            //errs() <<__func__<<" struct field: "<<(*it)->getTypeID()<<"\n";
            ao = CreateAliasObject(*it, v);
            aos->insert(ao);

            // initialize aliasMap at loc i 
            (*(aor->aliasMap))[i] = aos;
        }
    } else if (type->isPointerTy()) {
            // alias object set
            aos = new AliasObjectSet();

            // recursively create aliasobject for pointer type
            subtype = type->getPointerElementType();
            ao = CreateAliasObject(subtype, v);
            aos->insert(ao);

            // initialize aliasMap at loc 0 
            (*(aor->aliasMap))[0] = aos;
    } else {
        // alias object set
        aos = new AliasObjectSet();

        // create a location object for stack var 
        loc = new struct AliasObject();
        loc->val = v;
        loc->type = NULL;
        loc->isStruct = false;
        loc->isLocation = true;
        loc->aliasMap = NULL;
        loc->taintMap = NULL;

        aos->insert(loc);

        // initialize aliasMap at loc 0
        (*(aor->aliasMap))[0] = aos;
    }

    return aor;
}

void Nova::PrintAliasObject(AliasObjectRef ao) {
    unsigned i, size;

    if (ao->aliasMap == NULL) {
        errs() << __func__ << ": aliasMap is NULL\n";
        return;
    }

    if (ao->isStruct) {
        //errs() << __func__ << ":" << "struct  " << ao->type->getTypeID() << " { " << "\n"; 
        size = ao->aliasMap->size();
        for (i = 0; i < size; i++) {
            PrintAliasObject((*((*(ao->aliasMap))[i]))[0]);
        }
        //errs() << __func__ << ":" << "}\n";
    } else if (ao->type->isPointerTy()) {
            //errs() << __func__ << ":" << "typeID: " << ao->type->getTypeID() << "\n"; 
            PrintAliasObject((*((*(ao->aliasMap))[0]))[0]);
    } else {
            //errs() << __func__ << ":" << "typeID: " << ao->type->getTypeID() << "\n"; 
    }
}

void Nova::UpdatePtoAlloca(GlobalStateRef gs, Instruction &I){
    AllocaInst *ai = NULL;
    Type *type = NULL;
    AliasObjectRef aor = NULL;
    AliasObjectTupleRef aot = NULL;
    TupleSet *ts = NULL;

    //errs() <<__func__<<" : "<<I<<"\n";

    assert(ai = cast<AllocaInst>(&I));
    assert(ai != NULL);
    
    type = ai->getAllocatedType();
    //errs() << "AllocaInst typeID: " << ai->getType()->getTypeID() << "\n";
    //errs() << "AllocaInst pointee typeID: " << ai->getType()->getPointerElementType()->getTypeID() << "\n";
    //errs() << "AllocaInst allocated typeID: " << type->getTypeID() << "\n";

    aor = CreateAliasObject(type, &I); 

    // debug
    PrintAliasObject(aor);

    // create alias object tuple
    aot = new struct AliasObjectTuple();
    aot->offset = 0;
    aot->ao = aor;

    // create tuple set
    ts = new TupleSet();
    ts->insert(aot);

    // add ele into points to map
    (*(gs->pMap))[&I] = ts;

    return;
}

void Nova::UpdatePtoBinOp(GlobalStateRef gs, Instruction &I){
    TupleSet *ts1, *ts2, *ts;
    Value *op1, *op2;

    //errs() <<__func__<<" : "<<I<<"\n";

    op1 = I.getOperand(0);
    op2 = I.getOperand(1);

    // get tuple set for op1
    if (gs->pMap->find(op1) != gs->pMap->end()) {
        ts1 = (*(gs->pMap))[op1];
    } else {
        ts1 = NULL;
    }

    // get tuple set for op2
    if (gs->pMap->find(op2) != gs->pMap->end()) {
        ts2 = (*(gs->pMap))[op2];
    } else {
        ts2 = NULL;
    }

    // new tuple set is for v
    if (gs->pMap->find(&I) != gs->pMap->end()) {
        ts = (*(gs->pMap))[&I];
    } else {
        ts = new TupleSet();
        (*(gs->pMap))[&I] = ts;
    }

    // merge ts1, ts2 into ts
    if (ts1 != NULL) {
        for (TupleSet::iterator it = ts1->begin(), ie = ts1->end();
                                    it != ie; ++it) {
            ts->insert(*it);
        }
    }

    if (ts2 != NULL) {
        for (TupleSet::iterator it = ts2->begin(), ie = ts2->end();
                                    it != ie; ++it) {
            ts->insert(*it);
        }
    }

    return;
}

void Nova::UpdatePtoLoad(GlobalStateRef gs, Instruction &I){
    LoadInst *li;
    Value *op;
    TupleSet *ts, *nts;
    AliasObjectTupleRef aot;
    AliasObjectSet *aos;
    AliasMapRef aliasMap;
    std::stringstream stream;
    uint32_t offset;

    //errs() <<__func__<<" : "<<I<<"\n";

    // get operand
    assert(li = cast<LoadInst>(&I));
    op = li->getPointerOperand();

    // get tupleset
    if (gs->pMap->find(op) == gs->pMap->end()) {
         return;
    }
    ts = (*(gs->pMap))[op];
    if (ts == NULL)
        return;
    
    // create new tupleset for loadinst, llvm IR is in ssa form, so every load defines a new tmp var
    nts = new TupleSet();
    for (TupleSet::iterator tsit = ts->begin(), tsie = ts->end();
                                                tsit != tsie; ++tsit) {
        aliasMap = (*tsit)->ao->aliasMap;
        offset = (*tsit)->offset;

        //assert(aliasMap->find(offset) != aliasMap->end());
        //errs() << __func__ << "(*tsit)->offset:" << (*tsit)->offset << "\n";
        //errs() << __func__ <<": aliasMap :" << aliasMap<<"\n";
        if (aliasMap == NULL) {
            errs() << "aliasMap is NULL\n";
            return;
        }

        if (aliasMap == nullptr || aliasMap->find(offset) == aliasMap->end()) {
            return;
        }
    
        // create a new tuple
        aos = (*aliasMap)[offset];
        for(AliasObjectSet::iterator aosit = aos->begin(), aosie = aos->end();
                                                           aosit != aosie; ++aosit) {
            aot = new struct AliasObjectTuple();
            aot->offset = 0;
            aot->ao = (*aosit);

            // insert new tuple into a new tupleset 
            nts->insert(aot);
        }
    } 

    // add ele into points to map
    (*(gs->pMap))[&I] = nts;

    return;
}

void Nova::HandlePtoGEPOperator(GlobalStateRef gs, GEPOperator *gop) {
    Value *op, *idx1, *idx2;
    uint32_t i, off;
    TupleSet *ts, *nts;
    AliasObjectTupleRef aot;

    // get operand
    op = gop->getPointerOperand();
    //errs() << __func__ << " op: " << *op << "\n";
    //errs() << __func__ << " op typeid: " << op->getType()->getTypeID() << "\n";
    //errs() << __func__ << " op pointee typeid: " << op->getType()->getPointerElementType()->getTypeID() << "\n";

    // getelementptr inst is fairly complex, we only handle basic struct type and array here
    // for struct: we go into the struct to fetch the aliasobject at offset, it's possible
    //             because we create aliasobject for members of struct, refer to  CreateAliasObject
    // for array: we don't go into the array elements, cause we didn't create aliasobject for each
    //            element because of the possible overhead. we treat array as a whole object. we
    //            comprimise precision for lower overhead.
    i = 0;
    idx1 = NULL;
    idx2 = NULL;
    for(User::op_iterator it = gop->idx_begin(), ie = gop->idx_end();
                                                it != ie; ++it, ++i) {
        // idx = *it;
        if (i == 0)
            idx1 = *it;
        else if (i == 1)
            idx2 = *it;
        //errs() << "idx : " << *idx << "\n";
    }

    // TODOO :just a rough handling of offset 
    //assert (isa<ConstantInt>(idx1));
    if (!isa<ConstantInt>(idx1)) {
        return;
    }

    off = 0;
    if (idx2 != NULL && isa<ConstantInt>(idx2)) {
        off = (uint32_t)((cast<ConstantInt>(idx2))->getZExtValue());
        //errs() << "off: " << off << "\n";
    }

    // get tupleset
    if (gs->pMap->find(op) == gs->pMap->end()) {
        //errs() << __func__ << " error: gd->pMap->find(op) == gs->pMap->end(), give up trying!" << "\n";
        return;
    }

    ts = (*(gs->pMap))[op];

    if (ts == NULL)
        return;
    assert(op->getType()->isPointerTy());
    if (op->getType()->getPointerElementType()->isArrayTy()) {
        // for array type, we copy op's ts into gop's ts
        //errs() << __func__ << ": handle array " << "\n";

        // add ele into points to map
        (*(gs->pMap))[gop] = ts;

    } else if (op->getType()->getPointerElementType()->isStructTy()) {
        // for struct type, we fetch struct member and put it into gop's ts
        //errs() << __func__ << ": handle struct" << "\n";

        // create new tupleset for gep
        // NOTE: we don't dereference op here, op should always be the start address of the struct
        nts = new TupleSet();
        for (TupleSet::iterator tsit = ts->begin(), tsie = ts->end();
                                                    tsit != tsie; ++tsit) {
            aot = new struct AliasObjectTuple();
            aot->offset = off;
            aot->ao = (*tsit)->ao;

            // debug
            //errs() << __func__ << ": new tuple : off = " << off << " ao->val: " << (*tsit)->ao->val <<"\n";
            // print op'ao
            PrintAliasObject((*tsit)->ao);

            // insert new tuple into a new tupleset 
            nts->insert(aot);
        } 

        // add ele into points to map
        (*(gs->pMap))[gop] = nts;
    }

    return;
}

void Nova::UpdatePtoStore(GlobalStateRef gs, Instruction &I){
    StoreInst *si;
    GEPOperator *gepop;
    Value *op, *v;
    TupleSet *ots, *vts;
    AliasObjectSet *aos, *vaos;
    AliasMapRef aliasMap;
    AliasObjectRef ao;
    uint32_t offset;

    //errs() <<__func__<<" : "<<I<<"\n";

    // get operand
    assert(si = cast<StoreInst>(&I));
    op = si->getPointerOperand();
    v = si->getValueOperand();

    // if v is constant integer or function parameter, nothing to do here
    if (gs->pMap->find(v) == gs->pMap->end()) {
        //errs() << "val operand has not pMap entry, so it is constant or funciton prarmeter!\n";
        return;
    }

    // if op is gepoperator, call HandlePtoGEPOperator first
    if (isa<GEPOperator>(op)) {
        //errs() << "op operand is GEPOperator!\n";
        gepop = dyn_cast<GEPOperator>(op);
        HandlePtoGEPOperator(gs, gepop);
    }

    // get tupleset
    if (gs->pMap->find(op) == gs->pMap->end()) {
        errs() << __func__ << ": gs->pMap->find(op) can't find it!\n";
        return;
    }

    ots = (*(gs->pMap))[op];
    vts = (*(gs->pMap))[v];

    if (ots == NULL || vts == NULL) { 
        errs() << __func__ << ":" << "ots or vts is NULL";
        return;
    }
    
    // update aliasobject points-to by operand op

    // get operand v's AliasObject set
    vaos = new AliasObjectSet();
    for (TupleSet::iterator vtsit = vts->begin(), vtsie = vts->end();
                                                vtsit != vtsie; ++vtsit) {
        ao = (*vtsit)->ao;
        vaos->insert(ao);
    }


    // get operand op's value's AliasObject set, note, we dereference op here
    for (TupleSet::iterator otsit = ots->begin(), otsie = ots->end();
                                                otsit != otsie; ++otsit) {
        aliasMap = (*otsit)->ao->aliasMap;
        offset = (*otsit)->offset;

        //errs() << __func__ << ": tuple : off = " << offset << " ao->val: " << *((*otsit)->ao->val) <<"\n";
        // print op'ao
        PrintAliasObject((*otsit)->ao);

        if (aliasMap == NULL) {
            errs() << __func__ << ": aliasMap is Null\n";
            return;
        }
        assert(aliasMap);
        //assert(aliasMap->find(offset) != aliasMap->end());
        if (aliasMap->find(offset) == aliasMap->end()) {
           return;
        }

        // copy v's aliasobject into op's value's aliasobject
        // TODOO we lose the v's ao'offset info here
        aos = (*aliasMap)[offset];
        for(AliasObjectSet::iterator aosit = vaos->begin(), aosie = vaos->end();
                                                           aosit != aosie; ++aosit) {
            aos->insert(*aosit);
        }
    } 

    // release allocated tempary objects
    delete vaos;

    return;
}

void Nova::UpdatePtoGEP(GlobalStateRef gs, Instruction &I){
    GetElementPtrInst *gepi;
    Value *op, *idx2;
    uint32_t i, off;
    TupleSet *ts, *nts;
    AliasObjectTupleRef aot;

    //errs() <<__func__<<" : "<<I<<"\n";

    // get operand
    assert(gepi = cast<GetElementPtrInst>(&I));
    op = gepi->getPointerOperand();
    //errs() << " gep address space :" <<gepi->getAddressSpace() << "\n";

    i = 0;
    idx2 = NULL;
    for(User::op_iterator it = gepi->idx_begin(), ie = gepi->idx_end();
                                                it != ie; ++it, ++i) {
        // idx = *it;
        if (i == 1)
            idx2 = *it;
        //errs() << "idx : " << *idx << "\n";
    }

    //errs() << "idx1:" << (uint64_t)idx1 << "\n";

    // TODOO :just a rough handling of offset 
    //assert (isa<ConstantInt>(idx1));
    //if (!isa<ConstantInt>(idx1)) {
    //    errs() << __func__ << " skip instruction : " << I << "\n"; 
    //    return;
    //}

    off = 0;
    //errs() << "idx2:" << (uint64_t)idx2 << "\n";
    if(idx2 != NULL && isa<ConstantInt>(idx2)) {
        off = (uint32_t)((cast<ConstantInt>(idx2))->getZExtValue());
        //errs() << "off: " << off << "\n";
    }

    // get tupleset
    //assert(gs->pMap->find(op) != gs->pMap->end());
    if (gs->pMap->find(op) == gs->pMap->end()) {
        //errs() << __func__ << " skip instruction : " << I << "\n"; 
        return;
    }
    ts = (*(gs->pMap))[op];

    if (ts == NULL)
        return;

    assert(op->getType()->isPointerTy());

    if (op->getType()->getPointerElementType()->isArrayTy()) {
        // for array type, we copy op's ts into gop's ts
        //errs() << __func__ << ": handle array " << "\n";

        // add ele into points to map
        (*(gs->pMap))[&I] = ts;

    } else if (op->getType()->getPointerElementType()->isStructTy()) {
        // for struct type, we fetch struct member and put it into gop's ts
        //errs() << __func__ << ": handle struct" << "\n";

        // create new tupleset for gep
        // NOTE: we don't dereference op here, op should always be the start address of the struct
        nts = new TupleSet();
        for (TupleSet::iterator tsit = ts->begin(), tsie = ts->end();
                                                    tsit != tsie; ++tsit) {
            aot = new struct AliasObjectTuple();
            aot->offset = off;
            aot->ao = (*tsit)->ao;

            // debug
            //errs() << __func__ << ": new tuple : off = " << off << " ao->val: " << (*tsit)->ao->val <<"\n";
            // print op'ao
            PrintAliasObject((*tsit)->ao);

            // insert new tuple into a new tupleset 
            nts->insert(aot);
        } 

        // add ele into points to map
        (*(gs->pMap))[&I] = nts;
    } else {
        // errs() << __func__ << " nearly skip instruction : " << I << "\n"; 
        (*(gs->pMap))[&I] = ts;
    }
    
    return;
}

void Nova::UpdatePtoRet(GlobalStateRef gs, Instruction &I){
    ReturnInst *ri;
    Value *ret, *ci;
    TupleSet *ts = NULL, *nts = NULL;

    // get operand
    assert(ri = cast<ReturnInst>(&I));
    ret = ri->getReturnValue();

    // get tuple set
    if (gs->pMap->find(ret) != gs->pMap->end())
        ts = (*(gs->pMap))[ret];

    // get current callInst and the corresponding tuple set
    ci = gs->ci;
    if (gs->pMap->find(ci) != gs->pMap->end())
        nts = (*(gs->pMap))[ci];

    if (ts != NULL) {
        if (nts != NULL) {
            // merge ts and nts
            for (TupleSet::iterator it = ts->begin(), ie = ts->end();
                                            it != ie; ++it ) {
                nts->insert(*it);
            }
        } else {
            // assign ts to ci's points-to map, thus ci will carry the ret's info back to caller
            (*(gs->pMap))[ci] = ts;
        }
    }

    return;
}

void Nova::UpdatePtoBitCast(GlobalStateRef gs, Instruction &I){
    BitCastInst *bci;
    Value *op;
    TupleSet *ts = NULL;

    // get operand
    assert(bci = cast<BitCastInst>(&I));
    op = bci->getOperand(0);

    // get tuple set
    if (gs->pMap->find(op) != gs->pMap->end())
        ts = (*(gs->pMap))[op];

    // for cast inst, the target and src share the same tuple set
    if (gs->pMap->find(bci) != gs->pMap->end()) {
        //errs() << __func__ <<" pMap(bci) is not null : " << I << "\n";
    }

    (*(gs->pMap))[bci] = ts;
}

void Nova::UpdateTaintAlloca(GlobalStateRef gs, Instruction &I){
    // TODOO
    //errs() <<__func__<<" : "<<I<<"\n";
}

void Nova::UpdateTaintBinOp(GlobalStateRef gs, Instruction &I){
    InstSet *is1, *is2, *is;
    Value *op1, *op2;

    //errs() <<__func__<<" : "<<I<<"\n";

    op1 = I.getOperand(0);
    op2 = I.getOperand(1);

    // get instset for op1
    if (gs->tMap->find(op1) != gs->tMap->end()) {
        is1 = (*(gs->tMap))[op1];
    } else {
        is1 = NULL;
    }

    // get instset for op2
    if (gs->tMap->find(op2) != gs->tMap->end()) {
        is2 = (*(gs->tMap))[op2];
    } else {
        is2 = NULL;
    }

    // new instset is for v
    if (gs->tMap->find(&I) != gs->tMap->end()) {
        is = (*(gs->tMap))[&I];
    } else {
        is = new InstSet();
        (*(gs->tMap))[&I] = is;
    }

    // merge is1, is2 into is
    if (is1 != NULL) {
        for (InstSet::iterator it = is1->begin(), ie = is1->end();
                                    it != ie; ++it) {
            is->insert(*it);
        }
    }

    if (is2 != NULL) {
        for (InstSet::iterator it = is2->begin(), ie = is2->end();
                                    it != ie; ++it) {
            is->insert(*it);
        }
    }

    is->insert(&I);


    return;
}

void Nova::UpdateTaintLoad(GlobalStateRef gs, Instruction &I){
    LoadInst *li;
    Value *op;
    TupleSet *ts;
    LocalTaintMapRef taintMap;
    InstSet *is, *bis, *obis;
    uint32_t offset;

    //errs() <<__func__<<" : "<<I<<"\n";

    // get operand
    assert(li = cast<LoadInst>(&I));
    op = li->getPointerOperand();

    // get tupleset
    //assert(gs->pMap->find(op) != gs->pMap->end());
    if (gs->pMap->find(op) == gs->pMap->end()) {
        return;
    }
    ts = (*(gs->pMap))[op];
    if (ts == NULL)
        return;
    
    // collect local instset from all aliasobject's local taintmap, put them into big instset bis
    bis = new InstSet();
    for (TupleSet::iterator tsit = ts->begin(), tsie = ts->end();
                                                tsit != tsie; ++tsit) {
        taintMap = (*tsit)->ao->taintMap;
        offset = (*tsit)->offset;
        if (taintMap == NULL) {
            errs() <<__func__ <<  ":tainMap is NULL\n";
            return;
        }
        // if there's no taintMap at offset , create one
        if (taintMap->find(offset) != taintMap->end()) {
            is = (*taintMap)[offset];
        } else {
            is = new InstSet();
            (*taintMap)[offset] = is;
        }

        // iterate over local taintmap's instset is
        for(InstSet::iterator it = is->begin(), ie = is->end();
                                                it != ie; ++it) {
           bis->insert(*it);
        }
    }

    bis->insert(&I);

    // merge old bis with new bis
    if (gs->tMap->find(&I) != gs->tMap->end()) {
        obis = (*(gs->tMap))[&I];
        for (InstSet::iterator it = obis->begin(), ie = obis->end();
                                                it != ie; ++it) {
            bis->insert(*it);
        }
    } 

    // add (I, bis) into taint map
    (*(gs->tMap))[&I] = bis;

    return;
}

void Nova::UpdateTaintStore(GlobalStateRef gs, Instruction &I) {
    StoreInst *si;
    Value *v, *op;
    GEPOperator *gepop;
    TupleSet *ts;
    LocalTaintMapRef taintMap;
    InstSet *vis, *is;
    uint32_t offset;

    //errs() <<__func__<<" : "<<I<<"\n";

    // get operand v, op
    assert(si = cast<StoreInst>(&I));
    v = si->getValueOperand();
    op = si->getPointerOperand();

    // get v's taint map
    if (gs->tMap->find(v) != gs->tMap->end()) {
            vis = (*(gs->tMap))[v];
    } else {
        //errs() << __func__ << " : operand val's no tMap, it may be constant value or constant parameter!\n";
        vis = NULL;
    }

    // if op is gepoperator, call HandlePtoGEPOperator first
    if (isa<GEPOperator>(op)) {
        //errs() << "op operand is GEPOperator!\n";
        gepop = dyn_cast<GEPOperator>(op);
        HandlePtoGEPOperator(gs, gepop);
    }
    
    // get tupleset
    if (gs->pMap->find(op) == gs->pMap->end()) {
        //errs() << __func__ << " error: gd->pMap->find(op) == gs->pMap->end(), give up trying!" << "\n";
        return;
    }
    ts = (*(gs->pMap))[op];
    if (ts == NULL)
        return;

    // spread v's taint map to all op's aliasobject's local taint map
    for (TupleSet::iterator tsit = ts->begin(), tsie = ts->end();
                                                tsit != tsie; ++tsit) {
        taintMap = (*tsit)->ao->taintMap;
        offset = (*tsit)->offset;
        if (taintMap == NULL) {
             errs() << "taintMap is NULL\n";
            return;
        } 

        // if taintMap at ao'offset does not exist, create one.
        assert(taintMap != NULL);
        if (taintMap->find(offset) == taintMap->end()) {
            is = new InstSet();
            (*taintMap)[offset] = is;
        } else {
            is = (*taintMap)[offset];
        }

        // merge v's taintmap into op's aliasobject's local taintmap at offset
        if (vis != NULL) {
            for(InstSet::iterator it = vis->begin(), ie = vis->end();
                                                    it != ie; ++it) {
                is->insert(*it);
            }
        }

        // include this storeinst I
        is->insert(&I);
    }

    return;
}

void Nova::UpdateTaintGEP(GlobalStateRef gs, Instruction &I) {
    GetElementPtrInst *gepi;
    Value *op;
    InstSet *is1, *is;

    //errs() <<__func__<<" : "<<I<<"\n";

    // get operand
    assert(gepi = cast<GetElementPtrInst>(&I));
    op = gepi->getPointerOperand();

    // get instset for op
    if (gs->tMap->find(op) != gs->tMap->end()) {
        is1 = (*(gs->tMap))[op];
    } else {
        is1 = NULL;
    }

    // new instset is for v
    is = new InstSet();

    // merge is1, is2 into is
    if (is1 != NULL) {
        for (InstSet::iterator it = is1->begin(), ie = is1->end();
                                    it != ie; ++it) {
            is->insert(*it);
        }
    }

    // don't forget this GEP inst
    is->insert(&I);

    // add (I, is) into taint map
    //assert(gs->tMap->find(&I) == gs->tMap->end());
    (*(gs->tMap))[&I] = is;

    return;
}

void Nova::UpdateTaintRet(GlobalStateRef gs, Instruction &I){
    ReturnInst *ri;
    Value *ret, *ci;
    InstSet *is = NULL, *nis = NULL; // NOTE! we must initialize stack variable, it might has non-zero initial value!!!

    // get operand
    assert(ri = cast<ReturnInst>(&I));
    ret = ri->getReturnValue();

    // get ret's inst set
    if (gs->tMap->find(ret) != gs->tMap->end())
        is = (*(gs->tMap))[ret];

    // get current callInst and the corresponding tuple set
    ci = gs->ci;
    if (gs->tMap->find(ci) != gs->tMap->end())
        nis = (*(gs->tMap))[ci];

    if (is != NULL) {
        if (nis != NULL) {
            // merge is and nis
            for (InstSet::iterator it = is->begin(), ie = is->end();
                                            it != ie; ++it ) {
                nis->insert(*it);
            }
        } else {
            // assign ts to ci's points-to map, thus ci will carry the ret's info back to caller
            (*(gs->tMap))[ci] = is;
        }
    }

    return;
}

void Nova::UpdateTaintBitCast(GlobalStateRef gs, Instruction &I){
    BitCastInst *bci;
    Value *op;
    InstSet *is = NULL;

    // get operand
    assert(bci = cast<BitCastInst>(&I));
    op = bci->getOperand(0);

    // get inst set
    if (gs->tMap->find(op) != gs->tMap->end())
        is = (*(gs->tMap))[op];
    else
        is = new InstSet();

    is->insert(&I);

    // for cast inst, the target and src share the same tuple set
    if (gs->tMap->find(bci) != gs->tMap->end()) {
        //errs() << __func__ <<" tMap(bci) is not null : " << I << "\n";
    }

    (*(gs->tMap))[bci] = is;
}

void Nova::PointsToAnalysis(GlobalStateRef gs, Instruction &I) {
    if (isa<AllocaInst>(&I)) {
        UpdatePtoAlloca(gs, I);
    } else if (I.isBinaryOp()){
        UpdatePtoBinOp(gs, I);
    } else if (isa<LoadInst>(&I)){
        UpdatePtoLoad(gs, I);
    } else if (isa<StoreInst>(&I)){
        UpdatePtoStore(gs, I);
    } else if (isa<GetElementPtrInst>(&I)){
        UpdatePtoGEP(gs, I);
    } else if (isa<ReturnInst>(&I)){
        UpdatePtoRet(gs, I);
    } else if (isa<BitCastInst>(&I)){
        UpdatePtoBitCast(gs, I);
    } else {
        // Not handled inst
        //errs() <<"Unhandled Inst: "<<I<<"\n";
    }
}

void Nova::TaintAnalysis(GlobalStateRef gs, Instruction &I) {
    if (isa<AllocaInst>(&I)) {
        UpdateTaintAlloca(gs, I);
    } else if (I.isBinaryOp()){
        UpdateTaintBinOp(gs, I);
    } else if (isa<LoadInst>(&I)){
        UpdateTaintLoad(gs, I);
    } else if (isa<StoreInst>(&I)){
        UpdateTaintStore(gs, I);
    } else if (isa<GetElementPtrInst>(&I)){
        UpdateTaintGEP(gs, I);
    } else if (isa<ReturnInst>(&I)){
        UpdateTaintRet(gs, I);
    } else if (isa<BitCastInst>(&I)){
        UpdateTaintBitCast(gs, I);
    } else {
        // Not handled inst
        //errs() <<"Unhandled Inst: "<<I<<"\n";
    }
}

void Nova::DispatchClients(GlobalStateRef gs, Instruction &I) {
    PointsToAnalysis(gs, I);
    TaintAnalysis(gs, I);
}

void Nova::VisitSCC(GlobalStateRef gs, SCC &scc) {
    for (SCC::iterator BBI = scc.begin(),
                       BBIE = scc.end();
                       BBI != BBIE; ++BBI) {
        errs() << (*BBI)->getName() << " ";
        for (Instruction &I: *(*BBI)) {
            if (auto *callInst = dyn_cast<CallInst>(&I)) {
                HandleCall(gs, *callInst);
            } else {
                DispatchClients(gs, I);
            }
        }
    }
    return;
}

void Nova::Traversal(GlobalStateRef gs, Function *f) {
    std::vector<SCCRef> sccVector;

    ReverseSCC(sccVector, f);

    for (std::vector<SCCRef>::iterator it = sccVector.begin(), 
                                       ie = sccVector.end();
                                       it != ie; ++it) {
        // Is loop ?
        if ((*it)->size() > 1) {
            HandleLoop(gs, *(*it));
        } else {
            //errs() << "SCC: ";
            VisitSCC(gs, *(*it));
            //errs() << "\n";
        }
    }

    return;
}

void Nova::PrintPointsToMap(GlobalStateRef gs) {
    AliasMapRef aliasMap;
    unsigned offset;
    AliasObjectSet *aos;

    for (PointsToMap::iterator it = gs->pMap->begin(), ie = gs->pMap->end();
                                                         it != ie; ++it) { 
        if ((it)->second == NULL) {
            errs() << "it->second is NULL!" << "\n";
            continue;
        }

        errs() << it->first->getName() << " : " << "\n";
        for (TupleSet::iterator tsit = it->second->begin(), tsie = it->second->end();
                                                    tsit != tsie; ++tsit) {
            if ((*tsit)->ao == NULL) {
                errs() << "tsit->ao is NULL!" << "\n";
                continue;
            }

            errs() << "(" << (*tsit)->offset << ", " << (*tsit)->ao->val->getName() << ")" << "\n";
            aliasMap = (*tsit)->ao->aliasMap;
            offset = (*tsit)->offset;
            if (aliasMap != NULL && aliasMap->find(offset) != aliasMap->end()) {
                aos = (*aliasMap)[offset];
                if (aos == NULL) {
                    errs() << "aos is NULL!" << "\n";
                    continue;
                }
                errs() << "alias object set: ";
                for (AliasObjectSet::iterator aosit = aos->begin(), aosie = aos->end();
                                                    aosit != aosie; ++aosit) {
                    errs() << (*aosit)->val->getName() << ",";
                }
                errs() << "\n";
            } else {
                errs() << "location object ?" << "\n";
            }

            if ((*tsit)->ao->type != NULL && (*tsit)->ao->type->isStructTy()) {
                // print struct field aliasMap
                errs() << "struct internal alias map: \n";
                for (AliasMap::iterator ait = aliasMap->begin(), aie = aliasMap->end();
                                                ait != aie; ++ait) {
                    offset = ait->first;
                    aos = ait->second;
                    if (aos == NULL) {
                        errs() << "aos is NULL!" << "\n";
                        continue;
                    }
                    errs() << "alias object set at offset " << offset << " : ";
                    for (AliasObjectSet::iterator aosit = aos->begin(), aosie = aos->end();
                                                        aosit != aosie; ++aosit) {
                        errs() << (*aosit)->val->getName() << ",";
                    }
                    errs() << "\n";
                }
            }
        }
        errs() << "\n";
    }
}

void Nova::PrintTaintMap(GlobalStateRef gs) {
    LocalTaintMapRef taintMap;
    AliasObjectRef ao;
    unsigned offset;
    InstSet *is;

    for (TaintMap::iterator it = gs->tMap->begin(), ie = gs->tMap->end();
                                                        it != ie; ++it) { 
        errs() << it->first->getName() << " : " << "\n";
        for (InstSet::iterator isit = it->second->begin(), isie = it->second->end();
                                                    isit != isie; ++isit) {
            if ((*isit) != NULL)
                errs() << *(*isit) << "\n";
            else {
                errs() << "(*isit) == NULL!\n";
                continue;
            }
        }

        if (gs->pMap->find(it->first) != gs->pMap->end()) {
            if (((*(gs->pMap))[it->first]) == NULL || ((*(gs->pMap))[it->first])->empty()) {
                errs() << "pMap[it->first] == empty !\n";
                ao = NULL;
            } else {
                ao = (*(*(gs->pMap))[it->first])[0]->ao;
            }
        } else {
            ao = NULL;
        }

        if (ao!= NULL) {
            taintMap = ao->taintMap;
        } else {
            taintMap = NULL;
        }

        offset = 0;
        if (taintMap != NULL && taintMap->find(offset) != taintMap->end()) {
            is = (*taintMap)[offset];
            errs() << "alias object local taint trace: \n";
            for (InstSet::iterator iit = is->begin(), iie = is->end();
                                                iit != iie; ++iit) {
                errs() << *(*iit) << "\n";
            }
            errs() << "\n";
        } else {
            errs() << "location object ?" << "\n";
        }

        errs() << "\n";

        if (ao != NULL && taintMap != NULL && ao->type != NULL && ao->type->isStructTy()) {
            // print struct field taintMap
            errs() << "struct internal alias map: \n";
            for (LocalTaintMap::iterator tit = taintMap->begin(), tie = taintMap->end();
                                            tit != tie; ++tit) {
                offset = tit->first;
                is = tit->second;
                errs() << "locat taint map at offset " << offset << " : " << "\n";
                for (InstSet::iterator iit = is->begin(), iie = is->end();
                                                    iit != iie; ++iit) {
                    errs() << *(*iit) << "\n";
                }
                errs() << "\n";
            }
        }
    }
}

// for pointer type, we get boundary info when it's initialized
// it could be initialized by malloc, getAddrOf array type, or others.
// step : find its definition
// step : instrument its definition to collect boundary info
// step : check pointer-based read&write
void Nova::PointerAccessCheck(Module &M, Value *v) {
}

BasicBlock::iterator Nova::GetInstIterator(Value *v) {
    Instruction *inst;
    BasicBlock *bb;

    inst = dyn_cast<Instruction>(v);
    assert(inst && "v should be a instruction!");
    bb = inst->getParent();

    for (BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; ++i) {
        if (&(*i) == inst)
            return i;
    }

    assert( 0 && "Can't find inst in its parent basicblock!");
    return bb->end();
}

//
// Method: getSizeOfType 
// 
// Description: This function returns the size of the memory access
// based on the type of the pointer which is being dereferenced.  This
// function is used to pass the size of the access in many checks to
// perform byte granularity checking.
//
// Comments: May we should use TargetData instead of m_is_64_bit
// according Criswell's comments.
 

Value* Nova::GetSizeOfType(Type* input_type) {

  // Create a Constant Pointer Null of the input type.  Then get a
  // getElementPtr of it with next element access cast it to unsigned
  // int
   
  const PointerType* ptr_type = dyn_cast<PointerType>(input_type);

  if (isa<FunctionType>(ptr_type->getElementType())) {
      return ConstantInt::get(Type::getInt64Ty(ptr_type->getContext()), 0);
  }

  const SequentialType* seq_type = dyn_cast<SequentialType>(input_type);
  Constant* int64_size = NULL;
  if (!seq_type) {
    if(input_type->isSized()){
      return ConstantInt::get(Type::getInt64Ty(input_type->getContext()), 0);
    }
  }
  assert(seq_type && "pointer dereference and it is not a sequential type\n");
  
  StructType* struct_type = dyn_cast<StructType>(input_type);

  if(struct_type){
    if(struct_type->isOpaque()){
        return ConstantInt::get(Type::getInt64Ty(seq_type->getContext()), 0);        
    }
  }
  
  if(!seq_type->getElementType()->isSized()){
    return ConstantInt::get(Type::getInt64Ty(seq_type->getContext()), 0);
  }
  int64_size = ConstantExpr::getSizeOf(seq_type->getElementType());
  return int64_size;
}

// 
// Method: castToVoidPtr()
// **Borrowed from SoftBoundsCETS
//
// Description: 
// 
// This function introduces a bitcast instruction in the IR when an
// input operand that is a pointer type is not of type i8*. This is
// required as all the SoftBound/CETS handlers take i8*s
//

Value* Nova::CastToVoidPtr(Value* operand, Instruction* insert_at) {

  Value* cast_bitcast = operand;
  if (operand->getType() != m_void_ptr_type) {
    cast_bitcast = new BitCastInst(operand, m_void_ptr_type,
                                   "bitcast",
                                   insert_at);
  }
  return cast_bitcast;
}

//
// Method: dissociateBaseBound
// **Borrowed from SoftboundCETS
//
// Description: This function removes the base/bound metadata
// associated with the pointer operand in the SoftBound/CETS maps.

void Nova::DissociateBaseBound(Value* pointer_operand){

  if(m_pointer_base.count(pointer_operand)){
    m_pointer_base.erase(pointer_operand);
  }
  if(m_pointer_bound.count(pointer_operand)){
    m_pointer_bound.erase(pointer_operand);
  }
  assert((m_pointer_base.count(pointer_operand) == 0) && 
         "dissociating base failed\n");
  assert((m_pointer_bound.count(pointer_operand) == 0) && 
         "dissociating bound failed");
}

//
// Method: associateBaseBound
// **Borrowed from SoftboundCETS
//
// Description: This function associates the base bound with the
// pointer operand in the SoftBound/CETS maps.


void Nova::AssociateBaseBound(Value* pointer_operand, 
                              Value* pointer_base, 
                              Value* pointer_bound){

  if(m_pointer_base.count(pointer_operand)){
    DissociateBaseBound(pointer_operand);
  }

  if(pointer_base->getType() != m_void_ptr_type){
    assert(0 && "base does not have a void pointer type ");
  }
  m_pointer_base[pointer_operand] = pointer_base;

  if(m_pointer_bound.count(pointer_operand)){
    assert(0 && "bound map already has an entry in the map");
  }
  if(pointer_bound->getType() != m_void_ptr_type) {
    assert(0 && "bound does not have a void pointer type ");
  }
  m_pointer_bound[pointer_operand] = pointer_bound;
}

//
// Methods: getAssociatedBase, getAssociatedBound, getAssociatedKey,
// getAssociatedLock
//
// Description: Retrieves the metadata from SoftBound/CETS maps 
//

Value* Nova::GetAssociatedBase(Value* pointer_operand) {
    

  if(isa<Constant>(pointer_operand)){
    Value* base = NULL;
    Value* bound = NULL;
    Constant* ptr_constant = dyn_cast<Constant>(pointer_operand);
    GetConstantExprBaseBound(ptr_constant, base, bound);

    if(base->getType() != m_void_ptr_type){
      Constant* base_given_const = dyn_cast<Constant>(base);
      assert(base_given_const!=NULL);
      Constant* base_const = ConstantExpr::getBitCast(base_given_const, m_void_ptr_type);
      return base_const;
    }
    return base;
  }

  if(!m_pointer_base.count(pointer_operand)){
    pointer_operand->dump();
  }
  assert(m_pointer_base.count(pointer_operand) && 
         "Base absent. Try compiling with -simplifycfg option?");
    
  Value* pointer_base = m_pointer_base[pointer_operand];
  assert(pointer_base && "base present in the map but null?");

  if(pointer_base->getType() != m_void_ptr_type)
    assert(0 && "base in the map does not have the right type");

  return pointer_base;
}

Value* Nova::GetAssociatedBound(Value* pointer_operand) {

  if(isa<Constant>(pointer_operand)){
    Value* base = NULL;
    Value* bound = NULL;
    Constant* ptr_constant = dyn_cast<Constant>(pointer_operand);
    GetConstantExprBaseBound(ptr_constant, base, bound);

    if(bound->getType() != m_void_ptr_type){
      Constant* bound_given_const = dyn_cast<Constant>(bound);
      assert(bound_given_const != NULL);
      Constant* bound_const = ConstantExpr::getBitCast(bound_given_const, m_void_ptr_type);
      return bound_const;
    }

    return bound;
  }

    
  assert(m_pointer_bound.count(pointer_operand) && 
         "Bound absent.");
  Value* pointer_bound = m_pointer_bound[pointer_operand];
  assert(pointer_bound && 
         "bound present in the map but null?");    

  if(pointer_bound->getType() != m_void_ptr_type)
    assert(0 && "bound in the map does not have the right type");

  return pointer_bound;
}

Instruction* Nova:: GetGlobalInitInstruction(Module& module){
  Function* global_init_function = module.getFunction("__softboundcets_global_init");    
  assert(global_init_function && "no __softboundcets_global_init function??");    
  Instruction *global_init_terminator = NULL;
  bool return_inst_flag = false;
  for(Function::iterator fi = global_init_function->begin(), fe = global_init_function->end(); fi != fe; ++fi) {
      
    BasicBlock* bb = dyn_cast<BasicBlock>(fi);
    assert(bb && "basic block null");
    Instruction* bb_term = dyn_cast<Instruction>(bb->getTerminator());
    assert(bb_term && "terminator null?");
      
    if(isa<ReturnInst>(bb_term)) {
      assert((return_inst_flag == false) && "has multiple returns?");
      return_inst_flag = true;
      global_init_terminator = dyn_cast<ReturnInst>(bb_term);
      assert(global_init_terminator && "return inst null?");
    }
  }
  assert(global_init_terminator && "global init does not have return, strange");
  return global_init_terminator;
}

// Method: handleGlobalStructTypeInitializer()
//
// Description: handles the global
// initialization for global variables which are of struct type and
// have a pointer as one of their fields and is globally
// initialized 
//
// Comments: This function requires review and rewrite

void Nova::HandleGlobalStructTypeInitializer(Module& module, 
                                  StructType* init_struct_type,
                                  Constant* initializer, 
                                  GlobalVariable* gv, 
                                  std::vector<Constant*> indices_addr_ptr, 
                                  int length) {
  
  // TODOO:URGENT: Do I handle nesxted structures
  //errs() << __func__ <<"\n";
  
  // has zero initializer 
  if(initializer->isNullValue())
    return;
    
  Instruction* first = GetGlobalInitInstruction(module);
  unsigned num_elements = init_struct_type->getNumElements();
  Constant* constant = dyn_cast<Constant>(initializer);
  assert(constant && 
         "[handleGlobalStructTypeInit] global stype with init but not CA?");

  for(unsigned i = 0; i < num_elements ; i++) {
    
    CompositeType* struct_comp_type = 
      dyn_cast<CompositeType>(init_struct_type);
    assert(struct_comp_type && "not a struct type?");
    
    Type* element_type = struct_comp_type->getTypeAtIndex(i);      
    if(isa<PointerType>(element_type)){        
      Value* initializer_opd = constant->getOperand(i);
      Value* operand_base = NULL;
      Value* operand_bound = NULL;
      
      Constant* addr_of_ptr = NULL;
      
      Constant* given_constant = dyn_cast<Constant>(initializer_opd);
      assert(given_constant && 
             "[handleGlobalStructTypeInitializer] not a constant?");
      
      GetConstantExprBaseBound(given_constant, operand_base, operand_bound);   
      
      // Creating the address of ptr
        //      Constant* index1 = 
        //                ConstantInt::get(Type::getInt32Ty(module.getContext()), 0);
      Constant* index2 = ConstantInt::get(Type::getInt32Ty(module.getContext()), i);
      
      //      indices_addr_ptr.push_back(index1);
      indices_addr_ptr.push_back(index2);
      length++;

      addr_of_ptr = ConstantExpr::getGetElementPtr(nullptr, gv, indices_addr_ptr);
      
      Type* initializer_type = initializer_opd->getType();
      Value* initializer_size = GetSizeOfType(initializer_type);     
      AddStoreBaseBoundFunc(addr_of_ptr, operand_base, 
                            operand_bound,initializer_opd, 
                            initializer_size, first);
      
      indices_addr_ptr.pop_back();
      length--;

      continue;
    }     
    if(isa<StructType>(element_type)){
      StructType* child_element_type = 
        dyn_cast<StructType>(element_type);
      Constant* struct_initializer = 
        dyn_cast<Constant>(constant->getOperand(i));      
      Constant* index2 =
        ConstantInt::get(Type::getInt32Ty(module.getContext()), i);
      indices_addr_ptr.push_back(index2);
      length++;
      HandleGlobalStructTypeInitializer(module, child_element_type, 
                                        struct_initializer, gv, 
                                        indices_addr_ptr, length); 
      indices_addr_ptr.pop_back();
      length--;
      continue;
    }
  }
}

//
// Method: getConstantExprBaseBound
//
// Description: This function uniform handles all global constant
// expression and obtains the base and bound for these expressions
// without introducing any extra IR modifications.

void Nova::GetConstantExprBaseBound(Constant* given_constant, 
                                             Value* & tmp_base,
                                             Value* & tmp_bound){


  if(isa<ConstantPointerNull>(given_constant)){
    tmp_base = m_void_null_ptr;
    tmp_bound = m_void_null_ptr;
    return;
  }
  
  ConstantExpr* cexpr = dyn_cast<ConstantExpr>(given_constant);
  tmp_base = NULL;
  tmp_bound = NULL;
    

  if(cexpr) {

    assert(cexpr && "ConstantExpr and Value* is null??");
    switch(cexpr->getOpcode()) {
        
    case Instruction::GetElementPtr:
      {
        Constant* internal_constant = dyn_cast<Constant>(cexpr->getOperand(0));
        GetConstantExprBaseBound(internal_constant, tmp_base, tmp_bound);
        break;
      }
      
    case BitCastInst::BitCast:
      {
        Constant* internal_constant = dyn_cast<Constant>(cexpr->getOperand(0));
        GetConstantExprBaseBound(internal_constant, tmp_base, tmp_bound);
        break;
      }
    case Instruction::IntToPtr:
      {
        tmp_base = m_void_null_ptr;
        tmp_bound = m_void_null_ptr;
        return;
        break;
      }
    default:
      {
        break;
      }
    } // Switch ends
    
  } else {
      
    const PointerType* func_ptr_type = 
      dyn_cast<PointerType>(given_constant->getType());
      
    if(isa<FunctionType>(func_ptr_type->getElementType())) {
      tmp_base = m_void_null_ptr;
      tmp_bound = m_infinite_bound_ptr;
      return;
    }
    // Create getElementPtrs to create the base and bound 

    std::vector<Constant*> indices_base;
    std::vector<Constant*> indices_bound;
      
    GlobalVariable* gv = dyn_cast<GlobalVariable>(given_constant);


    // TODO: External globals get zero base and infinite_bound 

    if(gv && !gv->hasInitializer()) {
      tmp_base = m_void_null_ptr;
      tmp_bound = m_infinite_bound_ptr;
      return;
    }

    Constant* index_base0 = 
      Constant::
      getNullValue(Type::getInt32Ty(given_constant->getType()->getContext()));

    Constant* index_bound0 = 
      ConstantInt::
      get(Type::getInt32Ty(given_constant->getType()->getContext()), 1);

    indices_base.push_back(index_base0);
    indices_bound.push_back(index_bound0);

    Constant* gep_base = ConstantExpr::getGetElementPtr(nullptr,
							given_constant, 
                                                        indices_base);    
    Constant* gep_bound = ConstantExpr::getGetElementPtr(nullptr,
							 given_constant, 
                                                         indices_bound);
      
    tmp_base = gep_base;
    tmp_bound = gep_bound;      
  }
}

//
// Method: addStoreBaseBoundFunc
//
// Description:
//
// This function inserts metadata stores into the bitcode whenever a
// pointer is being stored to memory.
//
// Inputs:
//
// pointer_dest: address where the pointer being stored
//
// pointer_base, pointer_bound, pointer_key, pointer_lock: metadata
// associated with the pointer being stored
//
// pointer : pointer being stored to memory
//
// size_of_type: size of the access
//
// insert_at: the insertion point in the bitcode before which the
// metadata store is introduced.
//
void Nova::AddStoreBaseBoundFunc(Value* pointer_dest, 
                                 Value* pointer_base, 
                                 Value* pointer_bound, 
                                 Value* pointer,
                                 Value* size_of_type, 
                                 Instruction* insert_at) {

  Value* pointer_base_cast = NULL;
  Value* pointer_bound_cast = NULL;

  
  Value* pointer_dest_cast = CastToVoidPtr(pointer_dest, insert_at);

  pointer_base_cast = CastToVoidPtr(pointer_base, insert_at);
  pointer_bound_cast = CastToVoidPtr(pointer_bound, insert_at);

  //  Value* pointer_cast = castToVoidPtr(pointer, insert_at);
    
  SmallVector<Value*, 8> args;

  args.push_back(pointer_dest_cast);

  args.push_back(pointer_base_cast);
  args.push_back(pointer_bound_cast);

  CallInst::Create(m_store_base_bound_func, args, "", insert_at);
}

//
// Method: handleGlobalSequentialTypeInitializer
//
// Description: This performs the initialization of the metadata for
// the pointers in the global segments that are initialized with
// non-zero values.
//
// Comments: This function requires review and rewrite

void Nova::HandleGlobalSequentialTypeInitializer(Module& module, GlobalVariable* gv) {

  //errs() << __func__ <<"\n";
  // Sequential type can be an array type, a pointer type 
  const SequentialType* init_seq_type = 
    dyn_cast<SequentialType>((gv->getInitializer())->getType());
  assert(init_seq_type && 
         "[handleGlobalSequentialTypeInitializer] initializer  null?");

  Instruction* init_function_terminator = GetGlobalInitInstruction(module);
  if(gv->getInitializer()->isNullValue())
    return;
    
  if(isa<ArrayType>(init_seq_type)){      
    const ArrayType* init_array_type = dyn_cast<ArrayType>(init_seq_type);     
    if(isa<StructType>(init_array_type->getElementType())){
      // It is an array of structures

      // Check whether the structure has a pointer, if it has a
      // pointer then, we need to store the base and bound of the
      // pointer into the metadata space. However, if the structure
      // does not have any pointer, we can make a quick exit in
      // processing this global
      //

      bool struct_has_pointers = false;
      StructType* init_struct_type = 
        dyn_cast<StructType>(init_array_type->getElementType());
      CompositeType* struct_comp_type = 
        dyn_cast<CompositeType>(init_struct_type);
      
      assert(struct_comp_type && "struct composite type null?");
      assert(init_struct_type && 
             "Array of structures and struct type null?");        
      unsigned num_struct_elements = init_struct_type->getNumElements();        
      for(unsigned i = 0; i < num_struct_elements; i++) {
        Type* element_type = struct_comp_type->getTypeAtIndex(i);
        if(isa<PointerType>(element_type)){
          struct_has_pointers = true;
        }
      }
      if(!struct_has_pointers)
        return;

      // Here implies, global variable is an array of structures with
      // a pointer. Thus for each pointer we need to store the base
      // and bound

      size_t num_array_elements = init_array_type->getNumElements();
      ConstantArray* const_array = 
        dyn_cast<ConstantArray>(gv->getInitializer());
      if(!const_array)
        return;

      for( unsigned i = 0; i < num_array_elements ; i++) {
        Constant* struct_constant = const_array->getOperand(i);
        assert(struct_constant && 
               "Initializer structure type but not a constant?");          
        // Constant has zero initializer 
        if(struct_constant->isNullValue())
          continue;
          
        for( unsigned j = 0 ; j < num_struct_elements; j++) {
          const Type* element_type = init_struct_type->getTypeAtIndex(j);
            
          if(isa<PointerType>(element_type)){
              
            Value* initializer_opd = struct_constant->getOperand(j);
            Value* operand_base = NULL;
            Value* operand_bound = NULL;
            Constant* given_constant = dyn_cast<Constant>(initializer_opd);
            assert(given_constant && 
                   "[handleGlobalStructTypeInitializer] not a constant?");
              
            GetConstantExprBaseBound(given_constant, operand_base, operand_bound);            
            // Creating the address of ptr
            Constant* index0 = 
              ConstantInt::get(Type::getInt32Ty(module.getContext()), 0);
            Constant* index1 = 
              ConstantInt::get(Type::getInt32Ty(module.getContext()), i);
            Constant* index2 = 
              ConstantInt::get(Type::getInt32Ty(module.getContext()), j);
              
            std::vector<Constant *> indices_addr_ptr;            
                            
            indices_addr_ptr.push_back(index0);
            indices_addr_ptr.push_back(index1);
            indices_addr_ptr.push_back(index2);

            Constant* Indices[3] = {index0, index1, index2};
            Constant* addr_of_ptr = ConstantExpr::getGetElementPtr(nullptr, gv, Indices);
            Type* initializer_type = initializer_opd->getType();
            Value* initializer_size = GetSizeOfType(initializer_type);
            
            AddStoreBaseBoundFunc(addr_of_ptr, operand_base, operand_bound, 
                                  initializer_opd, initializer_size, init_function_terminator);
          }                       
        } // Iterating over struct element ends 
      } // Iterating over array element ends         
    }/// Array of Structures Ends 

    if (isa<PointerType>(init_array_type->getElementType())){
      // It is a array of pointers
    }
  }  // Array type case ends 

  if(isa<PointerType>(init_seq_type)){
    // individual pointer stores 
    Value* initializer_base = NULL;
    Value* initializer_bound = NULL;
    Value* initializer = gv->getInitializer();
    Constant* given_constant = dyn_cast<Constant>(initializer);
    GetConstantExprBaseBound(given_constant, 
                             initializer_base, 
                             initializer_bound);
    Type* initializer_type = initializer->getType();
    Value* initializer_size = GetSizeOfType(initializer_type);
    
    AddStoreBaseBoundFunc(gv, initializer_base, initializer_bound,
                          initializer, initializer_size, 
                          init_function_terminator);        
  }

}

void Nova::AddBaseBoundGlobalValue(Module &M, Value *v){

    GlobalVariable* gv = dyn_cast<GlobalVariable>(v);
    
    if(!gv){
      return;
    }

    if(StringRef(gv->getSection()) == "llvm.metadata"){
      return;
    }
    if(gv->getName() == "llvm.global_ctors"){
      return;
    }
    
    if(!gv->hasInitializer())
      return;
    
    /* gv->hasInitializer() is true */
    
    Constant* initializer = dyn_cast<Constant>(gv->getInitializer());
    ConstantArray* constant_array = dyn_cast<ConstantArray>(initializer);
    
    if(initializer && isa<CompositeType>(initializer->getType())){

      if(isa<StructType>(initializer->getType())){
        std::vector<Constant*> indices_addr_ptr;
        Constant* index1 = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);
        indices_addr_ptr.push_back(index1);
        StructType* struct_type = dyn_cast<StructType>(initializer->getType());
        HandleGlobalStructTypeInitializer(M, struct_type, initializer, gv, indices_addr_ptr, 1);
        return;
      }
      
      if(isa<SequentialType>(initializer->getType())){
        HandleGlobalSequentialTypeInitializer(M, gv);
      }
    }
    
    if(initializer && !constant_array){
        errs() <<"gv doesn't have constant_array initializer\n";
        initializer->getType()->dump();
      
      if(isa<PointerType>(initializer->getType())){
        errs() <<"gv has Pointer type initializer\n";
      }
    }
    
    if(!constant_array)
      return;
    
    int num_ca_opds = constant_array->getNumOperands();
    errs() <<"gv has constant_array initializer, num_ca_opds: "<< num_ca_opds << "\n";
    
    for(int i = 0; i < num_ca_opds; i++){
      Value* initializer_opd = constant_array->getOperand(i);
      Instruction* first = GetGlobalInitInstruction(M);
      Value* operand_base = NULL;
      Value* operand_bound = NULL;
      
      Constant* global_constant_initializer = dyn_cast<Constant>(initializer_opd);
      if(!isa<PointerType>(global_constant_initializer->getType())){
        break;
      }
      GetConstantExprBaseBound(global_constant_initializer, operand_base, operand_bound);
      
      SmallVector<Value*, 8> args;
      Constant* index1 = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);
      Constant* index2 = ConstantInt::get(Type::getInt32Ty(M.getContext()), i);

      std::vector<Constant*> indices_addr_ptr;
      indices_addr_ptr.push_back(index1);
      indices_addr_ptr.push_back(index2);

      Constant* addr_of_ptr = ConstantExpr::getGetElementPtr(nullptr, gv, indices_addr_ptr);
      Type* initializer_type = initializer_opd->getType();
      Value* initializer_size = GetSizeOfType(initializer_type);
      
      AddStoreBaseBoundFunc(addr_of_ptr, operand_base, operand_bound, initializer_opd, initializer_size, first);
    }
}

void Nova::CollectArrayBoundaryInfo(Module &M, Value *v) {
    AllocaInst *ai;
    Instruction *inst;


    if (isa<GlobalVariable>(v)) { // handle global variable here
        errs() << __func__ << " global v is :" << *v << "\n";
        AddBaseBoundGlobalValue(M, v);
    } else { // handle local variable here

        errs() << __func__ << " local v is :" << *v << "\n";

        ai = dyn_cast<AllocaInst>(v);
        assert(ai && " local array is not defined as alloca inst!");
        
        // get next inst after ai
        BasicBlock::iterator nextInst = GetInstIterator(v);
        nextInst++;
        Instruction *next = dyn_cast<Instruction>(nextInst);
        assert (next && "Cannot increment the instruction iterator?");

        // get basicblock that ai belongs to
        inst = dyn_cast<Instruction>(v);
        assert (inst && "v should be an instruction!");

        unsigned numOperands = ai->getNumOperands();

        /* For any alloca instruction, base is bitcast of alloca,
           bound is bitcast of alloca_ptr + 1. Refer to SoftboundsCETS */
        PointerType* ptrType = PointerType::get(ai->getAllocatedType(), 0);
        Type* ty1 = ptrType;
        BitCastInst* ptr = new BitCastInst(ai, ty1, ai->getName(), next);

        Value* ptrBase = CastToVoidPtr(v, next);
        Value* intBound;

        if(numOperands == 0) {
            // TODO: We only support 64-bit architechture
            intBound = ConstantInt::get(Type::getInt64Ty(ai->getType()->getContext()), 1, false);
        } else {
            // What can be operand of alloca instruction?
            intBound = ai->getOperand(0);
        }

        GetElementPtrInst* gep = GetElementPtrInst::Create(nullptr,
                                                           ptr,
                                                           intBound,
                                                           "mtmp",
                                                           next);
        Value *boundPtr = gep;
        Value* ptrBound = CastToVoidPtr(boundPtr, next);

        // TODOO instrument or leave it there?
        AssociateBaseBound(v, ptrBase, ptrBound);
    }

    return;
}

// Method: isStructOperand
// **Borrowed from SoftboundCETS
//
//
//Description: This function elides the checks for the structure
//accesses. This is safe when there are no casts in the program.
//
bool Nova::IsStructOperand(Value* pointer_operand) {
  
  if(isa<GetElementPtrInst>(pointer_operand)){
    GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(pointer_operand);
    Value* gep_operand = gep->getOperand(0);
    const PointerType* ptr_type = dyn_cast<PointerType>(gep_operand->getType());
    if(isa<StructType>(ptr_type->getElementType())){
      return true;
    }
  }
  return false;
}

// for array type, we get boundary info immediately
void Nova::ArrayAccessCheck(Module &M, Value *v) {
    Value *pointerOperand = NULL;
    Instruction *inst = NULL;
    Value* tmpBase = NULL;
    Value* tmpBound = NULL;
    Value* bitcastBase = NULL;
    Value* bitcastBound = NULL;
    Value* castPointerValue = NULL;
    ValueSet kinSet; // propagated variable set with the same metadata as v.
    SmallVector<Value*, 8> args;
    // if array is global variable, we should initialize it before program exeucte
    // if array is local variable, we should insert code after the last alloca instruction
    // in the first bb.

    // collect array boundary info
    CollectArrayBoundaryInfo(M, v);

    // propagate metadata to a set of related variables
    kinSet.insert(v);
    for (User *UoV : v->users()) {
        if ((inst = (dyn_cast<Instruction>(UoV)))) {
            if (isa<GetElementPtrInst>(inst)) {
                GetElementPtrInst *gepi = dyn_cast<GetElementPtrInst>(inst);
                assert(gepi && "Not a GEP instruction");
                pointerOperand = gepi->getPointerOperand();
                assert(v == pointerOperand && "not propagate from v to GEP's Pointer Operand ?");
                tmpBase = GetAssociatedBase(pointerOperand);
                tmpBound = GetAssociatedBound(pointerOperand);
                AssociateBaseBound(inst, tmpBase, tmpBound);   
                kinSet.insert(inst);
                errs() <<"add to kinSet: " <<*inst << "\n";
            } else if (isa<BitCastInst>(inst)) {
                BitCastInst *bci = dyn_cast<BitCastInst>(inst);
                assert(bci && "Not a BitCast instruction");
                pointerOperand = bci->getOperand(0);
                assert(v == pointerOperand && "not propagate from v to BitCast's Pointer Operand ?");
                tmpBase = GetAssociatedBase(pointerOperand);
                tmpBound = GetAssociatedBound(pointerOperand);
                AssociateBaseBound(inst, tmpBase, tmpBound); 
                kinSet.insert(inst);
                errs() <<"add to kinSet: " <<*inst << "\n";
            } else {
                errs() <<"not add to kinSet: " <<*inst << "\n";
            }
        } else {
            errs() <<"Attention: v("<< *v << ") is not used in any instruction:\n";
        }
    }

    // check array-based read&write
    // TODOO
    for (ValueSet::iterator it = kinSet.begin(), ie = kinSet.end(); it != ie; ++it) {
        for (User *UoV : (*it)->users()) {
            if ((inst = (dyn_cast<Instruction>(UoV)))) {
                errs() << "v is used in instruction:\n";
                errs() << *inst << "\n";

                if (isa<LoadInst>(inst)) {
                    LoadInst *ldi = dyn_cast<LoadInst>(inst);
                    assert(ldi && "Not a load instruction");
                    pointerOperand = ldi->getPointerOperand();
                } else if (isa<StoreInst>(inst)) {
                    StoreInst *sti = dyn_cast<StoreInst>(inst);
                    assert(sti && "Not a store instruction");
                    pointerOperand = sti->getPointerOperand();
                } else {
                    errs() << "not load nor store inst, skip\n";
                    continue;
                }

                assert(pointerOperand && "pointer operand null?");
                if (pointerOperand != (*it)) {
                    // pointerOperand isn't the sensitive var, need to handle its boundary collection
                    CollectArrayBoundaryInfo(M, pointerOperand);
                }

                // should we ignore stuct pointer check?
                // TODO
                if(IsStructOperand(pointerOperand))
                    continue;

                // If it is a null pointer which is being loaded, then it must seg
                // fault, no dereference check here. Refer to SoftboundCETS
                if(isa<ConstantPointerNull>(pointerOperand))
                    continue;

                tmpBase = GetAssociatedBase(pointerOperand);
                tmpBound = GetAssociatedBound(pointerOperand);

                // empty args
                args.clear();

                bitcastBase = CastToVoidPtr(tmpBase, inst);
                args.push_back(bitcastBase);
                
                bitcastBound = CastToVoidPtr(tmpBound, inst);    
                args.push_back(bitcastBound);
                 
                castPointerValue = CastToVoidPtr(pointerOperand, inst);    
                args.push_back(castPointerValue);

                // Pushing the size of the type 
                Type* pointerOperandType = pointerOperand->getType();
                Value* sizeOfType = GetSizeOfType(pointerOperandType);
                args.push_back(sizeOfType);

                // add check inst
                // TODOO create function symbol for it.
                errs() << "insert load/store dereference check \n";
                CallInst::Create(m_spatial_load_dereference_check, args, "", inst);
            }
        }
    }
}

// step : instrument its definition to collect boundary info
//void Nova::CollectArrayBoundaryInfo(Value *v) {
//    // TODOO
//    IRBuilder<> B(inst);
//    Module *M = B.GetInsertBlock()->getModule();
//    Type *VoidTy = B.getVoidTy();
//    Type *I64Ty = B.getInt64Ty();
//    Value *castAddr, *castVal;
//
//    //errs() << __func__ << " : "<< *inst << "\n";
//
//    Constant *RecordDefEvt = M->getOrInsertFunction("__record_defevt", VoidTy,
//                                                                      I64Ty,
//                                                                      I64Ty, 
//                                                                      nullptr);
//
//    Function *RecordDefEvtFunc = cast<Function>(RecordDefEvt);
//    castAddr = CastInst::Create(Instruction::PtrToInt, addr, I64Ty, "recptrtoint", inst);
//
//    if (val->getType()->isPointerTy()) {
//        castVal = CastInst::Create(Instruction::PtrToInt, val, I64Ty, "recptrtoint", inst);
//    } else if (val->getType()->isIntegerTy(64)) {
//        castVal = val;
//    } else if (val->getType()->isIntegerTy()) {
//        castVal = CastInst::Create(Instruction::ZExt, val, I64Ty, "reczexttoi64", inst);
//    } else
//	return;
//
//    B.CreateCall(RecordDefEvtFunc, {castAddr, castVal});
//
//    return;
//}

//
// Method: isFuncDefSoftBound
//
// Description: 
//
// This function checks if the input function name is a
// SoftBound/CETS defined function
//

bool Nova::IsFuncDefSoftBound(const std::string &str) {
  if (m_func_def_softbound.getNumItems() == 0) {

    m_func_wrappers_available["system"] = true;
    m_func_wrappers_available["setreuid"] = true;
    m_func_wrappers_available["mkstemp"] = true;
    m_func_wrappers_available["getuid"] = true;
    m_func_wrappers_available["getrlimit"] = true;
    m_func_wrappers_available["setrlimit"] = true;
    m_func_wrappers_available["fread"] = true;
    m_func_wrappers_available["umask"] = true;
    m_func_wrappers_available["mkdir"] = true;
    m_func_wrappers_available["chroot"] = true;
    m_func_wrappers_available["rmdir"] = true;
    m_func_wrappers_available["stat"] = true;
    m_func_wrappers_available["fputc"] = true;
    m_func_wrappers_available["fileno"] = true;
    m_func_wrappers_available["fgetc"] = true;
    m_func_wrappers_available["strncmp"] = true;
    m_func_wrappers_available["log"] = true;
    m_func_wrappers_available["fwrite"] = true;
    m_func_wrappers_available["atof"] = true;
    m_func_wrappers_available["feof"] = true;
    m_func_wrappers_available["remove"] = true;
    m_func_wrappers_available["acos"] = true;
    m_func_wrappers_available["atan2"] = true;
    m_func_wrappers_available["sqrtf"] = true;
    m_func_wrappers_available["expf"] = true;
    m_func_wrappers_available["exp2"] = true;
    m_func_wrappers_available["floorf"] = true;
    m_func_wrappers_available["ceil"] = true;
    m_func_wrappers_available["ceilf"] = true;
    m_func_wrappers_available["floor"] = true;
    m_func_wrappers_available["sqrt"] = true;
    m_func_wrappers_available["fabs"] = true;
    m_func_wrappers_available["abs"] = true;
    m_func_wrappers_available["srand"] = true;
    m_func_wrappers_available["srand48"] = true;
    m_func_wrappers_available["pow"] = true;
    m_func_wrappers_available["fabsf"] = true;
    m_func_wrappers_available["tan"] = true;
    m_func_wrappers_available["tanf"] = true;
    m_func_wrappers_available["tanl"] = true;
    m_func_wrappers_available["log10"] = true;
    m_func_wrappers_available["sin"] = true;
    m_func_wrappers_available["sinf"] = true;
    m_func_wrappers_available["sinl"] = true;
    m_func_wrappers_available["cos"] = true;
    m_func_wrappers_available["cosf"] = true;
    m_func_wrappers_available["cosl"] = true;
    m_func_wrappers_available["exp"] = true;
    m_func_wrappers_available["ldexp"] = true;
    m_func_wrappers_available["tmpfile"] = true;
    m_func_wrappers_available["ferror"] = true;
    m_func_wrappers_available["ftell"] = true;
    m_func_wrappers_available["fstat"] = true;
    m_func_wrappers_available["fflush"] = true;
    m_func_wrappers_available["fputs"] = true;
    m_func_wrappers_available["fopen"] = true;
    m_func_wrappers_available["fdopen"] = true;
    m_func_wrappers_available["fseek"] = true;
    m_func_wrappers_available["ftruncate"] = true;
    m_func_wrappers_available["popen"] = true;
    m_func_wrappers_available["fclose"] = true;
    m_func_wrappers_available["pclose"] = true;
    m_func_wrappers_available["rewind"] = true;
    m_func_wrappers_available["readdir"] = true;
    m_func_wrappers_available["opendir"] = true;
    m_func_wrappers_available["closedir"] = true;
    m_func_wrappers_available["rename"] = true;
    m_func_wrappers_available["sleep"] = true;
    m_func_wrappers_available["getcwd"] = true;
    m_func_wrappers_available["chown"] = true;
    m_func_wrappers_available["isatty"] = true;
    m_func_wrappers_available["chdir"] = true;
    m_func_wrappers_available["strcmp"] = true;
    m_func_wrappers_available["strcasecmp"] = true;
    m_func_wrappers_available["strncasecmp"] = true;
    m_func_wrappers_available["strlen"] = true;
    m_func_wrappers_available["strpbrk"] = true;
    m_func_wrappers_available["gets"] = true;
    m_func_wrappers_available["fgets"] = true;
    m_func_wrappers_available["perror"] = true;
    m_func_wrappers_available["strspn"] = true;
    m_func_wrappers_available["strcspn"] = true;
    m_func_wrappers_available["memcmp"] = true;
    m_func_wrappers_available["memchr"] = true;
    m_func_wrappers_available["rindex"] = true;
    m_func_wrappers_available["strtoul"] = true;
    m_func_wrappers_available["strtod"] = true;
    m_func_wrappers_available["strtol"] = true;
    m_func_wrappers_available["strchr"] = true;
    m_func_wrappers_available["strrchr"] = true;
    m_func_wrappers_available["strcpy"] = true;
    m_func_wrappers_available["abort"] = true;
    m_func_wrappers_available["rand"] = true;
    m_func_wrappers_available["atoi"] = true;
    //m_func_wrappers_available["puts"] = true;
    m_func_wrappers_available["exit"] = true;
    m_func_wrappers_available["strtok"] = true;
    m_func_wrappers_available["strdup"] = true;
    m_func_wrappers_available["strcat"] = true;
    m_func_wrappers_available["strncat"] = true;
    m_func_wrappers_available["strncpy"] = true;
    m_func_wrappers_available["strstr"] = true;
    m_func_wrappers_available["signal"] = true;
    m_func_wrappers_available["clock"] = true;
    m_func_wrappers_available["atol"] = true;
    m_func_wrappers_available["realloc"] = true;
    m_func_wrappers_available["calloc"] = true;
    m_func_wrappers_available["malloc"] = true;
    m_func_wrappers_available["mmap"] = true;

    m_func_wrappers_available["putchar"] = true;
    m_func_wrappers_available["times"] = true;
    m_func_wrappers_available["strftime"] = true;
    m_func_wrappers_available["localtime"] = true;
    m_func_wrappers_available["time"] = true;
    m_func_wrappers_available["drand48"] = true;
    m_func_wrappers_available["free"] = true;
    m_func_wrappers_available["lrand48"] = true;
    m_func_wrappers_available["ctime"] = true;
    m_func_wrappers_available["difftime"] = true;
    m_func_wrappers_available["toupper"] = true;
    m_func_wrappers_available["tolower"] = true;
    m_func_wrappers_available["setbuf"] = true;
    m_func_wrappers_available["getenv"] = true;
    m_func_wrappers_available["atexit"] = true;
    m_func_wrappers_available["strerror"] = true;
    m_func_wrappers_available["unlink"] = true;
    m_func_wrappers_available["close"] = true;
    m_func_wrappers_available["open"] = true;
    m_func_wrappers_available["read"] = true;
    m_func_wrappers_available["write"] = true;
    m_func_wrappers_available["lseek"] = true;
    m_func_wrappers_available["gettimeofday"] = true;
    m_func_wrappers_available["select"] = true;
    m_func_wrappers_available["__errno_location"] = true;
    m_func_wrappers_available["__ctype_b_loc"] = true;
    m_func_wrappers_available["__ctype_toupper_loc"] = true;
    m_func_wrappers_available["__ctype_tolower_loc"] = true;
    m_func_wrappers_available["qsort"] = true;

    m_func_def_softbound["puts"] = true;
    m_func_def_softbound["__softboundcets_intermediate"]= true;
    m_func_def_softbound["__softboundcets_dummy"] = true;
    m_func_def_softbound["__softboundcets_print_metadata"] = true;
    m_func_def_softbound["__softboundcets_introspect_metadata"] = true;
    m_func_def_softbound["__softboundcets_copy_metadata"] = true;
    m_func_def_softbound["__softboundcets_allocate_shadow_stack_space"] = true;
    m_func_def_softbound["__softboundcets_load_base_shadow_stack"] = true;
    m_func_def_softbound["__softboundcets_load_bound_shadow_stack"] = true;
    m_func_def_softbound["__softboundcets_load_key_shadow_stack"] = true;
    m_func_def_softbound["__softboundcets_load_lock_shadow_stack"] = true;
    m_func_def_softbound["__softboundcets_store_base_shadow_stack"] = true;      
    m_func_def_softbound["__softboundcets_store_bound_shadow_stack"] = true;      
    m_func_def_softbound["__softboundcets_store_key_shadow_stack"] = true;      
    m_func_def_softbound["__softboundcets_store_lock_shadow_stack"] = true;      
    m_func_def_softbound["__softboundcets_deallocate_shadow_stack_space"] = true;

    m_func_def_softbound["__softboundcets_trie_allocate"] = true;
    m_func_def_softbound["__shrinkBounds"] = true;
    m_func_def_softbound["__softboundcets_memcopy_check"] = true;

    m_func_def_softbound["__softboundcets_spatial_load_dereference_check"] = true;

    m_func_def_softbound["__softboundcets_spatial_store_dereference_check"] = true;
    m_func_def_softbound["__softboundcets_spatial_call_dereference_check"] = true;
    m_func_def_softbound["__softboundcets_temporal_load_dereference_check"] = true;
    m_func_def_softbound["__softboundcets_temporal_store_dereference_check"] = true;
    m_func_def_softbound["__softboundcets_stack_memory_allocation"] = true;
    m_func_def_softbound["__softboundcets_memory_allocation"] = true;
    m_func_def_softbound["__softboundcets_get_global_lock"] = true;
    m_func_def_softbound["__softboundcets_add_to_free_map"] = true;
    m_func_def_softbound["__softboundcets_check_remove_from_free_map"] = true;
    m_func_def_softbound["__softboundcets_allocation_secondary_trie_allocate"] = true;
    m_func_def_softbound["__softboundcets_allocation_secondary_trie_allocate_range"] = true;
    m_func_def_softbound["__softboundcets_allocate_lock_location"] = true;
    m_func_def_softbound["__softboundcets_memory_deallocation"] = true;
    m_func_def_softbound["__softboundcets_stack_memory_deallocation"] = true;

    m_func_def_softbound["__softboundcets_metadata_load_vector"] = true;
    m_func_def_softbound["__softboundcets_metadata_store_vector"] = true;
    
    m_func_def_softbound["__softboundcets_metadata_load"] = true;
    m_func_def_softbound["__softboundcets_metadata_store"] = true;
    m_func_def_softbound["__hashProbeAddrOfPtr"] = true;
    m_func_def_softbound["__memcopyCheck"] = true;
    m_func_def_softbound["__memcopyCheck_i64"] = true;

    m_func_def_softbound["__softboundcets_global_init"] = true;      
    m_func_def_softbound["__softboundcets_init"] = true;      
    m_func_def_softbound["__softboundcets_abort"] = true;      
    m_func_def_softbound["__softboundcets_printf"] = true;
    
    m_func_def_softbound["__softboundcets_stub"] = true;
    m_func_def_softbound["safe_mmap"] = true;
    m_func_def_softbound["safe_calloc"] = true;
    m_func_def_softbound["safe_malloc"] = true;
    m_func_def_softbound["safe_free"] = true;

    m_func_def_softbound["__assert_fail"] = true;
    m_func_def_softbound["assert"] = true;
    m_func_def_softbound["__strspn_c2"] = true;
    m_func_def_softbound["__strcspn_c2"] = true;
    m_func_def_softbound["__strtol_internal"] = true;
    m_func_def_softbound["__stroul_internal"] = true;
    m_func_def_softbound["ioctl"] = true;
    m_func_def_softbound["error"] = true;
    m_func_def_softbound["__strtod_internal"] = true;
    m_func_def_softbound["__strtoul_internal"] = true;
    
    
    m_func_def_softbound["fflush_unlocked"] = true;
    m_func_def_softbound["full_write"] = true;
    m_func_def_softbound["safe_read"] = true;
    m_func_def_softbound["_IO_getc"] = true;
    m_func_def_softbound["_IO_putc"] = true;
    m_func_def_softbound["__xstat"] = true;

    m_func_def_softbound["select"] = true;
    m_func_def_softbound["_setjmp"] = true;
    m_func_def_softbound["longjmp"] = true;
    m_func_def_softbound["fork"] = true;
    m_func_def_softbound["pipe"] = true;
    m_func_def_softbound["dup2"] = true;
    m_func_def_softbound["execv"] = true;
    m_func_def_softbound["compare_pic_by_pic_num_desc"] = true;
     
    m_func_def_softbound["wprintf"] = true;
    m_func_def_softbound["vfprintf"] = true;
    m_func_def_softbound["vsprintf"] = true;
    m_func_def_softbound["fprintf"] = true;
    m_func_def_softbound["printf"] = true;
    m_func_def_softbound["sprintf"] = true;
    m_func_def_softbound["snprintf"] = true;

    m_func_def_softbound["scanf"] = true;
    m_func_def_softbound["fscanf"] = true;
    m_func_def_softbound["sscanf"] = true;   

    m_func_def_softbound["asprintf"] = true;
    m_func_def_softbound["vasprintf"] = true;
    m_func_def_softbound["__fpending"] = true;
    m_func_def_softbound["fcntl"] = true;

    m_func_def_softbound["vsnprintf"] = true;
    m_func_def_softbound["fwrite_unlocked"] = true;
    m_func_def_softbound["__overflow"] = true;
    m_func_def_softbound["__uflow"] = true;
    m_func_def_softbound["execlp"] = true;
    m_func_def_softbound["execl"] = true;
    m_func_def_softbound["waitpid"] = true;
    m_func_def_softbound["dup"] = true;
    m_func_def_softbound["setuid"] = true;
    
    m_func_def_softbound["_exit"] = true;
    m_func_def_softbound["funlockfile"] = true;
    m_func_def_softbound["flockfile"] = true;

    m_func_def_softbound["__option_is_short"] = true;
    

  }

  // Is the function name in the above list?
  if (m_func_def_softbound.count(str) > 0) {
    return true;
  }

  // FIXME: handling new intrinsics which have isoc99 in their name
  if (str.find("isoc99") != std::string::npos){
    return true;
  }

  // If the function is an llvm intrinsic, don't transform it
  if (str.find("llvm.") == 0) {
    return true;
  }

  return false;
}

//
// Method: hasPtrArgRetType()
//
// Description:
//
// This function checks if the function has either pointer arguments
// or returns a pointer value. This function is used to determine
// whether shadow stack loads/stores need to be introduced for
// metadata propagation.
//

bool Nova::HasPtrArgRetType(Function* func) {
   
  const Type* ret_type = func->getReturnType();
  if (isa<PointerType>(ret_type))
    return true;

  for (Function::arg_iterator i = func->arg_begin(), e = func->arg_end(); 
      i != e; ++i) {
      
    if (isa<PointerType>(i->getType()))
      return true;
  }
  return false;
}

// 
// Method: identifyFuncToTrans
//
// Description: This function traverses the module and identifies the
// functions that need to be transformed by SoftBound/CETS
//

void Nova::IdentifyFuncToTrans(Module& module) {
    
  for (Module::iterator fb_it = module.begin(), fe_it = module.end(); 
      fb_it != fe_it; ++fb_it) {

    Function* func = dyn_cast<Function>(fb_it);
    assert(func && " Not a function");

    // Check if the function is defined in the module
    if (!func->isDeclaration()) {
      if (IsFuncDefSoftBound(func->getName())) 
        continue;
      
      m_func_softboundcets_transform[func->getName()] = true;
      if (HasPtrArgRetType(func)) {
        m_func_to_transform[func->getName()] = true;
      }
    }
  }
}

/* Identify the initial globals present in the program before we add
 * extra base and bound for all globals
 */
void Nova::IdentifyInitialGlobals(Module& module) {

  for(Module::global_iterator it = module.global_begin(), 
        ite = module.global_end();
      it != ite; ++it) {
      
    GlobalVariable* gv = dyn_cast<GlobalVariable>(it);
    if(gv) {
      m_initial_globals[gv] = true;
    }      
  }
}

// Currently just a placeholder for functions introduced by us
bool Nova::CheckIfFunctionOfInterest(Function* func) {

  if(IsFuncDefSoftBound(func->getName()))
    return false;

  if(func->isDeclaration())
    return false;


  /* TODO: URGENT: Need to do base and bound propagation in variable
   * argument functions
   */
#if 0
  if(func.isVarArg())
    return false;
#endif

  return true;
}

void Nova::IdentifyOriginalInst (Function * func) {

  for(Function::iterator bb_begin = func->begin(), bb_end = func->end();
      bb_begin != bb_end; ++bb_begin) {

    for(BasicBlock::iterator i_begin = bb_begin->begin(),
          i_end = bb_begin->end(); i_begin != i_end; ++i_begin){

      Value* insn = dyn_cast<Value>(i_begin);
      if(!m_present_in_original.count(insn)) {
        m_present_in_original[insn] = 1;
      }
      else {
        assert(0 && "present in original map already has the insn?");
      }

      if(isa<PointerType>(insn->getType())) {
        if(!m_is_pointer.count(insn)){
          m_is_pointer[insn] = 1;
        }
      }
    } /* BasicBlock ends */
  }/* Function ends */
}

bool Nova::CheckPtrsInST(StructType* struct_type){
  
  StructType::element_iterator I = struct_type->element_begin();

  bool ptr_flag = false;
  for(StructType::element_iterator E = struct_type->element_end(); I != E; ++I){
    
    Type* element_type = *I;

    if(isa<StructType>(element_type)){
      StructType* struct_element_type = dyn_cast<StructType>(element_type);
      bool recursive_flag = CheckPtrsInST(struct_element_type);
      ptr_flag = ptr_flag | recursive_flag;
    }
    if(isa<PointerType>(element_type)){
      ptr_flag = true;
    }
    if(isa<ArrayType>(element_type)){
      ptr_flag = true;      
    }
  }
  return ptr_flag;
}

bool Nova::CheckTypeHasPtrs(Argument* ptr_argument){

  if(!ptr_argument->hasByValAttr())
    return false;

  SequentialType* seq_type = dyn_cast<SequentialType>(ptr_argument->getType());
  assert(seq_type && "byval attribute with non-sequential type pointer, not handled?");

  StructType* struct_type = dyn_cast<StructType>(seq_type->getElementType());

  if(struct_type){
    bool has_ptrs = CheckPtrsInST(struct_type);
    return has_ptrs;
  }
  else{
    assert(0 && "non-struct byval parameters?");
  }

  // By default we assume any struct can return pointers 
  return true;                                              

}

void Nova::HandleAlloca (AllocaInst* alloca_inst,
                         BasicBlock* bb, 
                         BasicBlock::iterator& i) {

  Value *alloca_inst_value = alloca_inst;

  /* Get the base type of the alloca object For alloca instructions,
   * instructions need to inserted after the alloca instruction LLVM
   * provides interface for inserting before.  So use the iterators
   * and handle the case
   */
  
  BasicBlock::iterator nextInst = i;
  nextInst++;
  Instruction* next = dyn_cast<Instruction>(nextInst);
  assert(next && "Cannot increment the instruction iterator?");
  
  unsigned num_operands = alloca_inst->getNumOperands();
  
  /* For any alloca instruction, base is bitcast of alloca, bound is bitcast of alloca_ptr + 1
   */
  PointerType* ptr_type = PointerType::get(alloca_inst->getAllocatedType(), 0);
  Type* ty1 = ptr_type;
  //    Value* alloca_inst_temp_value = alloca_inst;
  BitCastInst* ptr = new BitCastInst(alloca_inst, ty1, alloca_inst->getName(), next);
  
  Value* ptr_base = CastToVoidPtr(alloca_inst_value, next);
  
  Value* intBound;
  
  if(num_operands == 0) {
      intBound = ConstantInt::get(Type::getInt64Ty(alloca_inst->getType()->getContext()), 1, false);
  } else {
    // What can be operand of alloca instruction?
    intBound = alloca_inst->getOperand(0);
  }

  GetElementPtrInst* gep = GetElementPtrInst::Create(nullptr,
  					                                 ptr,
                                                     intBound,
                                                     "mtmp",
                                                     next);
  Value *bound_ptr = gep;
  
  Value* ptr_bound = CastToVoidPtr(bound_ptr, next);
  
  AssociateBaseBound(alloca_inst_value, ptr_base, ptr_bound);
}

// 
// Method: getNextInstruction
// 
// Description:
// This method returns the next instruction after the input instruction.
//

Instruction* Nova::GetNextInstruction(Instruction* I){
  
  if (isa<TerminatorInst>(I)) {
    return I;
  } else {
    BasicBlock::iterator BBI(I);
    Instruction* temp = &*(++BBI);
    return temp;
  }    
}

void Nova::InsertMetadataLoad(LoadInst* load_inst){

  AllocaInst* base_alloca;
  AllocaInst* bound_alloca;

  SmallVector<Value*, 8> args;

  Value* load_inst_value = load_inst;
  Value* pointer_operand = load_inst->getPointerOperand();
  Instruction* load = load_inst;    
  Instruction* insert_at = GetNextInstruction(load);

  /* If the load returns a pointer, then load the base and bound
   * from the shadow space
   */
  Value* pointer_operand_bitcast =  CastToVoidPtr(pointer_operand, insert_at);      
  Instruction* first_inst_func = dyn_cast<Instruction>(load_inst->getParent()->getParent()->begin()->begin());
  assert(first_inst_func && "function doesn't have any instruction and there is load???");
  
  /* address of pointer being pushed */
  args.push_back(pointer_operand_bitcast);

  base_alloca = new AllocaInst(m_void_ptr_type, "base.alloca", first_inst_func);
  bound_alloca = new AllocaInst(m_void_ptr_type, "bound.alloca", first_inst_func);

  /* base */
  args.push_back(base_alloca);
  /* bound */
  args.push_back(bound_alloca);

  CallInst::Create(m_load_base_bound_func, args, "", insert_at);

  Instruction* base_load = new LoadInst(base_alloca, "base.load", insert_at);
  Instruction* bound_load = new LoadInst(bound_alloca, "bound.load", insert_at);
  AssociateBaseBound(load_inst_value, base_load, bound_load);
}

/* handleLoad Takes a load_inst If the load is through a pointer
 * which is a global then inserts base and bound for that global
 * Also if the loaded value is a pointer then loads the base and
 * bound for for the pointer from the shadow space
 */

void Nova::HandleLoad(LoadInst* load_inst) { 


  if(!isa<VectorType>(load_inst->getType()) && !isa<PointerType>(load_inst->getType())){
    return;
  }
  
  if(isa<PointerType>(load_inst->getType())){
    InsertMetadataLoad(load_inst);
    return;
  }
 
  if(isa<VectorType>(load_inst->getType())){
    
#if 0
    if(!spatial_safety || !temporal_safety){
      assert(0 && "Loading and Storing Pointers as a first-class types");            
      return;
    }

#endif
    
    // It should be a vector if here
    const VectorType* vector_ty = dyn_cast<VectorType>(load_inst->getType());
    // Introduce a series of metadata loads and associated it pointers
    if(!isa<PointerType>(vector_ty->getElementType()))
       return;
 
#if 0   
    Value* load_inst_value = load_inst;
    Instruction* load = load_inst;    
#endif

    Value* pointer_operand = load_inst->getPointerOperand();
    Instruction* insert_at = GetNextInstruction(load_inst);
        
    Value* pointer_operand_bitcast =  CastToVoidPtr(pointer_operand, insert_at);      
    Instruction* first_inst_func = dyn_cast<Instruction>(load_inst->getParent()->getParent()->begin()->begin());
    assert(first_inst_func && "function doesn't have any instruction and there is load???");
   
    uint64_t num_elements = vector_ty->getNumElements();

    
    SmallVector<Value*, 8> vector_base;
    SmallVector<Value*, 8> vector_bound;

    for(uint64_t i = 0; i < num_elements; i++){
      AllocaInst* base_alloca;
      AllocaInst* bound_alloca;
      
      SmallVector<Value*, 8> args;
      
      args.push_back(pointer_operand_bitcast);
      
      base_alloca = new AllocaInst(m_void_ptr_type, "base.alloca", first_inst_func);
      bound_alloca = new AllocaInst(m_void_ptr_type, "bound.alloca", first_inst_func);
	 
      /* base */
      args.push_back(base_alloca);
      /* bound */
      args.push_back(bound_alloca);

      Constant* index = ConstantInt::get(Type::getInt32Ty(load_inst->getContext()), i);

      args.push_back(index);
          
      CallInst::Create(m_metadata_load_vector_func, args, "", insert_at);
      
      Instruction* base_load = new LoadInst(base_alloca, "base.load", insert_at);
      Instruction* bound_load = new LoadInst(bound_alloca, "bound.load", insert_at);
      
      vector_base.push_back(base_load);
      vector_bound.push_back(bound_load);
    }
    
    if (num_elements > 2){
      assert(0 && "Loading and Storing Pointers as a first-class types with more than 2 elements");      
    }
    
    VectorType* metadata_ptr_type = VectorType::get(m_void_ptr_type, num_elements);
    
    Value *CV0 = ConstantInt::get(Type::getInt32Ty(load_inst->getContext()), 0);
    Value *CV1 = ConstantInt::get(Type::getInt32Ty(load_inst->getContext()), 1);

    Value* base_vector = InsertElementInst::Create(UndefValue::get(metadata_ptr_type),     vector_base[0],  CV0, "", insert_at);
    Value* base_vector_final = InsertElementInst::Create(base_vector, vector_base[1], CV1, "", insert_at);
  
    m_vector_pointer_base[load_inst] = base_vector_final;

    Value* bound_vector = InsertElementInst::Create(UndefValue::get(metadata_ptr_type),     vector_bound[0],  CV0, "", insert_at);
    Value* bound_vector_final = InsertElementInst::Create(bound_vector, vector_bound[1], CV1, "", insert_at); 
    m_vector_pointer_bound[load_inst] = bound_vector_final;

    return;
  }

#if 0
  if(unsafe_byval_opt && isByValDerived(load_inst->getOperand(0))) {

    if(spatial_safety){
      associateBaseBound(load_inst, m_void_null_ptr, m_infinite_bound_ptr);
    }
    if(temporal_safety){
      Value* func_lock = getAssociatedFuncLock(load_inst);
      associateKeyLock(load_inst, m_constantint64ty_one, func_lock);
    }
    return;
  }
#endif

}

//
// Method: checkBaseBoundMetadataPresent()
//
// Description:
// Checks if the metadata is present in the SoftBound/CETS maps.

bool Nova::CheckBaseBoundMetadataPresent(Value* pointer_operand){

  if(m_pointer_base.count(pointer_operand) && 
     m_pointer_bound.count(pointer_operand)){
      return true;
  }
  return false;
}

//
// Method: propagateMetadata
//
// Descripton;
//
// This function propagates the metadata from the source to the
// destination in the map for pointer arithmetic operations~(gep) and
// bitcasts. This is the place where we need to shrink bounds.
//

void Nova::PropagateMetadata(Value* pointer_operand, 
                                      Instruction* inst, 
                                      int instruction_type){

  // Need to just propagate the base and bound here if I am not
  // shrinking bounds
  if(CheckBaseBoundMetadataPresent(inst)){
    // Metadata added to the map in the first pass
    return;
  }

  if(isa<ConstantPointerNull>(pointer_operand)) {
    AssociateBaseBound(inst, m_void_null_ptr, m_void_null_ptr);
    return;
  }

  if (CheckBaseBoundMetadataPresent(pointer_operand)) {
    Value* tmp_base = GetAssociatedBase(pointer_operand); 
    Value* tmp_bound = GetAssociatedBound(pointer_operand);       
    AssociateBaseBound(inst, tmp_base, tmp_bound);
  } else{
    if(isa<Constant>(pointer_operand)) {
      
      Value* tmp_base = NULL;
      Value* tmp_bound = NULL;
      Constant* given_constant = dyn_cast<Constant>(pointer_operand);
      GetConstantExprBaseBound(given_constant, tmp_base, tmp_bound);
      assert(tmp_base && "gep with cexpr and base null?");
      assert(tmp_bound && "gep with cexpr and bound null?");
      tmp_base = CastToVoidPtr(tmp_base, inst);
      tmp_bound = CastToVoidPtr(tmp_bound, inst);        
  
      AssociateBaseBound(inst, tmp_base, tmp_bound);
    } // Constant case ends here
    // Could be in the first pass, do nothing here
  }
}

void Nova::HandleGEP(GetElementPtrInst* gep_inst) {
  Value* getelementptr_operand = gep_inst->getPointerOperand();
  PropagateMetadata(getelementptr_operand, gep_inst, SBCETS_GEP);
}

//
// Method: handleBitCast
//
// Description: Propagate metadata from source to destination with
// pointer bitcast operations.

void Nova::HandleBitCast(BitCastInst* bitcast_inst) {

  Value* pointer_operand = bitcast_inst->getOperand(0);  
  PropagateMetadata(pointer_operand, bitcast_inst, SBCETS_BITCAST);
}

//
// The metadata propagation for PHINode occurs in two passes. In the
// first pass, SoftBound/CETS transformation just creates the metadata
// PHINodes and records it in the maps maintained by
// SoftBound/CETS. In the second pass, it populates the incoming
// values of the PHINodes. This two pass approach ensures that every
// incoming value of the original PHINode will have metadata in the
// SoftBound/CETS maps
// 

//
// Method: handlePHIPass1()
//
// Description:
//
// This function creates a PHINode for the metadata in the bitcode for
// pointer PHINodes. It is important to note that this function just
// creates the PHINode and does not populate the incoming values of
// the PHINode, which is handled by the handlePHIPass2.
//

void Nova::HandlePHIPass1(PHINode* phi_node) {

  // Not a Pointer PHINode, then just return
  if (!isa<PointerType>(phi_node->getType()))
    return;

  unsigned num_incoming_values = phi_node->getNumIncomingValues();

  PHINode* base_phi_node = PHINode::Create(m_void_ptr_type,
                                           num_incoming_values,
                                           "phi.base",
                                           phi_node);
  
  PHINode* bound_phi_node = PHINode::Create(m_void_ptr_type, 
                                            num_incoming_values,
                                            "phi.bound", 
                                            phi_node);
  
  Value* base_phi_node_value = base_phi_node;
  Value* bound_phi_node_value = bound_phi_node;
  
  AssociateBaseBound(phi_node, base_phi_node_value, bound_phi_node_value);
}

void Nova::AddMemcopyMemsetCheck(CallInst* call_inst, Function* called_func) {
  SmallVector<Value*, 8> args;

  if(called_func->getName().find("llvm.memcpy") == 0 || 
     called_func->getName().find("llvm.memmove") == 0){

    CallSite cs(call_inst);

    Value* dest_ptr = cs.getArgument(0);
    Value* src_ptr  = cs.getArgument(1);
    Value* size_ptr = cs.getArgument(2);
    
    args.push_back(dest_ptr);
    args.push_back(src_ptr);

    Value* cast_size_ptr = size_ptr;
    if(size_ptr->getType() != m_key_type){
      BitCastInst* bitcast = new BitCastInst(size_ptr, m_key_type, "", call_inst);
      cast_size_ptr = bitcast;
    }

    args.push_back(cast_size_ptr);

    Value* dest_base = GetAssociatedBase(dest_ptr);
    Value* dest_bound =GetAssociatedBound(dest_ptr);
    
    Value* src_base = GetAssociatedBase(src_ptr);
    Value* src_bound = GetAssociatedBound(src_ptr);

    args.push_back(dest_base);
    args.push_back(dest_bound);
    
    args.push_back(src_base);
    args.push_back(src_bound);

    CallInst::Create(m_memcopy_check, args, "", call_inst);
    return;
  }

  if(called_func->getName().find("llvm.memset") == 0){

    args.clear();
    CallSite cs(call_inst);
    Value* dest_ptr = cs.getArgument(0);
    // Whats cs.getArgrument(1) return? Why am I not using it?
    Value* size_ptr = cs.getArgument(2);

    Value* cast_size_ptr = size_ptr;
    assert(size_ptr != NULL);

    if(size_ptr->getType() != m_key_type){
      BitCastInst* bitcast = new BitCastInst(size_ptr, m_key_type, "", call_inst);
      cast_size_ptr = bitcast;
    }

    args.push_back(dest_ptr);
    args.push_back(cast_size_ptr);
    
    Value* dest_base = GetAssociatedBase(dest_ptr);
    Value* dest_bound = GetAssociatedBound(dest_ptr);
    args.push_back(dest_base);
    args.push_back(dest_bound);   

    CallInst::Create(m_memset_check, args, "", call_inst);
    return;
  }
}

void Nova::HandleMemcpy(CallInst* call_inst){
  Function* func = call_inst->getCalledFunction();
  if(!func)
    return;

  assert(func && "function is null?");

  CallSite cs(call_inst);
  Value* arg1 = cs.getArgument(0);
  Value* arg2 = cs.getArgument(1);
  Value* arg3 = cs.getArgument(2);

  SmallVector<Value*, 8> args;
  args.push_back(arg1);
  args.push_back(arg2);
  args.push_back(arg3);

  if(arg3->getType() == Type::getInt64Ty(arg3->getContext())){
    CallInst::Create(m_copy_metadata, args, "", call_inst);
  }
  else{
    //    CallInst::Create(m_copy_metadata, args, "", call_inst);
  }
  args.clear();

#if 0

  Value* arg1_base = castToVoidPtr(getAssociatedBase(arg1), call_inst);
  Value* arg1_bound = castToVoidPtr(getAssociatedBound(arg1), call_inst);
  Value* arg2_base = castToVoidPtr(getAssociatedBase(arg2), call_inst);
  Value* arg2_bound = castToVoidPtr(getAssociatedBound(arg2), call_inst);
  args.push_back(arg1);
  args.push_back(arg1_base);
  args.push_back(arg1_bound);
  args.push_back(arg2);
  args.push_back(arg2_base);
  args.push_back(arg2_bound);
  args.push_back(arg3);

  CallInst::Create(m_memcopy_check,args.begin(), args.end(), "", call_inst);

#endif
  return;
    
}

//
// Method: introduceShadowStackAllocation
//
// Description: For every function call that has a pointer argument or
// a return value, shadow stack is used to propagate metadata. This
// function inserts the shadow stack allocation C-handler that
// reserves space in the shadow stack by reserving the requiste amount
// of space based on the input passed to it(number of pointer
// arguments/return).


void Nova::IntroduceShadowStackAllocation(CallInst* call_inst){
    
  // Count the number of pointer arguments and whether a pointer return     
  int pointer_args_return = GetNumPointerArgsAndReturn(call_inst);
  if(pointer_args_return == 0)
    return;
  Value* total_ptr_args;    
  total_ptr_args = 
    ConstantInt::get(Type::getInt32Ty(call_inst->getType()->getContext()), 
                     pointer_args_return, false);

  SmallVector<Value*, 8> args;
  args.push_back(total_ptr_args);
  CallInst::Create(m_shadow_stack_allocate, args, "", call_inst);
}

//
// Method: introduceShadowStackStores
//
// Description: This function inserts a call to the shadow stack store
// C-handler that stores the metadata, before the function call in the
// bitcode for pointer arguments.

void Nova::IntroduceShadowStackStores(Value* ptr_value, 
                                              Instruction* insert_at, 
                                              int arg_no){
  if(!isa<PointerType>(ptr_value->getType()))
    return;

  Value* argno_value;    
  argno_value = 
    ConstantInt::get(Type::getInt32Ty(ptr_value->getType()->getContext()), 
                     arg_no, false);

  Value* ptr_base = GetAssociatedBase(ptr_value);
  Value* ptr_bound = GetAssociatedBound(ptr_value);
  
  Value* ptr_base_cast = CastToVoidPtr(ptr_base, insert_at);
  Value* ptr_bound_cast = CastToVoidPtr(ptr_bound, insert_at);

  SmallVector<Value*, 8> args;
  args.push_back(ptr_base_cast);
  args.push_back(argno_value);
  CallInst::Create(m_shadow_stack_base_store, args, "", insert_at);
  
  args.clear();
  args.push_back(ptr_bound_cast);
  args.push_back(argno_value);
  CallInst::Create(m_shadow_stack_bound_store, args, "", insert_at);    
}

//
// Method: introduceShadowStackDeallocation
//
// Description: This function inserts a call to the C-handler that
// deallocates the shadow stack space on function exit.
  

void Nova::IntroduceShadowStackDeallocation(CallInst* call_inst, Instruction* insert_at){

  int pointer_args_return = GetNumPointerArgsAndReturn(call_inst);
  if(pointer_args_return == 0)
    return;
  SmallVector<Value*, 8> args;    
  CallInst::Create(m_shadow_stack_deallocate, args, "", insert_at);
}

//
// Method: getNumPointerArgsAndReturn
//
// Description: Returns the number of pointer arguments and return.
//
int Nova::GetNumPointerArgsAndReturn(CallInst* call_inst){

  int total_pointer_count = 0;
  CallSite cs(call_inst);
  for(unsigned i = 0; i < cs.arg_size(); i++){
    Value* arg_value = cs.getArgument(i);
    if(isa<PointerType>(arg_value->getType())){
      total_pointer_count++;
    }
  }

  if (total_pointer_count != 0) {
    // Reserve one for the return address if it has atleast one
    // pointer argument 
    total_pointer_count++;
  } else{
    // Increment the pointer arg return if the call instruction
    // returns a pointer
    if(isa<PointerType>(call_inst->getType())){
      total_pointer_count++;
    }
  }
  return total_pointer_count;
}

// 
// Method: introduceShadowStackLoads
//
// Description: This function introduces calls to the C-handlers that
// performs the loads from the shadow stack to retrieve the metadata.
// This function also associates the loaded metadata with the pointer
// arguments in the SoftBound/CETS maps.

void Nova::IntroduceShadowStackLoads(Value* ptr_value, 
                                             Instruction* insert_at, 
                                             int arg_no){
    
  if (!isa<PointerType>(ptr_value->getType()))
    return;
      
  Value* argno_value;    
  argno_value = 
    ConstantInt::get(Type::getInt32Ty(ptr_value->getType()->getContext()), 
                     arg_no, false);
    
  SmallVector<Value*, 8> args;
  args.clear();
  args.push_back(argno_value);
  Value* base = CallInst::Create(m_shadow_stack_base_load, args, "", 
                                 insert_at);    
  args.clear();
  args.push_back(argno_value);
  Value* bound = CallInst::Create(m_shadow_stack_bound_load, args, "", 
                                  insert_at);
  AssociateBaseBound(ptr_value, base, bound);
}

void Nova::IterateCallSiteIntroduceShadowStackStores(CallInst* call_inst){
    
  int pointer_args_return = GetNumPointerArgsAndReturn(call_inst);

  if(pointer_args_return == 0)
    return;
    
  int pointer_arg_no = 1;

  CallSite cs(call_inst);
  for(unsigned i = 0; i < cs.arg_size(); i++){
    Value* arg_value = cs.getArgument(i);
    if(isa<PointerType>(arg_value->getType())){
      IntroduceShadowStackStores(arg_value, call_inst, pointer_arg_no);
      pointer_arg_no++;
    }
  }    
}

void Nova::HandleCallInst(CallInst* call_inst) {
  // Function* func = call_inst->getCalledFunction();
  Value* mcall = call_inst;

#if 0
  CallingConv::ID id = call_inst->getCallingConv();


  if(id == CallingConv::Fast){
    printf("fast calling convention not handled\n");
    exit(1);
  }
#endif 
    
  Function* func = call_inst->getCalledFunction();
  if(func && ((func->getName().find("llvm.memcpy") == 0) || 
              (func->getName().find("llvm.memmove") == 0))){
    AddMemcopyMemsetCheck(call_inst, func);
    HandleMemcpy(call_inst);
    return;
  }

  if(func && func->getName().find("llvm.memset") == 0){
    AddMemcopyMemsetCheck(call_inst, func);
  }

  if(func && IsFuncDefSoftBound(func->getName())){
    if(!isa<PointerType>(call_inst->getType())){
      return;
    }
    
    AssociateBaseBound(call_inst, m_void_null_ptr, m_void_null_ptr);
    return;
  }

  Instruction* insert_at = GetNextInstruction(call_inst);
  //  call_inst->setCallingConv(CallingConv::C);

  IntroduceShadowStackAllocation(call_inst);
  IterateCallSiteIntroduceShadowStackStores(call_inst);
    
  if(isa<PointerType>(mcall->getType())) {

      /* ShadowStack for the return value is 0 */
      IntroduceShadowStackLoads(call_inst, insert_at, 0);       
  }
  IntroduceShadowStackDeallocation(call_inst,insert_at);
}

//
// Method: handleSelect
//
// This function propagates the metadata with Select IR instruction.
// Select  instruction is also handled in two passes.

void Nova::HandleSelect(SelectInst* select_ins, int pass) {

  if (!isa<PointerType>(select_ins->getType())) 
    return;
    
  Value* condition = select_ins->getOperand(0);
  Value* operand_base[2];
  Value* operand_bound[2];    

  for(unsigned m = 0; m < 2; m++) {
    Value* operand = select_ins->getOperand(m+1);

    operand_base[m] = NULL;
    operand_bound[m] = NULL;

    if (CheckBaseBoundMetadataPresent(operand)) {      
      operand_base[m] = GetAssociatedBase(operand);
      operand_bound[m] = GetAssociatedBound(operand);
    }
    
    if (isa<ConstantPointerNull>(operand) && 
        !CheckBaseBoundMetadataPresent(operand)) {            
      operand_base[m] = m_void_null_ptr;
      operand_bound[m] = m_void_null_ptr;
    }        
      
    Constant* given_constant = dyn_cast<Constant>(operand);
    if(given_constant) {
      GetConstantExprBaseBound(given_constant, 
                               operand_base[m], 
                               operand_bound[m]);     
    }    
    assert(operand_base[m] != NULL && 
           "operand doesn't have base with select?");
    assert(operand_bound[m] != NULL && 
           "operand doesn't have bound with select?");
    
    // Introduce a bit cast if the types don't match 
    if (operand_base[m]->getType() != m_void_ptr_type) {          
      operand_base[m] = new BitCastInst(operand_base[m], m_void_ptr_type,
                                        "select.base", select_ins);          
    }
    
    if (operand_bound[m]->getType() != m_void_ptr_type) {
      operand_bound[m] = new BitCastInst(operand_bound[m], m_void_ptr_type,
                                         "select_bound", select_ins);
    }
  } // for loop ends
    
    SelectInst* select_base = SelectInst::Create(condition, 
                                                 operand_base[0], 
                                                 operand_base[1], 
                                                 "select.base",
                                                 select_ins);
    
    SelectInst* select_bound = SelectInst::Create(condition, 
                                                  operand_bound[0], 
                                                  operand_bound[1], 
                                                  "select.bound",
                                                  select_ins);
    AssociateBaseBound(select_ins, select_base, select_bound);
}

void Nova::HandleIntToPtr(IntToPtrInst* inttoptrinst) {
    
  Value* inst = inttoptrinst;
    
  AssociateBaseBound(inst, m_void_null_ptr, m_void_null_ptr);
}

//
// Method: handleReturnInst
//
// Description: 
// This function inserts C-handler calls to store
// metadata for return values in the shadow stack.

void Nova::HandleReturnInst(ReturnInst* ret){

  Value* pointer = ret->getReturnValue();
  if(pointer == NULL){
    return;
  }
  if(isa<PointerType>(pointer->getType())){
    IntroduceShadowStackStores(pointer, ret, 0);
  }
}

void Nova::HandleExtractElement(ExtractElementInst* EEI){
  
  if(!isa<PointerType>(EEI->getType()))
     return;
  
  Value* EEIOperand = EEI->getOperand(0);
  
  if(isa<VectorType>(EEIOperand->getType())){
    
    if(!m_vector_pointer_base.count(EEIOperand) ||
       !m_vector_pointer_bound.count(EEIOperand)){
      assert(0 && "Extract element does not have vector metadata");
    }

    Constant* index = dyn_cast<Constant>(EEI->getOperand(1));
    
    Value* vector_base = m_vector_pointer_base[EEIOperand];
    Value* vector_bound = m_vector_pointer_bound[EEIOperand];
    
    Value* ptr_base = ExtractElementInst::Create(vector_base, index, "", EEI);
    Value* ptr_bound = ExtractElementInst::Create(vector_bound, index, "", EEI);
    
    AssociateBaseBound(EEI, ptr_base, ptr_bound);
    return;
  }
     
  assert (0 && "ExtractElement is returning a pointer, possibly some vectorization going on, not handled, try running with O0 or O1 or O2");    
}

void Nova::HandleExtractValue(ExtractValueInst* EVI){
    if(isa<PointerType>(EVI->getType())){
        assert(0 && "ExtractValue is returning a pointer, possibly some vectorization going on, not handled, try running with O0 or O1 or O2");
    }
  
    AssociateBaseBound(EVI, m_void_null_ptr, m_infinite_bound_ptr);

  return;  
}

void Nova::GatherBaseBoundPass1 (Function * func) {
  int arg_count= 0;
    
  //    std::cerr<<"transforming function with name:"<<func->getName()<< "\n";
  /* Scan over the pointer arguments and introduce base and bound */

  for(Function::arg_iterator ib = func->arg_begin(), ie = func->arg_end();
      ib != ie; ++ib) {

    if(!isa<PointerType>(ib->getType())) 
      continue;

    /* it is a pointer, so increment the arg count */
    arg_count++;

    Argument* ptr_argument = dyn_cast<Argument>(ib);
    Value* ptr_argument_value = ptr_argument;
    //Instruction* fst_inst = &*(func->begin()->begin());
      
    /* Urgent: Need to think about what we need to do about byval attributes */
    if(ptr_argument->hasByValAttr()){
      
      if(CheckTypeHasPtrs(ptr_argument)){
        assert(0 && "Pointer argument has byval attributes and the underlying structure returns pointers");
      }
      
      AssociateBaseBound(ptr_argument_value, m_void_null_ptr, m_infinite_bound_ptr);
    }
    else{
      // TODOO: don't know what byval attributes means here.
      // introduceShadowStackLoads(ptr_argument_value, fst_inst, arg_count);
      //      introspectMetadata(func, ptr_argument_value, fst_inst, arg_count);
    }
  }

  /* WorkList Algorithm for propagating the base and bound. Each
   * basic block is visited only once. We start by visiting the
   * current basic block, then push all the successors of the
   * current basic block on to the queue if it has not been visited
   */
  std::set<BasicBlock*> bb_visited;
  std::queue<BasicBlock*> bb_worklist;
  Function:: iterator bb_begin = func->begin();

  BasicBlock* bb = dyn_cast<BasicBlock>(bb_begin);
  assert( bb && "Not a basic block and I am gathering base and bound?");
  bb_worklist.push(bb);

  while(bb_worklist.size() != 0) {

    bb = bb_worklist.front();
    assert(bb && "Not a BasicBlock?");

    bb_worklist.pop();
    if( bb_visited.count(bb)) {
      /* Block already visited */
      continue;
    }
    /* If here implies basic block not visited */
      
    /* Insert the block into the set of visited blocks */
    bb_visited.insert(bb);

    /* Iterating over the successors and adding the successors to
     * the work list
     */
    for(succ_iterator si = succ_begin(bb), se = succ_end(bb); si != se; ++si) {

      BasicBlock* next_bb = *si;
      assert(next_bb && "Not a basic block and I am adding to the base and bound worklist?");
      bb_worklist.push(next_bb);
    }
      
    for(BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; ++i){
        Value* v1 = dyn_cast<Value>(i);
        Instruction* new_inst = dyn_cast<Instruction>(i);


        /* If the instruction is not present in the original, no
         * instrumentaion 
         */
        if(!m_present_in_original.count(v1)) {
          continue;
        }

        /* All instructions have been defined here as defining it in
         * switch causes compilation errors. Assertions have been in
         * the inserted in the specific cases
         */
        //errs() <<"inst: " << *new_inst << "\n";

        switch(new_inst->getOpcode()) {
          
        case Instruction::Alloca:
          {
            AllocaInst* alloca_inst = dyn_cast<AllocaInst>(v1);
            assert(alloca_inst && "Not an Alloca inst?");
            HandleAlloca(alloca_inst, bb, i);
          }
          break;

        case Instruction::Load:
          {
            LoadInst* load_inst = dyn_cast<LoadInst>(v1);            
            assert(load_inst && "Not a Load inst?");
            HandleLoad(load_inst);
          }
          break;

        case Instruction::GetElementPtr:
          {
            GetElementPtrInst* gep_inst = dyn_cast<GetElementPtrInst>(v1);
            assert(gep_inst && "Not a GEP inst?");
            HandleGEP(gep_inst);
          }
          break;
	
        case BitCastInst::BitCast:
          {
            BitCastInst* bitcast_inst = dyn_cast<BitCastInst>(v1);
            assert(bitcast_inst && "Not a BitCast inst?");
            HandleBitCast(bitcast_inst);
          }
          break;

        case Instruction::PHI:
          {
            PHINode* phi_node = dyn_cast<PHINode>(v1);
            assert(phi_node && "Not a phi node?");
            //printInstructionMap(v1);
            HandlePHIPass1(phi_node);
          }
          /* PHINode ends */
          break;
          
        case Instruction::Call:
          {
            CallInst* call_inst = dyn_cast<CallInst>(v1);
            assert(call_inst && "Not a Call inst?");
            HandleCallInst(call_inst);
          }
          break;

        case Instruction::Select:
          {
            SelectInst* select_insn = dyn_cast<SelectInst>(v1);
            assert(select_insn && "Not a select inst?");
            int pass = 1;
            HandleSelect(select_insn, pass);
          }
          break;

        case Instruction::Store:
          {
            break;
          }

        case Instruction::IntToPtr:
          {
            IntToPtrInst* inttoptrinst = dyn_cast<IntToPtrInst>(v1);
            assert(inttoptrinst && "Not a IntToPtrInst?");
            HandleIntToPtr(inttoptrinst);
            break;
          }

        case Instruction::Ret:
          {
            ReturnInst* ret = dyn_cast<ReturnInst>(v1);
            assert(ret && "not a return inst?");
            HandleReturnInst(ret);
          }
          break;
	
        case Instruction::ExtractElement:
	    {
	      ExtractElementInst * EEI = dyn_cast<ExtractElementInst>(v1);
	      assert(EEI && "ExtractElementInst inst?");
	      HandleExtractElement(EEI);
	    }
	    break;

        case Instruction::ExtractValue:
	    {
	      ExtractValueInst * EVI = dyn_cast<ExtractValueInst>(v1);
	      assert(EVI && "handle extract value inst?");
	      HandleExtractValue(EVI);
	    }
	    break;
            
        default:
          if(isa<PointerType>(v1->getType()))
            assert(!isa<PointerType>(v1->getType())&&
                   " Generating Pointer and not being handled");
        } // for-end
    }/* Basic Block iterator Ends */
  } /* Function iterator Ends */
}

void Nova::HandleVectorStore(StoreInst* store_inst){

  Value* operand = store_inst->getOperand(0);
  Value* pointer_dest = store_inst->getOperand(1);
  Instruction* insert_at = GetNextInstruction(store_inst);

  if(!m_vector_pointer_base.count(operand)){
    assert(0 && "vector base not found");
  }
  if(!m_vector_pointer_bound.count(operand)){
    assert(0 && "vector bound not found");
  }
  
  Value* vector_base = m_vector_pointer_base[operand];
  Value* vector_bound = m_vector_pointer_bound[operand];

  const VectorType* vector_ty = dyn_cast<VectorType>(operand->getType());
  uint64_t num_elements = vector_ty->getNumElements();
  if (num_elements > 2){
    assert(0 && "more than 2 element vectors not handled");
  }

  Value* pointer_operand_bitcast = CastToVoidPtr(pointer_dest, insert_at);
  for (uint64_t i = 0; i < num_elements; i++){
    Constant* index = ConstantInt::get(Type::getInt32Ty(store_inst->getContext()), i);

    Value* ptr_base = ExtractElementInst::Create(vector_base, index,"", insert_at);
    Value* ptr_bound = ExtractElementInst::Create(vector_bound, index, "", insert_at);
    
    SmallVector<Value*, 8> args;
    args.clear();

    args.push_back(pointer_operand_bitcast);
    args.push_back(ptr_base);
    args.push_back(ptr_bound);
    args.push_back(index);

    CallInst::Create(m_metadata_store_vector_func, args, "", insert_at);    
  }
}   

void Nova::HandleStore(StoreInst* store_inst) {
  Value* operand = store_inst->getOperand(0);
  Value* pointer_dest = store_inst->getOperand(1);
  Instruction* insert_at = GetNextInstruction(store_inst);
    
  if(isa<VectorType>(operand->getType())){
    const VectorType* vector_ty = dyn_cast<VectorType>(operand->getType());
    if(isa<PointerType>(vector_ty->getElementType())){
      HandleVectorStore(store_inst);
      return;
    }
  }

  /* If a pointer is being stored, then the base and bound
   * corresponding to the pointer must be stored in the shadow space
   */
  if(!isa<PointerType>(operand->getType()))
    return;
      

  if(isa<ConstantPointerNull>(operand)) {
    /* it is a constant pointer null being stored
     * store null to the shadow space
     */
#if 0    
    StructType* ST = dyn_cast<StructType>(operand->getType());

    if(ST){
      if(ST->isOpaque()){
        DEBUG(errs()<<"Opaque type found\n");        
      }

    }
      Value* size_of_type = getSizeOfType(operand->getType());
#endif

      Value* size_of_type = NULL;

      AddStoreBaseBoundFunc(pointer_dest, m_void_null_ptr, 
                            m_void_null_ptr, m_void_null_ptr, 
                            size_of_type, insert_at);

    return; 
  }

      
  /* if it is a global expression being stored, then add add
   * suitable base and bound
   */
    
  Value* tmp_base = NULL;
  Value* tmp_bound = NULL;

  //  Value* xmm_base_bound = NULL;
  //  Value* xmm_key_lock = NULL;
    
  Constant* given_constant = dyn_cast<Constant>(operand);
  if(given_constant) {      
      GetConstantExprBaseBound(given_constant, tmp_base, tmp_bound);
      assert(tmp_base && "global doesn't have base");
      assert(tmp_bound && "global doesn't have bound");        
  } else {      
    /* storing an external function pointer */
      if(!CheckBaseBoundMetadataPresent(operand)) {
        return;
      }

      tmp_base = GetAssociatedBase(operand);
      tmp_bound = GetAssociatedBound(operand);              
  }    
  
  /* Store the metadata into the metadata space
   */
  

  //  Type* stored_pointer_type = operand->getType();
  Value* size_of_type = NULL;
  //    Value* size_of_type  = getSizeOfType(stored_pointer_type);
  AddStoreBaseBoundFunc(pointer_dest, tmp_base, tmp_bound, operand,  size_of_type, insert_at);    
  
}

//
// Method: getGlobalVariableBaseBound

// Description: This function returns the base and bound for the
// global variables in the input reference arguments. This function
// may now be obsolete. We should try to use getConstantExprBaseBound
// instead in all places.
void Nova::GetGlobalVariableBaseBound(Value* operand, 
                                              Value* & operand_base, 
                                              Value* & operand_bound){

  GlobalVariable* gv = dyn_cast<GlobalVariable>(operand);
  Module* module = gv->getParent();
  assert(gv && "[getGlobalVariableBaseBound] not a global variable?");
    
  std::vector<Constant*> indices_base;
  Constant* index_base = 
    ConstantInt::get(Type::getInt32Ty(module->getContext()), 0);
  indices_base.push_back(index_base);

  Constant* base_exp = ConstantExpr::getGetElementPtr(nullptr, gv, indices_base);
        
  std::vector<Constant*> indices_bound;
  Constant* index_bound = 
    ConstantInt::get(Type::getInt32Ty(module->getContext()), 1);
  indices_bound.push_back(index_bound);

  Constant* bound_exp = ConstantExpr::getGetElementPtr(nullptr, gv, indices_bound);
    
  operand_base = base_exp;
  operand_bound = bound_exp;    
}

//
// Method: handlePHIPass2()
//
// Description: This pass fills the incoming values for the metadata
// PHINodes inserted in the first pass. There are four cases that
// needs to be handled for each incoming value.  First, if the
// incoming value is a ConstantPointerNull, then base, bound, key,
// lock will be default values.  Second, the incoming value can be an
// undef which results in default metadata values.  Third, Global
// variables need to get the same base and bound for each
// occurence. So we maintain a map which maps the base and boundfor
// each global variable in the incoming value.  Fourth, by default it
// retrieves the metadata from the SoftBound/CETS maps.

// Check if we need separate global variable and constant expression
// cases.

void Nova::HandlePHIPass2(PHINode* phi_node) {
  // Work to be done only for pointer PHINodes.
  if (!isa<PointerType>(phi_node->getType())) 
    return;

  PHINode* base_phi_node = NULL;
  PHINode* bound_phi_node  = NULL;

  // Obtain the metada PHINodes 
  base_phi_node = dyn_cast<PHINode>(GetAssociatedBase(phi_node));
  bound_phi_node = dyn_cast<PHINode>(GetAssociatedBound(phi_node));

  std::map<Value*, Value*> globals_base;
  std::map<Value*, Value*> globals_bound;
 
  unsigned num_incoming_values = phi_node->getNumIncomingValues();
  for (unsigned m = 0; m < num_incoming_values; m++) {

    Value* incoming_value = phi_node->getIncomingValue(m);
    BasicBlock* bb_incoming = phi_node->getIncomingBlock(m);

    if (isa<ConstantPointerNull>(incoming_value)) {
        base_phi_node->addIncoming(m_void_null_ptr, bb_incoming);
        bound_phi_node->addIncoming(m_void_null_ptr, bb_incoming);
      continue;
    } // ConstantPointerNull ends
   
    // The incoming vlaue can be a UndefValue
    if (isa<UndefValue>(incoming_value)) {        
        base_phi_node->addIncoming(m_void_null_ptr, bb_incoming);
        bound_phi_node->addIncoming(m_void_null_ptr, bb_incoming);
      continue;
    } // UndefValue ends
      
    Value* incoming_value_base = NULL;
    Value* incoming_value_bound = NULL;
    
    // handle global variables      
    GlobalVariable* gv = dyn_cast<GlobalVariable>(incoming_value);
    if (gv) {
        if (!globals_base.count(gv)) {
          Value* tmp_base = NULL;
          Value* tmp_bound = NULL;
          GetGlobalVariableBaseBound(incoming_value, tmp_base, tmp_bound);
          assert(tmp_base && "base of a global variable null?");
          assert(tmp_bound && "bound of a global variable null?");
          
          Function * PHI_func = phi_node->getParent()->getParent();
          Instruction* PHI_func_entry = &*(PHI_func->begin()->begin());
          
          incoming_value_base = CastToVoidPtr(tmp_base, PHI_func_entry);                                               
          incoming_value_bound = CastToVoidPtr(tmp_bound, PHI_func_entry);
            
          globals_base[incoming_value] = incoming_value_base;
          globals_bound[incoming_value] = incoming_value_bound;       
        } else {
          incoming_value_base = globals_base[incoming_value];
          incoming_value_bound = globals_bound[incoming_value];          
        }
    } // global variable ends
      
    // handle constant expressions 
    Constant* given_constant = dyn_cast<Constant>(incoming_value);
    if (given_constant) {
        if (!globals_base.count(incoming_value)) {
          Value* tmp_base = NULL;
          Value* tmp_bound = NULL;
          GetConstantExprBaseBound(given_constant, tmp_base, tmp_bound);
          assert(tmp_base && tmp_bound  &&
                 "[handlePHIPass2] tmp_base tmp_bound, null?");
          
          Function* PHI_func = phi_node->getParent()->getParent();
          Instruction* PHI_func_entry = &*(PHI_func->begin()->begin());

          incoming_value_base = CastToVoidPtr(tmp_base, PHI_func_entry);
          incoming_value_bound = CastToVoidPtr(tmp_bound, PHI_func_entry);
          
          globals_base[incoming_value] = incoming_value_base;
          globals_bound[incoming_value] = incoming_value_bound;        
        }
        else{
          incoming_value_base = globals_base[incoming_value];
          incoming_value_bound = globals_bound[incoming_value];          
        }
    }
    
    // handle values having map based pointer base and bounds 
    if(CheckBaseBoundMetadataPresent(incoming_value)){
      incoming_value_base = GetAssociatedBase(incoming_value);
      incoming_value_bound = GetAssociatedBound(incoming_value);
    }

    assert(incoming_value_base &&
           "[handlePHIPass2] incoming_value doesn't have base?");
    assert(incoming_value_bound && 
           "[handlePHIPass2] incoming_value doesn't have bound?");
    
    base_phi_node->addIncoming(incoming_value_base, bb_incoming);
    bound_phi_node->addIncoming(incoming_value_bound, bb_incoming);
  } // Iterating over incoming values ends 

  assert(base_phi_node && "[handlePHIPass2] base_phi_node null?");
  assert(bound_phi_node && "[handlePHIPass2] bound_phi_node null?");

  unsigned n_values = phi_node->getNumIncomingValues();
  unsigned n_base_values = base_phi_node->getNumIncomingValues();
  unsigned n_bound_values = bound_phi_node->getNumIncomingValues();    
  assert((n_values == n_base_values)  && 
         "[handlePHIPass2] number of values different for base");
  assert((n_values == n_bound_values) && 
         "[handlePHIPass2] number of values different for bound");
}
  

void Nova::GatherBaseBoundPass2(Function* func){

  /* WorkList Algorithm for propagating base and bound. Each basic
   * block is visited only once
   */
  std::set<BasicBlock*> bb_visited;
  std::queue<BasicBlock*> bb_worklist;
  Function::iterator bb_begin = func->begin();

  BasicBlock* bb = dyn_cast<BasicBlock>(bb_begin);
  assert(bb && "Not a basic block and gathering base bound in the next pass?");
  bb_worklist.push(bb);
    
  while( bb_worklist.size() != 0) {

    bb = bb_worklist.front();
    assert(bb && "Not a BasicBlock?");

    bb_worklist.pop();
    if( bb_visited.count(bb)) {
      /* Block already visited */

      continue;
    }
    /* If here implies basic block not visited */
      
    /* Insert the block into the set of visited blocks */
    bb_visited.insert(bb);

    /* Iterating over the successors and adding the successors to
     * the work list
     */
    for(succ_iterator si = succ_begin(bb), se = succ_end(bb); si != se; ++si) {

      BasicBlock* next_bb = *si;
      assert(next_bb && "Not a basic block and I am adding to the base and bound worklist?");
      bb_worklist.push(next_bb);
    }

    for(BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; ++i) {
      Value* v1 = dyn_cast<Value>(i);
      Instruction* new_inst = dyn_cast<Instruction>(i);

      // If the instruction is not present in the original, no instrumentaion
      if(!m_present_in_original.count(v1))
        continue;

      switch(new_inst->getOpcode()) {

      case Instruction::GetElementPtr:
        {
          GetElementPtrInst* gep_inst = dyn_cast<GetElementPtrInst>(v1);         
          assert(gep_inst && "Not a GEP instruction?");
          HandleGEP(gep_inst);
        }
        break;
          
      case Instruction::Store:
        {
          StoreInst* store_inst = dyn_cast<StoreInst>(v1);
          assert(store_inst && "Not a Store instruction?");
          HandleStore(store_inst);
        }
        break;

      case Instruction::PHI:
        {
          PHINode* phi_node = dyn_cast<PHINode>(v1);
          assert(phi_node && "Not a PHINode?");
          HandlePHIPass2(phi_node);
        }
        break;
 
      case BitCastInst::BitCast:
        {
          BitCastInst* bitcast_inst = dyn_cast<BitCastInst>(v1);
          assert(bitcast_inst && "Not a bitcast instruction?");
          HandleBitCast(bitcast_inst);
        }
        break;

      case SelectInst::Select:
        {
        }
        break;
          
      default:
        break;
      }/* Switch Ends */
    }/* BasicBlock iterator Ends */
  }/* Function iterator Ends */
}

void Nova::AddBaseBoundGlobals(Module& M){
  /* iterate over the globals here */

  for(Module::global_iterator it = M.global_begin(), ite = M.global_end(); it != ite; ++it){
    
    GlobalVariable* gv = dyn_cast<GlobalVariable>(it);
    
    if(!gv){
      continue;
    }

    if(StringRef(gv->getSection()) == "llvm.metadata"){
      continue;
    }
    if(gv->getName() == "llvm.global_ctors"){
      continue;
    }
    
    if(!gv->hasInitializer())
      continue;
    
    /* gv->hasInitializer() is true */
    
    Constant* initializer = dyn_cast<Constant>(it->getInitializer());
    ConstantArray* constant_array = dyn_cast<ConstantArray>(initializer);
    
    if(initializer && isa<CompositeType>(initializer->getType())){

      if(isa<StructType>(initializer->getType())){
        std::vector<Constant*> indices_addr_ptr;
        Constant* index1 = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);
        indices_addr_ptr.push_back(index1);
        StructType* struct_type = dyn_cast<StructType>(initializer->getType());
        HandleGlobalStructTypeInitializer(M, struct_type, initializer, gv, indices_addr_ptr, 1);
        continue;
      }
      
      if(isa<SequentialType>(initializer->getType())){
        HandleGlobalSequentialTypeInitializer(M, gv);
      }
    }
    
    if(initializer && !constant_array){
      
      if(isa<PointerType>(initializer->getType())){
        //        std::cerr<<"Pointer type initializer\n";
      }
    }
    
    if(!constant_array)
      continue;
    
    int num_ca_opds = constant_array->getNumOperands();
    
    for(int i = 0; i < num_ca_opds; i++){
      Value* initializer_opd = constant_array->getOperand(i);
      Instruction* first = GetGlobalInitInstruction(M);
      Value* operand_base = NULL;
      Value* operand_bound = NULL;
      
      Constant* global_constant_initializer = dyn_cast<Constant>(initializer_opd);
      if(!isa<PointerType>(global_constant_initializer->getType())){
        break;
      }
      GetConstantExprBaseBound(global_constant_initializer, operand_base, operand_bound);
      
      SmallVector<Value*, 8> args;
      Constant* index1 = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);
      Constant* index2 = ConstantInt::get(Type::getInt32Ty(M.getContext()), i);

      std::vector<Constant*> indices_addr_ptr;
      indices_addr_ptr.push_back(index1);
      indices_addr_ptr.push_back(index2);

      Constant* addr_of_ptr = ConstantExpr::getGetElementPtr(nullptr, gv, indices_addr_ptr);
      Type* initializer_type = initializer_opd->getType();
      Value* initializer_size = GetSizeOfType(initializer_type);
      
      AddStoreBaseBoundFunc(addr_of_ptr, operand_base, operand_bound, initializer_opd, initializer_size, first);
      
    }
  }

}

//
//
// Method: addLoadStoreChecks
//
// Description: This function inserts calls to C-handler spatial
// safety check functions and elides the check if the map says it is
// not necessary to check.

void Nova::AddLoadStoreChecks(Instruction* load_store, 
                                      std::map<Value*, int>& FDCE_map) {
  SmallVector<Value*, 8> args;
  Value* pointer_operand = NULL;
    
  if(isa<LoadInst>(load_store)) {
    LoadInst* ldi = dyn_cast<LoadInst>(load_store);
    assert(ldi && "not a load instruction");
    pointer_operand = ldi->getPointerOperand();
  }
    
  if(isa<StoreInst>(load_store)){
    StoreInst* sti = dyn_cast<StoreInst>(load_store);
    assert(sti && "not a store instruction");
    // The pointer where the element is being stored is the second
    // operand
    pointer_operand = sti->getOperand(1);
  }
    
  assert(pointer_operand && "pointer operand null?");

  // If it is a null pointer which is being loaded, then it must seg
  // fault, no dereference check here
  
  
  if(isa<ConstantPointerNull>(pointer_operand))
    return;

  // Find all uses of pointer operand, then check if it dominates and
  //if so, make a note in the map
  
  GlobalVariable* gv = dyn_cast<GlobalVariable>(pointer_operand);    
  if(gv && GLOBALCONSTANTOPT && !isa<SequentialType>(gv->getType())) {
    return;
  }
  
  if(BOUNDSCHECKOPT) {
    // Enable dominator based dereference check optimization only when
    // suggested
    
    if(FDCE_map.count(load_store)) {
      return;
    }
    
    // FIXME: Add more comments here Iterate over the uses
    
    for(Value::use_iterator ui = pointer_operand->use_begin(), 
          ue = pointer_operand->use_end(); 
        ui != ue; ++ui) {
      
      Instruction* temp_inst = dyn_cast<Instruction>(*ui);       
      if(!temp_inst)
        continue;
      
      if(temp_inst == load_store)
        continue;
      
      if(!isa<LoadInst>(temp_inst) && !isa<StoreInst>(temp_inst))
        continue;
      
      if(isa<StoreInst>(temp_inst)){
        if(temp_inst->getOperand(1) != pointer_operand){
          // When a pointer is a being stored at at a particular
          // address, don't elide the check
          continue;
        }
      }
      
#if 0
        if(m_dominator_tree->dominates(load_store, temp_inst)) {
          if(!FDCE_map.count(temp_inst)) {
            FDCE_map[temp_inst] = true;
            continue;
          }                  
        }
#endif
    } // Iterating over uses ends 
  } // BOUNDSCHECKOPT ends 
    
  Value* tmp_base = NULL;
  Value* tmp_bound = NULL;
    
  Constant* given_constant = dyn_cast<Constant>(pointer_operand);    
  if(given_constant ) {
    if(GLOBALCONSTANTOPT)
      return;      

    GetConstantExprBaseBound(given_constant, tmp_base, tmp_bound);
  }
  else {
    tmp_base = GetAssociatedBase(pointer_operand);
    tmp_bound = GetAssociatedBound(pointer_operand);
  }

  Value* bitcast_base = CastToVoidPtr(tmp_base, load_store);
  args.push_back(bitcast_base);
  
  Value* bitcast_bound = CastToVoidPtr(tmp_bound, load_store);    
  args.push_back(bitcast_bound);
   
  Value* cast_pointer_operand_value = CastToVoidPtr(pointer_operand, 
                                                    load_store);    
  args.push_back(cast_pointer_operand_value);
    
  // Pushing the size of the type 
  Type* pointer_operand_type = pointer_operand->getType();
  Value* size_of_type = GetSizeOfType(pointer_operand_type);
  args.push_back(size_of_type);

  if(isa<LoadInst>(load_store)){
            
    CallInst::Create(m_spatial_load_dereference_check, args, "", load_store);
  }
  else{    
    CallInst::Create(m_spatial_store_dereference_check, args, "", load_store);
  }

  return;
}

void Nova::AddDereferenceChecks(Function* func, ValueSet &vs) {
  Function &F = *func;
  Value* pointer_operand = NULL;
  
  if(func->isVarArg())
    return;

#if 0
  if(Blacklist->isIn(F))
    return;

#endif

  std::vector<Instruction*> CheckWorkList;
  std::map<Value*, bool> ElideSpatialCheck;
  std::map<Value*, bool> ElideTemporalCheck;

  // identify all the instructions where we need to insert the spatial checks
  for(inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i){
    Instruction* I = &*i;

    if(!m_present_in_original.count(I)){
      continue;
    }
    // add check optimizations here
    // add checks for memory fences and atomic exchanges
    if(isa<LoadInst>(I) || isa<StoreInst>(I)){
      CheckWorkList.push_back(I);
    }     
    if(isa<AtomicCmpXchgInst>(I) || isa<AtomicRMWInst>(I)){
      assert(0 && "Atomic Instructions not handled");
    }    
  }

#if 0
  // spatial check optimizations here 

  for(std::vector<Instruction*>::iterator i = CheckWorkList.begin(), 
	e = CheckWorkList.end(); i!= e; ++i){

    Instruction* inst = *i;
    Value* pointer_operand = NULL;
    
    if(ElideSpatialCheck.count(inst))
      continue;
    
    if(isa<LoadInst>(inst)){
      LoadInst* ldi = dyn_cast<LoadInst>(inst);
      pointer_operand = ldi->getPointerOperand();
    }
    if(isa<StoreInst>(inst)){
      StoreInst* st = dyn_cast<StoreInst>(inst);
      pointer_operand = st->getOperand(1);      
    }

    for(Value::use_iterator ui = pointer_operand->use_begin(),  
	  ue = pointer_operand->use_end();
	ui != ue; ++ui){

      Instruction* use_inst = dyn_cast<Instruction>(*ui);
      if(!use_inst || (use_inst == inst))
	continue;

      if(!isa<LoadInst>(use_inst)  && !isa<StoreInst>(use_inst))
	continue;

      if(isa<StoreInst>(use_inst)){
	if(use_inst->getOperand(1) != pointer_operand)
	  continue;
      }

#if 0
      if(m_dominator_tree->dominates(inst, use_inst)){
	if(!ElideSpatialCheck.count(use_inst))
	  ElideSpatialCheck[use_inst] = true;		
      }
    }
#endif  
  }

#endif

  //Temporal Check Optimizations

  
#if 0

#endif

  /* intra-procedural load dererference check elimination map */
  std::map<Value*, int> func_deref_check_elim_map;
  std::map<Value*, int> func_temporal_check_elim_map;

  /* WorkList Algorithm for adding dereference checks. Each basic
   * block is visited only once. We start by visiting the current
   * basic block, then pushing all the successors of the current
   * basic block on to the queue if it has not been visited
   */
    
  std::set<BasicBlock*> bb_visited;
  std::queue<BasicBlock*> bb_worklist;
  Function:: iterator bb_begin = func->begin();

  BasicBlock* bb = dyn_cast<BasicBlock>(bb_begin);
  assert(bb && "Not a basic block  and I am adding dereference checks?");
  bb_worklist.push(bb);

    
  while(bb_worklist.size() != 0) {
      
    bb = bb_worklist.front();
    assert(bb && "Not a BasicBlock?");
    bb_worklist.pop();

    if(bb_visited.count(bb)) {
      /* Block already visited */
      continue;
    }

    /* If here implies basic block not visited */
    /* Insert the block into the set of visited blocks */
    bb_visited.insert(bb);

    /* Iterating over the successors and adding the successors to
     * the worklist
     */
    for(succ_iterator si = succ_begin(bb), se = succ_end(bb); si != se; ++si) {
        
      BasicBlock* next_bb = *si;
      assert(next_bb && "Not a basic block and I am adding to the base and bound worklist?");
      bb_worklist.push(next_bb);
    }

    /* basic block load deref check optimization */
    std::map<Value*, int> bb_deref_check_map;
    std::map<Value*, int> bb_temporal_check_elim_map;
    /* structure check optimization */
    std::map<Value*, int> bb_struct_check_opt;

    for(BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; ++i){
      Value* v1 = dyn_cast<Value>(i);
      Instruction* new_inst = dyn_cast<Instruction>(i);
      
      /* Do the dereference check stuff */
      if(!m_present_in_original.count(v1))
        continue;
      
      if(isa<LoadInst>(new_inst)){
        LoadInst* ldi = dyn_cast<LoadInst>(new_inst);
        pointer_operand = ldi->getPointerOperand();
        if (vs.count(pointer_operand) == 0) {
            //errs() << "skip check for non-sensitive pointers\n" ;
            continue;
        }
        AddLoadStoreChecks(new_inst, func_deref_check_elim_map);
        continue;
      }

      if(isa<StoreInst>(new_inst)){
        StoreInst* st = dyn_cast<StoreInst>(new_inst);
        pointer_operand = st->getOperand(1);      
        if (vs.count(pointer_operand) == 0) {
            //errs() << "skip check for non-sensitive pointers\n" ;
            continue;
        }
        AddLoadStoreChecks(new_inst, func_deref_check_elim_map);
        continue;
      }

      /* check call through function pointers */
      if(isa<CallInst>(new_inst)) {
          
        // ZC:COMMENT OUT CALL CHECK DUE TO COMPLAINS ABOUT NO BASE AND BOUND INFO
        if(CALLCHECKS) {
          continue;
        }          
	  

        SmallVector<Value*, 8> args;
        CallInst* call_inst = dyn_cast<CallInst>(new_inst);
        Value* tmp_base = NULL;
        Value* tmp_bound = NULL;
        
        assert(call_inst && "call instruction null?");
        errs() << "DEBUG: " << *call_inst;
        
        if(!INDIRECTCALLCHECKS)
          continue;

        /* TODO:URGENT : indirect function call checking commented
         * out for the time being to test other aspect of the code,
         * problem was with spec benchmarks perl and h264. They were
         * primarily complaining that the use of a function did not
         * have base and bound in the map
         */


        /* here implies its an indirect call */
        Value* indirect_func_called = call_inst->getOperand(0);
            
        Constant* func_constant = dyn_cast<Constant>(indirect_func_called);
        if(func_constant) {
          GetConstantExprBaseBound(func_constant, tmp_base, tmp_bound);           
        }
        else {
          tmp_base = GetAssociatedBase(indirect_func_called);
          tmp_bound = GetAssociatedBound(indirect_func_called);
        }
        /* Add BitCast Instruction for the base */
        Value* bitcast_base = CastToVoidPtr(tmp_base, new_inst);
        args.push_back(bitcast_base);
            
        /* Add BitCast Instruction for the bound */
        Value* bitcast_bound = CastToVoidPtr(tmp_bound, new_inst);
        args.push_back(bitcast_bound);
        Value* pointer_operand_value = CastToVoidPtr(indirect_func_called, new_inst);
        args.push_back(pointer_operand_value);            
        CallInst::Create(m_call_dereference_func, args, "", new_inst);
        continue;
      } /* Call check ends */
    }
  }  
}

//
// Method: transformMain()
//
// Description:
//
// This method renames the function "main" in the module as
// pseudo_main. The C-handler has the main function which calls
// pseudo_main. Actually transformation of the main takes places in
// two steps.  Step1: change the name to pseudo_main and Step2:
// Function renaming to append the function name with softboundcets_
//
// Inputs:
// module: Input module with the function main
//
// Outputs:
//
// Changed module with any function named "main" is changed to
// "pseudo_main"
//
// Comments:
//
// This function is doing redundant work. We should probably use
// renameFunction to accomplish the task. The key difference is that
// transform renames it the function as either pseudo_main or
// softboundcets_pseudo_main which is subsequently renamed to
// softboundcets_pseudo_main in the first case by renameFunction
//

void Nova::TransformMain(Module& module) {
    
  Function* main_func = module.getFunction("main");

  // 
  // If the program doesn't have main then don't do anything
  //
  if (!main_func) return;

  Type* ret_type = main_func->getReturnType();
  const FunctionType* fty = main_func->getFunctionType();
  std::vector<Type*> params;

  SmallVector<AttributeSet, 8> param_attrs_vec;
  const AttributeSet& pal = main_func->getAttributes();

  //
  // Get the attributes of the return value
  //

  if(pal.hasAttributes(AttributeSet::ReturnIndex))
    param_attrs_vec.push_back(AttributeSet::get(main_func->getContext(), pal.getRetAttributes()));

  // Get the attributes of the arguments 
  int arg_index = 1;
  for(Function::arg_iterator i = main_func->arg_begin(), 
        e = main_func->arg_end();
      i != e; ++i, arg_index++) {
    params.push_back(i->getType());

    AttributeSet attrs = pal.getParamAttributes(arg_index);

    if(attrs.hasAttributes(arg_index)){
      AttrBuilder B(attrs, arg_index);
      param_attrs_vec.push_back(AttributeSet::get(main_func->getContext(), params.size(), B));
    }
  }

  FunctionType* nfty = FunctionType::get(ret_type, params, fty->isVarArg());
  Function* new_func = NULL;

  // create the new function 
  new_func = Function::Create(nfty, main_func->getLinkage(), 
                              "softboundcets_pseudo_main");

  // set the new function attributes 
  new_func->copyAttributesFrom(main_func);
  new_func->setAttributes(AttributeSet::get(main_func->getContext(), param_attrs_vec));
    
  main_func->getParent()->getFunctionList().insert(main_func->getIterator(), new_func);
  main_func->replaceAllUsesWith(new_func);

  // 
  // Splice the instructions from the old function into the new
  // function and set the arguments appropriately
  // 
  new_func->getBasicBlockList().splice(new_func->begin(), 
                                       main_func->getBasicBlockList());
  Function::arg_iterator arg_i2 = new_func->arg_begin();
  for(Function::arg_iterator arg_i = main_func->arg_begin(), 
        arg_e = main_func->arg_end(); 
      arg_i != arg_e; ++arg_i) {      
    arg_i->replaceAllUsesWith(&*arg_i2);
    arg_i2->takeName(&*arg_i);
    ++arg_i2;
    arg_index++;
  }  
  //
  // Remove the old function from the module
  //
  main_func->eraseFromParent();
}

void Nova::PrepareForBoundsCheck(Module &M, ValueSet &vs) {
    TransformMain(M);
    IdentifyFuncToTrans(M);
    IdentifyInitialGlobals(M);
    AddBaseBoundGlobals(M);

    for(Module::iterator ff_begin = M.begin(), ff_end = M.end(); 
        ff_begin != ff_end; ++ff_begin){
      Function* func_ptr = dyn_cast<Function>(ff_begin);
      assert(func_ptr && "Not a function??");
      
      //
      // No instrumentation for functions introduced by us for updating
      // and retrieving the shadow space
      //
        
      if (!CheckIfFunctionOfInterest(func_ptr)) {
        continue;
      }  
      //
      // Iterating over the instructions in the function to identify IR
      // instructions in the original program In this pass, the pointers
      // in the original program are also identified
      //
        
      IdentifyOriginalInst(func_ptr);
        
      //
      // Iterate over all basic block and then each insn within a basic
      // block We make two passes over the IR for base and bound
      // propagation and one pass for dereference checks
      //
  
      GatherBaseBoundPass1(func_ptr);
      GatherBaseBoundPass2(func_ptr);
      //AddDereferenceChecks(func_ptr, vs);
    }
}

void Nova::PointerBoundaryCheck(Module &M, ValueSet &vs) {
//    Value *v;

    errs() << __func__ << " : "<< "\n";

    // Note! we almost need to fully implement Softbound here, the
    // only difference is that we selectively add check for sensitive vars.
    // we simplify the implementation as follows:
    //  1. we only support ARM64bit architecture.
    //  2. we only support spatial safty check and don't support temporal safty.
    //  3. we don't support shadow stack.
    // Attention:
    //  1. we might need to transform all functions that take pointer as params
    //     or ret_type is pointer.
    //  2. such functions can't be exported to outside module due to the transformation.
    //  3. library calls, leave it as it is.
    PrepareForBoundsCheck(M, vs);

    // identify pointer types
#if 0
    for (ValueSet::iterator it = vs.begin(), ie = vs.end();
                                              it != ie; ++it) {
        v = *it;
        errs() <<"var :" << (*v) << "\n";
        if (v->getType()->isPointerTy()) {
            errs() << v->getName() << " is pointer \n";
            if (v->getType()->getPointerElementType()->isPointerTy()) {
                errs() << v->getName() << " points to a pointer type\n";
                //PointerAccessCheck(M, v);
                ArrayAccessCheck(M, v);
            } else if (v->getType()->getPointerElementType()->isArrayTy()) {
                errs() << v->getName() << " points to a array type\n";
                ArrayAccessCheck(M, v);
            }
        }
    }
#endif

    return;
}

// enforce def-use check based on analysis result stored in gs
void Nova::DefUseCheck(Module &M, GlobalStateRef gs) {
    ValueSet senVarSet;
    Value *v;
    AliasMapRef aliasMap;
    TupleSet *ts = NULL;
    AliasObjectSet *aos = NULL;
    unsigned int offset;

    // step1: get the scope of sensitive variables
    // start from annotated vars, get all its alias vars
    for(ValueSet::iterator it = gs->senVarSet->begin(), ie = gs->senVarSet->end();
                                                        it != ie; ++it) {
        v = *it;
        assert(v != NULL);
        senVarSet.insert(v);

        ts = (*(gs->pMap))[v];

        //assert(ts != NULL);
        if (ts == NULL) {
            errs() << "NOTE: can't find tupleSet in pointsToMap for v = " << *v;
            continue;
        }

        for (TupleSet::iterator tsit = ts->begin(), tsie = ts->end();
                                             tsit != tsie; ++tsit) {
            if (*tsit == NULL || (*tsit)->ao == NULL) {
                errs() << "*tsit == NULL || *tsit->ao == NULL, skip!" << "\n";
                continue;
            }

            aliasMap = (*tsit)->ao->aliasMap;
            offset = (*tsit)->offset;
            if (aliasMap != NULL && aliasMap->find(offset) != aliasMap->end()) {
                aos = (*aliasMap)[offset];
                if (aos == NULL) {
                    errs() << "aos == NULL!\n";
                    continue;
                }

                for (AliasObjectSet::iterator aosit = aos->begin(), aosie = aos->end();
                                                    aosit != aosie; ++aosit) {
                    if ((*aosit) == NULL) {
                        errs() << "aosit == NULL!\n";
                        continue;
                    }
                    senVarSet.insert((*aosit)->val);
                }
            } else {
                //errs() << "location object ?" << "\n";
            }

            if ((*tsit)->ao->type != NULL && (*tsit)->ao->type->isStructTy()) {
                // print struct field aliasMap
                for (AliasMap::iterator ait = aliasMap->begin(), aie = aliasMap->end();
                                                ait != aie; ++ait) {
                    offset = ait->first;
                    aos = ait->second;

                    if (aos == NULL) {
                        errs() << "aos == NULL!\n";
                        continue;
                    }

                    for (AliasObjectSet::iterator aosit = aos->begin(), aosie = aos->end();
                                                        aosit != aosie; ++aosit) {
                        if ((*aosit) == NULL) {
                            errs() << "aosit == NULL!\n";
                            continue;
                        }
                        senVarSet.insert((*aosit)->val);
                    }
                }
            }
        }
    }

    // scan over all vars, put those whose alias is in the senVar set into senVar, too
    for (PointsToMap::iterator it = gs->pMap->begin(), ie = gs->pMap->end();
                                                         it != ie; ++it) { 
        // skip sensitive var, they are already in senVarSet.
        if (senVarSet.count(it->first) != 0)
            continue;

        //errs() << it->first->getName() << " : " << "\n";

        if (it->second == NULL) {
            errs() << "it->second == NULL\n";
            continue;
        }

        for (TupleSet::iterator tsit = it->second->begin(), tsie = it->second->end();
                                                    tsit != tsie; ++tsit) {
            if ((*tsit) == NULL || (*tsit)->ao == NULL) {
                errs() << "(*tsit)->ao == NULL!\n";
                continue;
            }

            //errs() << "(" << (*tsit)->offset << ", " << (*tsit)->ao->val->getName() << ")" << "\n";
            aliasMap = (*tsit)->ao->aliasMap;
            offset = (*tsit)->offset;
            if (aliasMap != NULL && aliasMap->find(offset) != aliasMap->end()) {
                aos = (*aliasMap)[offset];
                if (aos == NULL) {
                        errs() << "aos == NULL"<<"\n";
                        continue;
                }
                //errs() << "alias object set: ";
                for (AliasObjectSet::iterator aosit = aos->begin(), aosie = aos->end();
                                                    aosit != aosie; ++aosit) {
                    //errs() << (*aosit)->val->getName() << ",";
                    if (senVarSet.count((*aosit)->val) != 0) {
                        // var contains sensitive var as its alias object, so add it to senVarSet
                        errs() << " added to senVarSet!!\n";
                        if (senVarSet.count(it->first) == 0) {
                            senVarSet.insert(it->first);
                        }

                        break; // end the inner for-loop
                    }
                }
                //errs() << "\n";
            } else {
                //errs() << "location object ?" << "\n";
            }

            if ((*tsit)->ao->type != NULL && (*tsit)->ao->type->isStructTy()) {
                // print struct field aliasMap
                //errs() << "struct internal alias map: \n";
                for (AliasMap::iterator ait = aliasMap->begin(), aie = aliasMap->end();
                                                ait != aie; ++ait) {
                    offset = ait->first;
                    aos = ait->second;
                    //errs() << "alias object set at offset " << offset << " : ";
                    for (AliasObjectSet::iterator aosit = aos->begin(), aosie = aos->end();
                                                        aosit != aosie; ++aosit) {
                        //errs() << (*aosit)->val->getName() << ",";
                        if (senVarSet.count((*aosit)->val) != 0) {
                            // var contains sensitive var as its alias object, so add it to senVarSet
                            errs() << " added to senVarSet!!\n";
                            if (senVarSet.count(it->first) == 0) {
                                senVarSet.insert(it->first);
                            }

                            break; // end the inner for-loop
                        }
                    }
                    //errs() << "\n";
                }
            }
        }
        //errs() << "\n";
    }


    // step2: for all sensitive pointers, add boundary check
    //PointerBoundaryCheck(M, senVarSet);

    // step3: for all sensitive variables
    //          do def/use check
    errs() << "List PointsToMap vars:" <<"\n";
#ifdef INSTRUMENT_ALL
    for (PointsToMap::iterator it = gs->pMap->begin(), ie = gs->pMap->end();
                                                         it != ie; ++it) { 
        errs() <<"PointsToMap element:"<<(it->first)->getName() <<"\n";
        RecordDefineEvent(M, (it->first));
        CheckUseEvent(M, (it->first));
    }
#elif defined(INSTRUMENT_HALF)
    int i = 0;
    for (PointsToMap::iterator it = gs->pMap->begin(), ie = gs->pMap->end();
                                                         it != ie; ++it) { 
        if (i++ % 2 == 0)
            continue;
        errs() <<(it->first)->getName() <<"\n";
        RecordDefineEvent(M, (it->first));
        CheckUseEvent(M, (it->first));
    }
#else
    errs() << "senVarSet : \n";
    for (ValueSet::iterator it = senVarSet.begin(), ie = senVarSet.end();
                                    it != ie; ++it) {
        errs() <<(*it)->getName() <<"\n";
        RecordDefineEvent(M, *it);
        CheckUseEvent(M, *it);
    }
#endif

    return;
}

// For var, there are two cases:
//  # int var;  store val, var
//  # int *var; load tmp, var; store val, tmp;
// Explanation: 
//  Because var could be normal variable or a pointer
//  For normal variable, we consider define event as a store inst using var as address
//  For pointer variable,  we also consider pointer based write, which first load var's
//  value into tmp var, then use tmp var as address to write.
void Nova::RecordDefineEvent(Module &M, Value *var) {
    Value *op, *val;
    for (User *UoV : var->users()) {
        errs()<<"RecordDefineEvent: UoV:" << *UoV <<"\n";
        if (Instruction *Inst = dyn_cast<Instruction>(UoV)) {
            errs()<<"RecordDefineEvent: Inst:" << *Inst <<"\n";
            // normal variable
            if (isa<StoreInst>(Inst)){
                op = cast<StoreInst>(Inst)->getPointerOperand();
                val = cast<StoreInst>(Inst)->getValueOperand();
                if (op == var) {
                    // define event :insert call to void record_defevt(uint64 addr, uint64 val)
                    InstrumentStoreInst(Inst, op, val);
                }
            } else if (isa<LoadInst>(Inst)) {
                for (User *UoL : Inst->users()) {
                    if (Instruction *Inst1 = dyn_cast<Instruction>(UoL)) {
                        if (isa<StoreInst>(Inst1)){
                            val = cast<StoreInst>(Inst1)->getValueOperand();
                            op = cast<StoreInst>(Inst1)->getPointerOperand();
                            if (op == Inst) {
                                InstrumentStoreInst(Inst1, op, val);
                            }
                        }
                    } else {
                        //errs() <<"    " << UoL->getName() << " is not an instruction\n";
                    }
                }
            }
        }
    }
}

void Nova::InstrumentStoreInst(Instruction *inst, Value *addr, Value *val) {
    IRBuilder<> B(inst);
    Module *M = B.GetInsertBlock()->getModule();
    Type *VoidTy = B.getVoidTy();
    Type *I64Ty = B.getInt64Ty();
    Value *castAddr, *castVal;

    //errs() << __func__ << " : "<< *inst << "\n";
    define_event_count++;

    Constant *RecordDefEvt = M->getOrInsertFunction("__record_defevt", VoidTy,
                                                                      I64Ty,
                                                                      I64Ty, 
                                                                      nullptr);

    Function *RecordDefEvtFunc = cast<Function>(RecordDefEvt);
    castAddr = CastInst::Create(Instruction::PtrToInt, addr, I64Ty, "recptrtoint", inst);

    if (val->getType()->isPointerTy()) {
        castVal = CastInst::Create(Instruction::PtrToInt, val, I64Ty, "recptrtoint", inst);
    } else if (val->getType()->isIntegerTy(64)) {
        castVal = val;
    } else if (val->getType()->isIntegerTy()) {
        castVal = CastInst::Create(Instruction::ZExt, val, I64Ty, "reczexttoi64", inst);
    } else
	return;

    B.CreateCall(RecordDefEvtFunc, {castAddr, castVal});

    return;
}

void Nova::InstrumentLoadInst(Instruction *inst, Value *addr, Value *val) {
    IRBuilder<> B(inst);
    Module *M = B.GetInsertBlock()->getModule();
    Type *VoidTy = B.getVoidTy();
    Type *I64Ty = B.getInt64Ty();
    Value *castAddr, *castVal;

    use_event_count++;
    //errs() << __func__ << " inst: "<< *inst << "\n";
    //errs() << __func__ << " addr->name: "<< addr->getName() << "\n";
    //errs() << __func__ << " val->name: "<< val->getName() << "\n";

    Constant *CheckUseEvt = M->getOrInsertFunction("__check_useevt", VoidTy,
                                                                     I64Ty,
                                                                     I64Ty, 
                                                                     nullptr);

    Function *CheckUseEvtFunc = cast<Function>(CheckUseEvt);
    castAddr = CastInst::Create(Instruction::PtrToInt, addr, I64Ty, "chkptrtoint", inst);

    if (val->getType()->isPointerTy()) {
        //errs() << "val cast from pointer to int64\n";
        //errs() << "val:"<< *val;
        castVal = CastInst::Create(Instruction::PtrToInt, val, I64Ty, "chkptrtoint", inst);
    } else if (val->getType()->isIntegerTy(64)) {
        castVal = val; 
    } else if (val->getType()->isIntegerTy()) {
        //errs() << "val cast from other to int64\n";
        //errs() << "val:"<< *val;
        castVal = CastInst::Create(Instruction::ZExt, val, I64Ty, "chkzexttoi64", inst);
    } else
        return;

    B.CreateCall(CheckUseEvtFunc, {castAddr, castVal});

    // remove fake inst
    inst->eraseFromParent();

    return;
}

void Nova::CheckUseEvent(Module &M, Value *var) {
    LLVMContext& ctxt = M.getContext();
    Value *op;
    BasicBlock *pb;
    Instruction *fakeInst;
    Type *I64Ty = Type::getInt64Ty(ctxt);
    for (User *UoV : var->users()) {
        if (Instruction *Inst = dyn_cast<Instruction>(UoV)) {
            if (isa<LoadInst>(Inst)){
                op = cast<LoadInst>(Inst)->getPointerOperand();
                if (op == var) {
                    // define event :insert call to record_defevt(uint64 addr, uint64 val)
                    fakeInst = CastInst::Create(Instruction::PtrToInt, op, I64Ty, "fptrtoint");
                    pb = Inst->getParent();
                    assert(pb != nullptr);
                    pb->getInstList().insertAfter(Inst->getIterator(), fakeInst);
                    InstrumentLoadInst(fakeInst, op, Inst);
                }
            }
        }
    }
}

//void Nova::extendSenVarSet(Module &M, SenObjSet &exSenVarSet) {
//    GlobalVariable *gv;
//    StructType *st;
//    unsigned numTypes, bitWidth, i;
//    std::queue<SenObj *> exSenVarQueue;
//    SenObj *pSenObj, *nSenObj;
//    Value *var, *exvar, *val;
//
//    // Initial Set is all the global variables
//    for (Module::global_iterator s = M.global_begin(), 			\
//          e = M.global_end(); s != e; ++s) {
//
//        // construct Sensitive Object from Value *
//        gv = &(*s);
//        pSenObj = new SenObj();
//        pSenObj->val = (Value *)gv;
//        if (gv->getType()->getPointerElementType()->isPointerTy()) {
//            pSenObj->isPointer = true;
//            // TODOO: get initial pointsToSet from initializer of global variable.
//        } else {
//            pSenObj->isPointer = false;
//        }
//
//        // insert pSenObj into exSenVarSet & exSenVarQueue
//        exSenVarSet.insert(pSenObj);
//        exSenVarQueue.push(pSenObj);
//
//        errs() << "Global Variable: " << gv->getName() << "\n";
//        errs() << " TypeID: " <<gv->getType()->getTypeID() << "\n";
//
//        errs() << "Pointee TypeID:" << gv->getType()->getPointerElementType()->getTypeID() << "\n";
//        if (gv->getType()->getPointerElementType()->isIntegerTy()) {
//            bitWidth = gv->getType()->getPointerElementType()->getIntegerBitWidth();
//            errs() << "Type is integer, bitwidth is " << bitWidth << "\n";
//        }
//
//        if (gv->getType()->getPointerElementType()->isStructTy()) {
//            st = ((StructType *)(gv->getType()->getPointerElementType()));
//            numTypes = st->getNumElements();
//            for (i = 0; i < numTypes; i++) {
//                errs() << "Struct field ("<< i <<") TypeID:" << st->getElementType(i)->getTypeID() << "\n";
//                if (st->getElementType(i)->isIntegerTy()) {
//                    bitWidth = st->getElementType(i)->getIntegerBitWidth();
//                    errs() << "Element ("<< i <<") Type is integer, bitwidth is " << bitWidth << "\n";
//                }
//            }
//        }
//
//        errs() << "\n";
//    }
//
//    // Note! Don't forget to handle the initializer of global variables. insert_def_record(...)
//    // TODOO
//
//    // Roll the snowball to include all relevant sensitive variables following the propagation rule
//    // TODOO
//    while(!exSenVarQueue.empty()) {
//        nSenObj = exSenVarQueue.front();
//        exSenVarQueue.pop();
//        var = nSenObj->val;
//
//        // Get all the users of var
//        for (User *UoV : var->users()) {
//            if (Instruction *Inst = dyn_cast<Instruction>(UoV)) {
//                errs() << var->getName() <<" is used in instruction:\n";
//                errs() << *Inst << "\n";
//
//                // From propagation rule 1: find exvar follow load-a-store-b chain.
//                // From propagation rule 2: find exvar follow store-a-ptr chain.
//                if (isa<LoadInst>(Inst)){
//                    for (User *UoL : Inst->users()) {
//                        if (Instruction *Inst1 = dyn_cast<Instruction>(UoL)) {
//                            errs() << "    " << "cur inst is used in instruction:\n";
//                            errs() << "    " << *Inst1 << "\n";
//
//                            if (isa<StoreInst>(Inst1)){
//                                val = cast<StoreInst>(Inst1)->getValueOperand();
//                                exvar = cast<StoreInst>(Inst1)->getPointerOperand();
//                                if (val == Inst) {
//                                    // TODOO check existence of exvar, whether exvar has already been added ?
//                                    pSenObj = new SenObj();
//                                    pSenObj->val = exvar;
//                                    pSenObj->isPointer = nSenObj->isPointer; // var and exvar are of same type
//                                    // TODOO Copy pointsToSet from nSenObj to pSenObj
//                                    exSenVarQueue.push(pSenObj);
//                                    exSenVarSet.insert(pSenObj);
//                                }
//
//                                errs() << "    " << "     " << "store val :" << *val << "\n";
//                                errs() << "    " << "     " << "store operand :" << exvar->getName() << "\n";
//                            }
//                        } else {
//                            errs() <<"    " << UoL->getName() << " is not an instruction\n";
//                        }
//                    }
//                } else if(isa<StoreInst>(Inst)) {
//                    val = cast<StoreInst>(Inst)->getValueOperand();
//                    exvar = cast<StoreInst>(Inst)->getPointerOperand();
//                    if (val == var) {
//                        exSenVarQueue.push(exvar);
//                        exSenVarSet.insert(exvar);
//
//                        errs() << "    " << "val:" << *val << "\n";
//                        errs() << "    " << "var:" << *var << "\n";
//                        errs() << "    " << "exvar:" << *exvar << "\n";
//                    } else {
//                        errs() << "    " << "val:" << *val << "\n";
//                        errs() << "    " << "var:" << *var << "\n";
//                    }
//                }
//            } else {
//                errs() << "UoV is not an instruction:\n";
//                errs() << *UoV<<"\n";
//            }
//        }
//
//        errs() << "\n";
//
//    }
//
//    errs() << "extended sensitive var set: \n";
//    for (auto v :exSenVarSet) {
//        errs() << (*v).getName()<<"\n";
//    }
//}

char Nova::ID = 0;
static RegisterPass<Nova> X("nova", "Nova Module Pass", false, false);
