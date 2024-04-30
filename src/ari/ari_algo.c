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
#include "ari_algo.h"
#include "ari_containers.h"
#include "util.h"
#include <math.h>

static int ari_visit_ari(const ari_t *ari, const ari_visitor_t *visitor, const ari_visit_ctx_t *ctx);

static int ari_visit_ac(const ari_ac_t *obj, const ari_visitor_t *visitor, const ari_visit_ctx_t *ctx)
{
    int retval;

    ari_list_it_t it;
    for (ari_list_it(it, obj->items); !ari_list_end_p(it); ari_list_next(it))
    {
        const ari_list_subtype_ct *item = ari_list_cref(it);

        retval = ari_visit_ari(*item, visitor, ctx);
        CHKERRVAL(retval);
    }
    return 0;
}

static int ari_visit_am(const ari_am_t *obj, const ari_visitor_t *visitor, ari_visit_ctx_t *ctx)
{
    int retval;

    ari_dict_it_t it;
    for (ari_dict_it(it, obj->items); !ari_dict_end_p(it); ari_dict_next(it))
    {
        const ari_dict_subtype_ct *pair = ari_dict_cref(it);

        ctx->is_map_key = true;
        retval          = ari_visit_ari(pair->key, visitor, ctx);
        CHKERRVAL(retval);

        ctx->is_map_key = false;
        retval          = ari_visit_ari(pair->value, visitor, ctx);
        CHKERRVAL(retval);
    }
    return 0;
}

static int ari_visit_tbl(const ari_tbl_t *obj, const ari_visitor_t *visitor, const ari_visit_ctx_t *ctx)
{
    int retval;

    ari_array_it_t it;
    for (ari_array_it(it, obj->items); !ari_array_end_p(it); ari_array_next(it))
    {
        const ari_array_subtype_ct *item = ari_array_cref(it);

        retval = ari_visit_ari(*item, visitor, ctx);
        CHKERRVAL(retval);
    }
    return 0;
}

static int ari_visit_execset(const ari_execset_t *obj, const ari_visitor_t *visitor, const ari_visit_ctx_t *ctx)
{
    int retval;

    ari_list_it_t it;
    for (ari_list_it(it, obj->targets); !ari_list_end_p(it); ari_list_next(it))
    {
        const ari_list_subtype_ct *item = ari_list_cref(it);

        retval = ari_visit_ari(*item, visitor, ctx);
        CHKERRVAL(retval);
    }
    return 0;
}

static int ari_visit_report(const ari_report_t obj, const ari_visitor_t *visitor, const ari_visit_ctx_t *ctx)
{
    int retval;

    retval = ari_visit_ari(&(obj->reltime), visitor, ctx);
    CHKERRVAL(retval);

    retval = ari_visit_ari(&(obj->source), visitor, ctx);
    CHKERRVAL(retval);

    ari_list_it_t it;
    for (ari_list_it(it, obj->items); !ari_list_end_p(it); ari_list_next(it))
    {
        const ari_list_subtype_ct *item = ari_list_cref(it);

        retval = ari_visit_ari(*item, visitor, ctx);
        CHKERRVAL(retval);
    }
    return 0;
}

static int ari_visit_rptset(const ari_rptset_t *obj, const ari_visitor_t *visitor, const ari_visit_ctx_t *ctx)
{
    int retval;

    retval = ari_visit_ari(&(obj->nonce), visitor, ctx);
    CHKERRVAL(retval);

    retval = ari_visit_ari(&(obj->reftime), visitor, ctx);
    CHKERRVAL(retval);

    ari_report_list_it_t it;
    for (ari_report_list_it(it, obj->reports); !ari_report_list_end_p(it); ari_report_list_next(it))
    {
        const ari_report_list_subtype_ct *item = ari_report_list_cref(it);

        retval = ari_visit_report(*item, visitor, ctx);
        CHKERRVAL(retval);
    }
    return 0;
}

