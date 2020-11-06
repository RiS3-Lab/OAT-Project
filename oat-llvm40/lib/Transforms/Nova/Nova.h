#ifndef NOVA_H
#define NOVA_H

#include "llvm/ADT/SetVector.h"

#define MAX_RUNS    1

namespace llvm{
    class Module;

    struct AliasObject;
    struct AliasObjectTuple;
    struct GlobalState;
    struct CallInst;
    struct GEPOperator;
    struct GlobalValue;

    typedef SetVector<Value *> ValueSet;
    typedef SetVector<Instruction *> InstSet;

    // AliasTuple
    typedef struct AliasObjectTuple *AliasObjectTupleRef;
    typedef SetVector<AliasObjectTupleRef> TupleSet;

    // AliasObject members
    typedef struct AliasObject *AliasObjectRef;
    typedef SetVector<AliasObjectRef> AliasObjectSet;
    typedef std::map<int, AliasObjectSet *> AliasMap;
    typedef AliasMap *AliasMapRef;
    typedef std::map<int, InstSet *> LocalTaintMap;
    typedef LocalTaintMap *LocalTaintMapRef;

    // GlobalState members
    typedef std::map<Value*, InstSet *> TaintMap;
    typedef std::map<Value*, TupleSet *> PointsToMap;
    typedef std::map<Value*, bool> VisitMap;
    typedef TaintMap *TaintMapRef;
    typedef PointsToMap *PointsToMapRef;
    typedef VisitMap *VisitMapRef;

    typedef struct GlobalState *GlobalStateRef; 

    // SCC
    typedef std::vector<BasicBlock *> SCC;
    typedef std::vector<BasicBlock *> *SCCRef;

// all local/global variables and dynamically allocated objects should have an alias object
// for locations, .aliasMap = null, .taintMap = null, .val = Instruction
// for dynamic allocated object, size is used for record allocation size.
struct AliasObject {
    Value *val;
    bool isStruct;
    bool isLocation;
    Type *type;
    uint32_t size;
    AliasMapRef aliasMap;
    LocalTaintMapRef taintMap;
}; // struct AliasObject

struct AliasObjectTuple {
    int offset;
    struct AliasObject *ao;
}; // struct AliasObjectTuple

struct GlobalState {
    TaintMapRef tMap;
    PointsToMapRef pMap;
    VisitMapRef vMap;
    CallInst *ci;   // current callInst
    ValueSet *senVarSet;
}; // GlobalState

struct Nova : public ModulePass {
    static char ID;
    Nova() : ModulePass(ID) {}
    bool runOnModule(Module &M);

    // def-use check
    void GetAnnotatedVariables(Module &M, GlobalStateRef gs);
    void DefUseCheck(Module &M, GlobalStateRef gs);
    void RecordDefineEvent(Module &M, Value *var);
    void CheckUseEvent(Module &M, Value *var);
    void InstrumentStoreInst(Instruction *inst, Value *addr, Value *val);
    void InstrumentLoadInst(Instruction *inst, Value *addr, Value *val);

