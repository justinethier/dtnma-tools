/*
 * Copyright (c) 2011-2023 The Johns Hopkins University Applied Physics
 * Laboratory LLC.
 *
 * This file is part of the Delay-Tolerant Networking Management
 * Architecture (DTNMA) Tools package.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "ari_type.h"
#include "util.h"
#include <m-dict.h>
#include <pthread.h>
#include <stddef.h>
#include <strings.h>

typedef struct
{
    ari_type_t  type;
    const char *name;
} _ari_type_name_pair_t;

// clang-format off
/** Map from non-negative literal type enumerations to names.
 * These are from the IANA sub-registries "Literal Types" and
 * "Managed Object Types" from @cite IANA:DTNMA.
 */
static _ari_type_name_pair_t _ari_type_names[] = {
    {ARI_TYPE_LITERAL, "LITERAL"},
    {ARI_TYPE_NULL, "NULL"},
    {ARI_TYPE_BOOL, "BOOL"},
    {ARI_TYPE_BYTE, "BYTE"},
    {ARI_TYPE_INT, "INT"},
    {ARI_TYPE_UINT, "UINT"},
    {ARI_TYPE_VAST, "VAST"},
    {ARI_TYPE_UVAST, "UVAST"},
    {ARI_TYPE_REAL32, "REAL32"},
    {ARI_TYPE_REAL64, "REAL64"},
    {ARI_TYPE_TEXTSTR, "TEXTSTR"},
    {ARI_TYPE_BYTESTR, "BYTESTR"},
    {ARI_TYPE_TP, "TP"},
    {ARI_TYPE_TD, "TD"},
    {ARI_TYPE_LABEL, "LABEL"},
    {ARI_TYPE_CBOR, "CBOR"},
    {ARI_TYPE_ARITYPE, "ARITYPE"},
    {ARI_TYPE_AC, "AC"},
    {ARI_TYPE_AM, "AM"},
    {ARI_TYPE_TBL, "TBL"},
    {ARI_TYPE_EXECSET, "EXECSET"},
    {ARI_TYPE_RPTSET, "RPTSET"},

    {ARI_TYPE_OBJECT, "OBJECT"},
    {ARI_TYPE_IDENT, "IDENT"},
    {ARI_TYPE_CONST, "CONST"},
    {ARI_TYPE_CTRL, "CTRL"},
};
// clang-format on

#ifdef ENABLE_LUT_CACHE

#define M_CASESTR_EQUAL(a, b) (strcasecmp((a), (b)) == 0)
#define M_CASESTR_OPLIST      M_OPEXTEND(M_CSTR_OPLIST, EQUAL(M_CASESTR_EQUAL), CMP(strcasecmp))

DICT_DEF2(_ari_type_by_id, ari_type_t, M_BASIC_OPLIST, const char *, M_CASESTR_OPLIST)
DICT_DEF2(_ari_type_by_name, const char *, M_CASESTR_OPLIST, ari_type_t, M_BASIC_OPLIST)

/// Cached type dictionary
static _ari_type_by_id_t   _ari_type_id_dict;
static _ari_type_by_name_t _ari_type_name_dict;

/// Initializer for #amm_builtin_dict
void _ari_type_dict_init(void)
{
    const _ari_type_name_pair_t *curs = _ari_type_names;
    const size_t                 len  = sizeof(_ari_type_names) / sizeof(_ari_type_name_pair_t);
    const _ari_type_name_pair_t *end  = curs + len;

    _ari_type_by_id_init(_ari_type_id_dict);
    _ari_type_by_name_init(_ari_type_name_dict);
    for (; curs < end; ++curs)
    {
        _ari_type_by_id_set_at(_ari_type_id_dict, curs->type, curs->name);
        _ari_type_by_name_set_at(_ari_type_name_dict, curs->name, curs->type);
    }
}

/// Guard for amm_builtin_dict_init()
static pthread_once_t _ari_type_dict_ctrl = PTHREAD_ONCE_INIT;

const char *ari_type_to_name(int32_t typenum)
{
    pthread_once(&_ari_type_dict_ctrl, _ari_type_dict_init);
    const char **found = _ari_type_by_id_get(_ari_type_id_dict, typenum);
    return found ? *found : NULL;
}

int ari_type_from_name(int32_t *typenum, const char *name)
{
    CHKERR1(name);

    pthread_once(&_ari_type_dict_ctrl, _ari_type_dict_init);
    ari_type_t *found = _ari_type_by_name_get(_ari_type_name_dict, name);
    if (typenum && found)
    {
        *typenum = *found;
    }
    return found ? 0 : 1;
}

#else

const char *ari_type_to_name(int32_t typenum)
{
    const _ari_type_name_pair_t *curs = _ari_type_names;
    const _ari_type_name_pair_t *end  = curs + sizeof(_ari_type_names) / sizeof(_ari_type_name_pair_t);
    for (; curs < end; ++curs)
    {
        if (curs->type == typenum)
        {
            return curs->name;
        }
    }
    return NULL;
}

int ari_type_from_name(int32_t *typenum, const char *name)
{
    CHKERR1(typenum);
    CHKERR1(name);

    const _ari_type_name_pair_t *curs = _ari_type_names;
    const _ari_type_name_pair_t *end  = curs + sizeof(_ari_type_names) / sizeof(_ari_type_name_pair_t);
    for (; curs < end; ++curs)
    {
        // local names guarantee cmp termination
        if (strcasecmp(curs->name, name) == 0)
        {
            return curs->type;
        }
    }
    return NULL;
}

#endif /* ENABLE_LUT_CACHE */