static int ari_visit_ari(const ari_t *ari, const ari_visitor_t *visitor, const ari_visit_ctx_t *ctx)
{
    int retval;

    // visit main ARI first
    if (visitor->visit_ari)
    {
        retval = visitor->visit_ari(ari, ctx);
        CHKERRVAL(retval);
    }

    ari_visit_ctx_t sub_ctx = {
        .parent    = ari,
        .user_data = ctx->user_data,
    };

    // dive into contents
    if (ari->is_ref)
    {
        if (visitor->visit_ref)
        {
            retval = visitor->visit_ref(&(ari->as_ref), &sub_ctx);
            CHKERRVAL(retval);
        }

        if (visitor->visit_objpath)
        {
            retval = visitor->visit_objpath(&(ari->as_ref.objpath), &sub_ctx);
            CHKERRVAL(retval);
        }

        switch (ari->as_ref.params.state)
        {
            case ARI_PARAMS_NONE:
                break;
            case ARI_PARAMS_AC:
            {
                retval = ari_visit_ac(ari->as_ref.params.as_ac, visitor, &sub_ctx);
                CHKERRVAL(retval);
                break;
            }
            case ARI_PARAMS_AM:
            {
                {
                    retval = ari_visit_am(ari->as_ref.params.as_am, visitor, &sub_ctx);
                    CHKERRVAL(retval);
                    break;
                }
            }
            default:
                return 0;
        }
    }
    else
    {
        if (visitor->visit_lit)
        {
            retval = visitor->visit_lit(&(ari->as_lit), &sub_ctx);
            CHKERRVAL(retval);
        }

        if (ari->as_lit.has_ari_type)
        {
            switch (ari->as_lit.ari_type)
            {
                case ARI_TYPE_AC:
                    retval = ari_visit_ac(ari->as_lit.value.as_ac, visitor, &sub_ctx);
                    CHKERRVAL(retval);
                    break;
                case ARI_TYPE_AM:
                    retval = ari_visit_am(ari->as_lit.value.as_am, visitor, &sub_ctx);
                    CHKERRVAL(retval);
                    break;
                case ARI_TYPE_TBL:
                    retval = ari_visit_tbl(ari->as_lit.value.as_tbl, visitor, &sub_ctx);
                    CHKERRVAL(retval);
                    break;
                case ARI_TYPE_EXECSET:
                    retval = ari_visit_execset(ari->as_lit.value.as_execset, visitor, &sub_ctx);
                    CHKERRVAL(retval);
                    break;
                case ARI_TYPE_RPTSET:
                    retval = ari_visit_rptset(ari->as_lit.value.as_rptset, visitor, &sub_ctx);
                    CHKERRVAL(retval);
                    break;
                default:
                    break;
            }
        }
    }

    return retval;
}

int ari_visit(const ari_t *ari, const ari_visitor_t *visitor, void *user_data)
{
    CHKERR1(ari);
    CHKERR1(visitor);

    ari_visit_ctx_t sub_ctx = {
        .parent    = NULL,
        .user_data = user_data,
    };
    return ari_visit_ari(ari, visitor, &sub_ctx);
}

static int ari_map_ac(ari_ac_t *out, const ari_ac_t *in, const ari_translator_t *translator, void *user_data)
{
    int           retval;
    ari_list_it_t it;
    for (ari_list_it(it, in->items); !ari_list_end_p(it); ari_list_next(it))
    {
        const ari_list_subtype_ct *in_item  = ari_list_cref(it);
        ari_t                     *out_item = *ari_list_push_back_new(out->items);
        retval                              = ari_translate(out_item, *in_item, translator, user_data);
        CHKERRVAL(retval);
    }
    return 0;
}

