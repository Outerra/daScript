#pragma once

#include "daScript/simulate/simulate.h"

namespace das
{
    using namespace std;

	#define DAS_HASH_EMPTY32	0
	#define DAS_HASH_KILLED32	1

	__forceinline uint32_t hash_block32(uint8_t * block, size_t size) {
		uint32_t crc = 0xFFFFFFFF;
		while(size--) {
			crc = v_crc32u8(crc, *block++);
		}
		return crc <= DAS_HASH_KILLED32 ? 16777619 : crc;
	}

	__forceinline uint32_t hash_blockz32(uint8_t * block) {
		uint32_t crc = 0xFFFFFFFF;
		while(*block) {
			crc = v_crc32u8(crc, *block++);
		}
		return crc <= DAS_HASH_KILLED32 ? 16777619 : crc;
	}

    __forceinline uint32_t hash_function ( const void * x, size_t size ) {
		return hash_block32((uint8_t *)x, size);
    }

    template <typename TT>
    __forceinline uint32_t hash_function ( TT x ) {
        uint32_t crc = (uint32_t) hash<TT>()(x);
		return crc <= DAS_HASH_KILLED32 ? 16777619 : crc;
    }

    template <>
    __forceinline uint32_t hash_function ( Block b ) {
        return hash_function(&b, sizeof(b));
    }

    template <>
    __forceinline uint32_t hash_function ( char * x ) {
		return hash_blockz32((uint8_t *)x);
    }

    template <typename QQ>
    __forceinline uint32_t hash_function ( const vec2<QQ> & x) {
        return hash_function(&x, sizeof(vec2<QQ>));
    }

    template <typename QQ>
    __forceinline uint32_t hash_function ( const vec3<QQ> & x) {
        return hash_function(&x, sizeof(vec3<QQ>));
    }

    template <typename QQ>
    __forceinline uint32_t hash_function ( const vec4<QQ> & x) {
        return hash_function(&x, sizeof(vec4<QQ>));
    }

    template <typename QQ>
    __forceinline uint32_t hash_function ( const RangeType<QQ> & x) {
        return hash_function(&x, sizeof(RangeType<QQ>));
    }

    uint32_t hash_function ( void * data, TypeInfo * type );

    template <typename TT>
    struct SimNode_HashOfValue : SimNode {
        SimNode_HashOfValue ( const LineInfo & at, SimNode * s ) : SimNode(at), subexpr(s) {}
        virtual vec4f eval ( Context & context ) override {
            // note, exception point not nessaray. wrong value is still ok to cast
            TT res = cast<TT>::to(subexpr->eval(context));
            return cast<uint32_t>::from ( hash_function(res) );
        }
        SimNode * subexpr;
    };

    struct SimNode_HashOfRef : SimNode {
        SimNode_HashOfRef ( const LineInfo & at, SimNode * s, uint32_t sz ) : SimNode(at), subexpr(s), size(sz) {}
        virtual vec4f eval ( Context & context ) override {
            char * data = cast<char *>::to(subexpr->eval(context));
            DAS_EXCEPTION_POINT;
            return cast<uint32_t>::from ( hash_function(data,size) );
        }
        SimNode *   subexpr;
        uint32_t    size;
    };

    struct SimNode_HashOfMixedType : SimNode {
        SimNode_HashOfMixedType ( const LineInfo & at, SimNode * s, TypeInfo *t ) : SimNode(at), subexpr(s), typeInfo(t) {}
        virtual vec4f eval ( Context & context ) override {
            char * data = cast<char *>::to(subexpr->eval(context));
            DAS_EXCEPTION_POINT;
            return cast<uint32_t>::from ( hash_function(data,typeInfo) );
        }
        SimNode *   subexpr;
        TypeInfo *  typeInfo;
    };
}