    // pointer boundary check
    void ConstructCheckHandlers(Module &M);
    void PointerBoundaryCheck(Module &M, ValueSet &vs);
    void PointerAccessCheck(Module &M, Value *v);
    void ArrayAccessCheck(Module &M, Value *v);
    void CollectArrayBoundaryInfo(Module &M, Value *v);
    BasicBlock::iterator GetInstIterator(Value *v);
    Value* CastToVoidPtr(Value* operand, Instruction* insert_at);
    void DissociateBaseBound(Value* pointer_operand);
    void AssociateBaseBound(Value* pointer_operand, Value* pointer_base, Value* pointer_bound);
    Value* GetAssociatedBase(Value* pointer_operand);
    Value* GetAssociatedBound(Value* pointer_operand);
    bool IsStructOperand(Value* pointer_operand);
    Value* GetSizeOfType(Type* input_type);
    void AddBaseBoundGlobalValue(Module &M, Value *v);
    void AddBaseBoundGlobals(Module &M);
    void HandleGlobalSequentialTypeInitializer(Module& M, GlobalVariable* gv);
    void AddStoreBaseBoundFunc(Value* pointer_dest, Value* pointer_base, Value* pointer_bound,
                                 Value* pointer, Value* size_of_type, Instruction* insert_at);
    void GetConstantExprBaseBound(Constant* given_constant, Value* & tmp_base, Value* & tmp_bound);
    void HandleGlobalStructTypeInitializer(Module& M, StructType* init_struct_type, Constant* initializer, 
                                  GlobalVariable* gv, std::vector<Constant*> indices_addr_ptr, int length);
    Instruction* GetGlobalInitInstruction(Module& M);
    void TransformMain(Module& module);
    void GatherBaseBoundPass1(Function* func);
    void GatherBaseBoundPass2(Function* func);
    void AddLoadStoreChecks(Instruction* load_store, std::map<Value*, int>& FDCE_map);
    void AddDereferenceChecks(Function* func, ValueSet &vs);
    void PrepareForBoundsCheck(Module &M, ValueSet &vs);
    void IdentifyFuncToTrans(Module& module);
    bool CheckIfFunctionOfInterest(Function* func);
    bool CheckTypeHasPtrs(Argument* ptr_argument);
    bool CheckPtrsInST(StructType* struct_type);
    bool CheckBaseBoundMetadataPresent(Value* pointer_operand);
    bool HasPtrArgRetType(Function* func); 
    bool IsFuncDefSoftBound(const std::string &str);
    void IdentifyInitialGlobals(Module& module);
    void IdentifyOriginalInst(Function * func);
    void HandleAlloca (AllocaInst* alloca_inst, BasicBlock* bb, BasicBlock::iterator& i);
    void HandleLoad(LoadInst* load_inst);
    void HandleGEP(GetElementPtrInst* gep_inst);
    void HandleBitCast(BitCastInst* bitcast_inst);
    void HandleMemcpy(CallInst* call_inst);
    void HandleCallInst(CallInst* call_inst);
    void HandleSelect(SelectInst* select_ins, int pass);
    void HandlePHIPass1(PHINode* phi_node);
    void HandlePHIPass2(PHINode* phi_node);
    void HandleIntToPtr(IntToPtrInst* inttoptrinst);
    void HandleReturnInst(ReturnInst* ret);
    void HandleExtractElement(ExtractElementInst* EEI);
    void HandleExtractValue(ExtractValueInst* EVI);
    void HandleVectorStore(StoreInst* store_inst);
    void HandleStore(StoreInst* store_inst);
    void InsertMetadataLoad(LoadInst* load_inst);
    void PropagateMetadata(Value* pointer_operand, Instruction* inst, int instruction_type);
    void AddMemcopyMemsetCheck(CallInst* call_inst, Function* called_func);
    void IntroduceShadowStackAllocation(CallInst* call_inst);
    void IntroduceShadowStackDeallocation(CallInst* call_inst, Instruction* insert_at);
    void IntroduceShadowStackStores(Value* ptr_value, Instruction* insert_at, int arg_no);
    void IntroduceShadowStackLoads(Value* ptr_value, Instruction* insert_at, int arg_no);
    int GetNumPointerArgsAndReturn(CallInst* call_inst);
    void GetGlobalVariableBaseBound(Value* operand, Value* & operand_base, Value* & operand_bound);
    Instruction* GetNextInstruction(Instruction* I);
    void IterateCallSiteIntroduceShadowStackStores(CallInst* call_inst);

    // SCC traversal
    void Traversal(GlobalStateRef gs, Function *f);
    void ReverseSCC(std::vector<SCCRef> &, Function *f);
    void VisitSCC(GlobalStateRef gs, SCC &scc);  
    void HandleLoop(GlobalStateRef gs, SCC &scc);  
    void HandleCall(GlobalStateRef gs, CallInst &I);  
    uint32_t LongestUseDefChain(SCC &scc);
    void DispatchClients(GlobalStateRef gs, Instruction &I);
    Function *ResolveCall(GlobalStateRef gs, CallInst &I);

    // points to analysis & taint analysis framework
    void PointsToAnalysis(GlobalStateRef gs, Instruction &I);
    void TaintAnalysis(GlobalStateRef gs, Instruction &I);
    void InitializeGS(GlobalStateRef gs, Module &M);
    void InitializeFunction(GlobalStateRef gs, Function *f, CallInst &I);
    void PrintPointsToMap(GlobalStateRef gs);
    void PrintTaintMap(GlobalStateRef gs);

    // points to analysis rules
    void UpdatePtoAlloca(GlobalStateRef gs, Instruction &I);
    void UpdatePtoBinOp(GlobalStateRef gs, Instruction &I);
    void UpdatePtoLoad(GlobalStateRef gs, Instruction &I);
    void UpdatePtoStore(GlobalStateRef gs, Instruction &I);
    void UpdatePtoGEP(GlobalStateRef gs, Instruction &I);
    void UpdatePtoRet(GlobalStateRef gs, Instruction &I);
    void UpdatePtoBitCast(GlobalStateRef gs, Instruction &I);

    // points to analysis helper functions 
    void HandlePtoGEPOperator(GlobalStateRef gs, GEPOperator *op);
    AliasObject *CreateAliasObject(Type *type, Value *v);
    void PrintAliasObject(AliasObjectRef ao);
    bool SkipStructType(Type *type);

    // taint to analysis rules
    void UpdateTaintAlloca(GlobalStateRef gs, Instruction &I);
    void UpdateTaintBinOp(GlobalStateRef gs, Instruction &I);
    void UpdateTaintLoad(GlobalStateRef gs, Instruction &I);
    void UpdateTaintStore(GlobalStateRef gs, Instruction &I);
    void UpdateTaintGEP(GlobalStateRef gs, Instruction &I);
    void UpdateTaintRet(GlobalStateRef gs, Instruction &I);
    void UpdateTaintBitCast(GlobalStateRef gs, Instruction &I);
}; // struct Nova
} // namespace

#endif //NOVA_H