static int ari_map_am(ari_am_t *out, const ari_am_t *in, const ari_translator_t *translator, void *user_data)
{
    int           retval;
    ari_dict_it_t it;
    for (ari_dict_it(it, in->items); !ari_dict_end_p(it); ari_dict_next(it))
    {
        const ari_dict_subtype_ct *pair = ari_dict_cref(it);

        ari_t out_key  = ARI_INIT_UNDEFINED;
        retval         = ari_translate(&out_key, pair->key, translator, user_data);
        ari_t *out_val = *ari_dict_safe_get(out->items, &out_key);
        ari_deinit(&out_key);
        CHKERRVAL(retval);

        retval = ari_translate(out_val, pair->value, translator, user_data);
        CHKERRVAL(retval);
    }
    return 0;
}

static int ari_map_tbl(ari_tbl_t *out, const ari_tbl_t *in, const ari_translator_t *translator, void *user_data)
{
    int retval;

    out->ncols = in->ncols;

    ari_array_it_t it;
    for (ari_array_it(it, in->items); !ari_array_end_p(it); ari_array_next(it))
    {
        const ari_array_subtype_ct *in_item  = ari_array_cref(it);
        ari_a1_t                    out_item = { ARI_INIT_UNDEFINED };
        retval                               = ari_translate(out_item, *in_item, translator, user_data);
        ari_array_push_move(out->items, &out_item);
        CHKERRVAL(retval);
    }
    return 0;
}

int ari_translate(ari_t *out, const ari_t *in, const ari_translator_t *translator, void *user_data)
{
    CHKERR1(out);
    CHKERR1(in);
    CHKERR1(translator);
    int retval;

    // handle main ARI first
    if (translator->map_ari)
    {
        retval = translator->map_ari(out, in, user_data);
        CHKERRVAL(retval);
    }
    else
    {
        out->is_ref = in->is_ref;
    }

    if (in->is_ref)
    {
        if (translator->map_objpath)
        {
            retval = translator->map_objpath(&(out->as_ref.objpath), &(in->as_ref.objpath), user_data);
            CHKERRVAL(retval);
        }
        else
        {
            ari_objpath_copy(&(out->as_ref.objpath), &(in->as_ref.objpath));
        }

        switch (in->as_ref.params.state)
        {
            case ARI_PARAMS_NONE:
                out->as_ref.params.state = ARI_PARAMS_NONE;
                break;
            case ARI_PARAMS_AC:
            {
                ari_lit_t tmp;
                ari_lit_init_container(&tmp, ARI_TYPE_AC);
                retval = ari_map_ac(tmp.value.as_ac, in->as_ref.params.as_ac, translator, user_data);
                if (retval)
                {
                    ari_lit_deinit(&tmp);
                    return retval;
                }
                out->as_ref.params.state = ARI_PARAMS_NONE;
                out->as_ref.params.as_ac = tmp.value.as_ac;
                break;
            }
            case ARI_PARAMS_AM:
            {
                {
                    ari_lit_t tmp;
                    ari_lit_init_container(&tmp, ARI_TYPE_AM);
                    retval = ari_map_am(tmp.value.as_am, in->as_ref.params.as_am, translator, user_data);
                    if (retval)
                    {
                        ari_lit_deinit(&tmp);
                        return retval;
                    }
                    out->as_ref.params.state = ARI_PARAMS_NONE;
                    out->as_ref.params.as_ac = tmp.value.as_ac;
                    break;
                }
            }
            default:
                return 0;
        }
    }
    else
    {
        if (translator->map_lit)
        {
            retval = translator->map_lit(&(out->as_lit), &(in->as_lit), user_data);
            CHKERRVAL(retval);
        }
        else
        {
            ari_lit_copy(&(out->as_lit), &(in->as_lit));
        }

        if (in->as_lit.has_ari_type)
        {
            switch (in->as_lit.ari_type)
            {
                case ARI_TYPE_AC:
                    ari_lit_init_container(&(out->as_lit), ARI_TYPE_AC);
                    retval = ari_map_ac(out->as_lit.value.as_ac, in->as_lit.value.as_ac, translator, user_data);
                    CHKERRVAL(retval);
                    break;
                case ARI_TYPE_AM:
                {
                    ari_lit_init_container(&(out->as_lit), ARI_TYPE_AM);
                    retval = ari_map_am(out->as_lit.value.as_am, in->as_lit.value.as_am, translator, user_data);
                    CHKERRVAL(retval);
                    break;
                }
                case ARI_TYPE_TBL:
                {
                    ari_lit_init_container(&(out->as_lit), ARI_TYPE_TBL);
                    retval = ari_map_tbl(out->as_lit.value.as_tbl, in->as_lit.value.as_tbl, translator, user_data);
                    CHKERRVAL(retval);
                    break;
                }
                default:
                    break;
            }
        }
    }

    return retval;
}

