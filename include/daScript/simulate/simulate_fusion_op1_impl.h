#define IMPLEMENT_ANY_OP1_FUSION_POINT(INLINE,OPNAME,TYPE,CTYPE) \
    struct Op1FusionPoint_##OPNAME##_##CTYPE : FusionPointOp1 { \
        Op1FusionPoint_##OPNAME##_##CTYPE ( ) {} \
        IMPLEMENT_ANY_OP1_NODE(INLINE,OPNAME,TYPE,CTYPE,Const); \
        IMPLEMENT_ANY_OP1_NODE(INLINE,OPNAME,TYPE,CTYPE,Local); \
        IMPLEMENT_ANY_OP1_NODE(INLINE,OPNAME,TYPE,CTYPE,Argument); \
        virtual SimNode * match(const SimNodeInfoLookup & info, SimNode *, SimNode * node_x, Context * context) override { \
            if ( false ) {} \
            MATCH_ANY_OP1_NODE(CTYPE,"ConstValue",Const) \
            MATCH_ANY_OP1_NODE(CTYPE,"GetLocalR2V",Local) \
            MATCH_ANY_OP1_NODE(CTYPE,"GetArgument",Argument) \
            return nullptr; \
        } \
        virtual void set(SimNode_Op1Fusion * result, SimNode * node) override { \
            result->set(#OPNAME,Type(ToBasicType<CTYPE>::type),node->debugInfo); \
            IMPLEMENT_OP1_SETUP_NODE(result,node); \
        } \
        virtual SimNode * fuse(const SimNodeInfoLookup & info, SimNode * node, Context * context) override { \
            auto cnode = static_cast<SimNode_Op1*>(node); \
            return fuseOp1(info, node, cnode->x, context); \
        } \
    };