static int ari_hash_visit_objpath(const ari_objpath_t *path, const ari_visit_ctx_t *ctx)
{
    size_t *accum = ctx->user_data;
    M_HASH_UP(*accum, ari_idseg_hash(&(path->ns_id)));
    if (path->has_ari_type)
    {
        M_HASH_UP(*accum, path->ari_type);
    }
    else
    {
        M_HASH_UP(*accum, ari_idseg_hash(&(path->type_id)));
    }
    M_HASH_UP(*accum, ari_idseg_hash(&(path->obj_id)));
    return 0;
}

static int ari_hash_visit_lit(const ari_lit_t *obj, const ari_visit_ctx_t *ctx)
{
    size_t *accum = ctx->user_data;

    M_HASH_UP(*accum, M_HASH_DEFAULT(obj->has_ari_type));
    if (obj->has_ari_type)
    {
        M_HASH_UP(*accum, M_HASH_DEFAULT(obj->ari_type));
        switch (obj->ari_type)
        {
            case ARI_TYPE_TBL:
                // include metadata
                M_HASH_UP(*accum, M_HASH_DEFAULT(obj->value.as_tbl->ncols));
                break;
            default:
                // container contents are visited separately
                break;
        }
    }
    M_HASH_UP(*accum, M_HASH_DEFAULT(obj->prim_type));
    switch (obj->prim_type)
    {
        case ARI_PRIM_UNDEFINED:
        case ARI_PRIM_NULL:
            break;
        case ARI_PRIM_BOOL:
            M_HASH_UP(*accum, M_HASH_POD_DEFAULT(obj->value.as_bool));
            break;
        case ARI_PRIM_UINT64:
            M_HASH_UP(*accum, M_HASH_DEFAULT(obj->value.as_uint64));
            break;
        case ARI_PRIM_INT64:
            M_HASH_UP(*accum, M_HASH_DEFAULT(obj->value.as_int64));
            break;
        case ARI_PRIM_FLOAT64:
            M_HASH_UP(*accum, M_HASH_DEFAULT(obj->value.as_float64));
            break;
        case ARI_PRIM_TSTR:
        case ARI_PRIM_BSTR:
            M_HASH_UP(*accum, ari_data_hash(&(obj->value.as_data)));
            break;
        case ARI_PRIM_TIMESPEC:
            M_HASH_UP(*accum, M_HASH_POD_DEFAULT(obj->value.as_timespec));
            break;
        default:
            break;
    }

    return 0;
}

size_t ari_hash(const ari_t *ari)
{
    CHKRET(ari, 0);
    static const ari_visitor_t visitor = {
        .visit_objpath = ari_hash_visit_objpath,
        .visit_lit     = ari_hash_visit_lit,
    };

    M_HASH_DECL(accum);
    ari_visit(ari, &visitor, &accum);
    accum = M_HASH_FINAL(accum);
    return accum;
}

static bool ari_objpath_equal(const ari_objpath_t *left, const ari_objpath_t *right)
{
    // prefer derived values
    bool type_equal;
    if (left->has_ari_type && right->has_ari_type)
    {
        type_equal = left->ari_type == right->ari_type;
    }
    else
    {
        type_equal = ari_idseg_equal(&(left->type_id), &(right->type_id));
    }

    return (ari_idseg_equal(&(left->ns_id), &(right->ns_id)) && type_equal
            && ari_idseg_equal(&(left->obj_id), &(right->obj_id)));
}

static bool ari_params_equal(const ari_params_t *left, const ari_params_t *right)
{
    if (left->state != right->state)
    {
        return false;
    }
    switch (left->state)
    {
        case ARI_PARAMS_NONE:
            return true;
        case ARI_PARAMS_AC:
            return ari_ac_equal(left->as_ac, right->as_ac);
        case ARI_PARAMS_AM:
            return ari_am_equal(left->as_am, right->as_am);
        default:
            return false;
    }
}

bool ari_equal(const ari_t *left, const ari_t *right)
{
    CHKFALSE(left);
    CHKFALSE(right);
    if (left->is_ref != right->is_ref)
    {
        return false;
    }
    if (left->is_ref)
    {
        return (ari_objpath_equal(&(left->as_ref.objpath), &(right->as_ref.objpath))
                && ari_params_equal(&(left->as_ref.params), &(right->as_ref.params)));
    }
    else
    {
        if (left->as_lit.has_ari_type != right->as_lit.has_ari_type)
        {
            return false;
        }
        if (left->as_lit.has_ari_type)
        {
            if (left->as_lit.ari_type != right->as_lit.ari_type)
            {
                return false;
            }
            switch (left->as_lit.ari_type)
            {
                case ARI_TYPE_AC:
                    if (!ari_ac_equal(left->as_lit.value.as_ac, right->as_lit.value.as_ac))
                    {
                        return false;
                    }
                    break;
                case ARI_TYPE_AM:
                    if (!ari_am_equal(left->as_lit.value.as_am, right->as_lit.value.as_am))
                    {
                        return false;
                    }
                    break;
                case ARI_TYPE_TBL:
                    if (!ari_tbl_equal(left->as_lit.value.as_tbl, right->as_lit.value.as_tbl))
                    {
                        return false;
                    }
                    break;
                case ARI_TYPE_EXECSET:
                    if (!ari_execset_equal(left->as_lit.value.as_execset, right->as_lit.value.as_execset))
                    {
                        return false;
                    }
                    break;
                case ARI_TYPE_RPTSET:
                    if (!ari_rptset_equal(left->as_lit.value.as_rptset, right->as_lit.value.as_rptset))
                    {
                        return false;
                    }
                    break;
                default:
                    break;
            }
        }
        if (left->as_lit.prim_type != right->as_lit.prim_type)
        {
            return false;
        }
        switch (left->as_lit.prim_type)
        {
            case ARI_PRIM_UNDEFINED:
            case ARI_PRIM_NULL:
                break;
            case ARI_PRIM_BOOL:
                if (left->as_lit.value.as_bool != right->as_lit.value.as_bool)
                {
                    return false;
                }
                break;
            case ARI_PRIM_UINT64:
                if (left->as_lit.value.as_uint64 != right->as_lit.value.as_uint64)
                {
                    return false;
                }
                break;
            case ARI_PRIM_INT64:
                if (left->as_lit.value.as_int64 != right->as_lit.value.as_int64)
                {
                    return false;
                }
                break;
            case ARI_PRIM_FLOAT64:
                if (isnan(left->as_lit.value.as_float64) != isnan(right->as_lit.value.as_float64))
                {
                    return false;
                }
                if (!isnan(left->as_lit.value.as_float64)
                    && (left->as_lit.value.as_float64 != right->as_lit.value.as_float64))
                {
                    return false;
                }
                break;
            case ARI_PRIM_TSTR:
            case ARI_PRIM_BSTR:
                if (!ari_data_equal(&(left->as_lit.value.as_data), &(right->as_lit.value.as_data)))
                {
                    return false;
                }
                break;
            case ARI_PRIM_TIMESPEC:
                if (!M_MEMCMP1_DEFAULT(left->as_lit.value.as_timespec, right->as_lit.value.as_timespec))
                {
                    return false;
                }
                break;
            default:
                break;
        }
        return true;
    }
}
