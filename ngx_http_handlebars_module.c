#include <handlebars/handlebars_compiler.h>
#include <handlebars/handlebars_delimiters.h>
#include <handlebars/handlebars_json.h>
#include <handlebars/handlebars_memory.h>
#include <handlebars/handlebars_opcode_serializer.h>
#include <handlebars/handlebars_parser.h>
#include <handlebars/handlebars_partial_loader.h>
#include <handlebars/handlebars_string.h>
#include <handlebars/handlebars_value.h>
#include <handlebars/handlebars_vm.h>
#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_flag_t enable;
} ngx_http_handlebars_context_t;

typedef struct {
    ngx_flag_t enable;
} ngx_http_handlebars_main_t;

typedef struct {
    ngx_http_complex_value_t *content;
    ngx_http_complex_value_t *json;
    ngx_http_complex_value_t *template;
    ngx_uint_t compiler_flags;
} ngx_http_handlebars_location_t;

ngx_module_t ngx_http_handlebars_module;

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

static char *ngx_http_handlebars_flags_conf(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_handlebars_location_t *location = conf;
    if (location->compiler_flags != NGX_CONF_UNSET_UINT) return "duplicate";
    ngx_str_t *args = cf->args->elts;
    static const ngx_conf_enum_t e[] = {
        { ngx_string("all"), handlebars_compiler_flag_all },
        { ngx_string("alternate_decorators"), handlebars_compiler_flag_alternate_decorators },
        { ngx_string("assume_objects"), handlebars_compiler_flag_assume_objects },
        { ngx_string("compat"), handlebars_compiler_flag_compat },
        { ngx_string("explicit_partial_context"), handlebars_compiler_flag_explicit_partial_context },
        { ngx_string("ignore_standalone"), handlebars_compiler_flag_ignore_standalone },
        { ngx_string("known_helpers_only"), handlebars_compiler_flag_known_helpers_only },
        { ngx_string("mustache_style_lambdas"), handlebars_compiler_flag_mustache_style_lambdas },
        { ngx_string("no_escape"), handlebars_compiler_flag_no_escape },
        { ngx_string("none"), handlebars_compiler_flag_none },
        { ngx_string("prevent_indent"), handlebars_compiler_flag_prevent_indent },
        { ngx_string("strict"), handlebars_compiler_flag_strict },
        { ngx_string("string_params"), handlebars_compiler_flag_string_params },
        { ngx_string("track_ids"), handlebars_compiler_flag_track_ids },
        { ngx_string("use_data"), handlebars_compiler_flag_use_data },
        { ngx_string("use_depths"), handlebars_compiler_flag_use_depths },
        { ngx_null_string, 0 }
    };
    ngx_uint_t compiler_flags = handlebars_compiler_flag_none;
    ngx_uint_t j;
    for (j = 0; e[j].name.len; j++) if (e[j].name.len == args[1].len && !ngx_strncmp(e[j].name.data, args[1].data, args[1].len)) { compiler_flags = e[j].value; break; }
    if (!e[j].name.len) { ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "\"%V\" directive error: value \"%V\" must be \"all\", \"alternate_decorators\", \"assume_objects\", \"compat\", \"explicit_partial_context\", \"ignore_standalone\", \"known_helpers_only\", \"mustache_style_lambdas\", \"no_escape\", \"none\", \"prevent_indent\", \"strict\", \"string_params\", \"track_ids\", \"use_data\" or \"use_depths\"", &cmd->name, &args[1]); return NGX_CONF_ERROR; }
    if (compiler_flags) location->compiler_flags |= compiler_flags;
    else location->compiler_flags = handlebars_compiler_flag_none;
    return NGX_CONF_OK;
}

static ngx_buf_t *ngx_http_handlebars_process(ngx_http_request_t *r, ngx_str_t json) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_http_clear_accept_ranges(r);
    ngx_http_clear_content_length(r);
    ngx_http_weak_etag(r);
    ngx_http_handlebars_location_t *location = ngx_http_get_module_loc_conf(r, ngx_http_handlebars_module);
    if (location->content && ngx_http_complex_value(r, location->content, &r->headers_out.content_type) != NGX_OK) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_complex_value != NGX_OK"); return NULL; }
    if (!r->headers_out.content_type.data) {
        ngx_http_core_loc_conf_t *core = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
        r->headers_out.content_type = core->default_type;
    }
    r->headers_out.content_type_len = r->headers_out.content_type.len;
    ngx_str_t template;
    if (ngx_http_complex_value(r, location->template, &template) != NGX_OK) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_complex_value != NGX_OK"); return NULL; }
    if (!template.len) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!template.len"); return NULL; }
    ngx_buf_t *b = NULL;
    jmp_buf jmp;
    struct handlebars_context *ctx = handlebars_context_ctor();
    if (handlebars_setjmp_ex(ctx, &jmp)) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, handlebars_error_message(ctx)); goto free; }
    struct handlebars_compiler *compiler = handlebars_compiler_ctor(ctx);
    handlebars_compiler_set_flags(compiler, location->compiler_flags);
    struct handlebars_parser *parser = handlebars_parser_ctor(ctx);
    struct handlebars_string *tmpl = handlebars_string_ctor(ctx, (const char *)template.data, template.len);
    if (location->compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    struct handlebars_ast_node *ast = handlebars_parse_ex(parser, tmpl, location->compiler_flags);
    struct handlebars_program *program = handlebars_compiler_compile_ex(compiler, ast);
    struct handlebars_module *module = handlebars_program_serialize(ctx, program);
    struct handlebars_value *input = handlebars_value_ctor(ctx);
    struct handlebars_string *buffer = handlebars_string_ctor(ctx, (const char *)json.data, json.len);
    handlebars_value_init_json_string(ctx, input, hbs_str_val(buffer));
    handlebars_value_convert(input);
    struct handlebars_value *partials = handlebars_value_partial_loader_init(ctx, handlebars_string_ctor(ctx, ".", sizeof(".") - 1), handlebars_string_ctor(ctx, ".hbs", sizeof(".hbs") - 1), handlebars_value_ctor(ctx));
    struct handlebars_vm *vm = handlebars_vm_ctor(ctx);
    handlebars_vm_set_flags(vm, location->compiler_flags);
    handlebars_vm_set_partials(vm, partials);
    buffer = talloc_steal(ctx, handlebars_vm_execute(vm, module, input));
    handlebars_vm_dtor(vm);
    handlebars_value_dtor(input);
    handlebars_value_dtor(partials);
    if (!buffer) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!buffer"); goto free; }
    ngx_str_t output = {hbs_str_len(buffer), (u_char *)hbs_str_val(buffer)};
    if (!(b = ngx_create_temp_buf(r->pool, output.len))) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!ngx_create_temp_buf"); goto free; }
    b->last_buf = 1;
    b->last = ngx_copy(b->last, output.data, output.len);
    b->memory = 1;
    if (b->last != b->end) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "b->last != b->end"); goto free; }
    if (r == r->main) {
        r->headers_out.content_length_n = b->last - b->pos;
        if (r->headers_out.content_length) {
            r->headers_out.content_length->hash = 0;
            r->headers_out.content_length = NULL;
        }
        ngx_http_weak_etag(r);
    }
free:
    handlebars_context_dtor(ctx);
    return b;
}

static ngx_int_t ngx_http_handlebars_handler(ngx_http_request_t *r) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_int_t rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK && rc != NGX_AGAIN) return rc;
    ngx_http_handlebars_location_t *location = ngx_http_get_module_loc_conf(r, ngx_http_handlebars_module);
    ngx_str_t json;
    if (ngx_http_complex_value(r, location->json, &json) != NGX_OK) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_complex_value != NGX_OK"); return NGX_HTTP_INTERNAL_SERVER_ERROR; }
    ngx_chain_t cl = {.buf = ngx_http_handlebars_process(r, json), .next = NULL};
    if (!cl.buf) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!cl.buf"); return NGX_HTTP_INTERNAL_SERVER_ERROR; }
    r->headers_out.status = NGX_HTTP_OK;
    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) return rc;
    return ngx_http_output_filter(r, &cl);
}

static char *ngx_http_set_complex_value_slot_enable(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_handlebars_main_t *main = ngx_http_conf_get_module_main_conf(cf, ngx_http_handlebars_module);
    main->enable = 1;
    return ngx_http_set_complex_value_slot(cf, cmd, conf);
}

static char *ngx_http_set_complex_value_slot_handler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_core_loc_conf_t *core = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    if (!core->handler) core->handler = ngx_http_handlebars_handler;
    return ngx_http_set_complex_value_slot(cf, cmd, conf);
}

static ngx_command_t ngx_http_handlebars_commands[] = {
  { .name = ngx_string("handlebars_flags"),
    .type = NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_1MORE,
    .set = ngx_http_handlebars_flags_conf,
    .conf = NGX_HTTP_LOC_CONF_OFFSET,
    .offset = offsetof(ngx_http_handlebars_location_t, compiler_flags),
    .post = NULL },
  { .name = ngx_string("handlebars_content"),
    .type = NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
    .set = ngx_http_set_complex_value_slot,
    .conf = NGX_HTTP_LOC_CONF_OFFSET,
    .offset = offsetof(ngx_http_handlebars_location_t, content),
    .post = NULL },
  { .name = ngx_string("handlebars_json"),
    .type = NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
    .set = ngx_http_set_complex_value_slot_handler,
    .conf = NGX_HTTP_LOC_CONF_OFFSET,
    .offset = offsetof(ngx_http_handlebars_location_t, json),
    .post = NULL },
  { .name = ngx_string("handlebars_template"),
    .type = NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
    .set = ngx_http_set_complex_value_slot_enable,
    .conf = NGX_HTTP_LOC_CONF_OFFSET,
    .offset = offsetof(ngx_http_handlebars_location_t, template),
    .post = NULL },
    ngx_null_command
};

static void *ngx_http_handlebars_create_main_conf(ngx_conf_t *cf) {
    ngx_http_handlebars_main_t *main = ngx_pcalloc(cf->pool, sizeof(*main));
    if (!main) return NULL;
    return main;
}

static void *ngx_http_handlebars_create_loc_conf(ngx_conf_t *cf) {
    ngx_http_handlebars_location_t *location = ngx_pcalloc(cf->pool, sizeof(*location));
    if (!location) return NULL;
    location->compiler_flags = NGX_CONF_UNSET_UINT;
    return location;
}

static char *ngx_http_handlebars_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_handlebars_location_t *prev = parent;
    ngx_http_handlebars_location_t *conf = child;
    if (!conf->content) conf->content = prev->content;
    if (!conf->json) conf->json = prev->json;
    if (!conf->template) conf->template = prev->template;
    ngx_conf_merge_uint_value(conf->compiler_flags, prev->compiler_flags, handlebars_compiler_flag_alternate_decorators|handlebars_compiler_flag_compat|handlebars_compiler_flag_explicit_partial_context|handlebars_compiler_flag_ignore_standalone|handlebars_compiler_flag_known_helpers_only|handlebars_compiler_flag_mustache_style_lambdas|handlebars_compiler_flag_prevent_indent|handlebars_compiler_flag_use_data|handlebars_compiler_flag_use_depths);
    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_handlebars_header_filter(ngx_http_request_t *r) {
    ngx_http_handlebars_location_t *location = ngx_http_get_module_loc_conf(r, ngx_http_handlebars_module);
    if (!location->template) return ngx_http_next_header_filter(r);
    if (!(r->headers_out.content_type.len >= sizeof("application/json") - 1 && !ngx_strncasecmp(r->headers_out.content_type.data, (u_char *)"application/json", sizeof("application/json") - 1))) return ngx_http_next_header_filter(r);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_http_handlebars_context_t *context = ngx_pcalloc(r->pool, sizeof(*context));
    if (!context) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!ngx_pcalloc"); return NGX_ERROR; }
    context->enable = 1;
    ngx_http_set_ctx(r, context, ngx_http_handlebars_module);
    r->allow_ranges = 0;
    r->main_filter_need_in_memory = 1;
    return NGX_OK;
}

static ngx_int_t ngx_http_handlebars_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
    if (!in) return ngx_http_next_body_filter(r, in);
    ngx_http_handlebars_context_t *context = ngx_http_get_module_ctx(r, ngx_http_handlebars_module);
    ngx_http_handlebars_location_t *location = ngx_http_get_module_loc_conf(r, ngx_http_handlebars_module);
    if (!location->template || !context || !context->enable) return ngx_http_next_body_filter(r, in);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_str_t json = ngx_null_string;
    for (ngx_chain_t *cl = in; cl; cl = cl->next) {
        if (!ngx_buf_in_memory(cl->buf)) continue;
        json.len += cl->buf->last - cl->buf->pos;
    }
    if (!json.len) return ngx_http_next_body_filter(r, in);
    if (!(json.data = ngx_pnalloc(r->pool, json.len))) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!ngx_pnalloc"); goto error; }
    u_char *p = json.data;
    size_t len;
    for (ngx_chain_t *cl = in; cl; cl = cl->next) {
        if (!ngx_buf_in_memory(cl->buf)) continue;
        if (!(len = cl->buf->last - cl->buf->pos)) continue;
        p = ngx_copy(p, cl->buf->pos, len);
    }
    ngx_chain_t cl = {.buf = ngx_http_handlebars_process(r, json), .next = NULL};
    if (!cl.buf) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!cl.buf"); goto error; }
    ngx_int_t rc = ngx_http_next_header_filter(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) return NGX_ERROR;
    ngx_http_set_ctx(r, NULL, ngx_http_handlebars_module);
    return ngx_http_next_body_filter(r, &cl);
error:
    ngx_http_set_ctx(r, NULL, ngx_http_handlebars_module);
    return ngx_http_filter_finalize_request(r, &ngx_http_handlebars_module, NGX_HTTP_INTERNAL_SERVER_ERROR);
}

static ngx_int_t ngx_http_handlebars_postconfiguration(ngx_conf_t *cf) {
    ngx_http_handlebars_main_t *main = ngx_http_conf_get_module_main_conf(cf, ngx_http_handlebars_module);
    if (!main->enable) return NGX_OK;
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_handlebars_header_filter;
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_handlebars_body_filter;
    return NGX_OK;
}

static ngx_http_module_t ngx_http_handlebars_ctx = {
    .preconfiguration = NULL,
    .postconfiguration = ngx_http_handlebars_postconfiguration,
    .create_main_conf = ngx_http_handlebars_create_main_conf,
    .init_main_conf = NULL,
    .create_srv_conf = NULL,
    .merge_srv_conf = NULL,
    .create_loc_conf = ngx_http_handlebars_create_loc_conf,
    .merge_loc_conf = ngx_http_handlebars_merge_loc_conf
};

ngx_module_t ngx_http_handlebars_module = {
    NGX_MODULE_V1,
    .ctx = &ngx_http_handlebars_ctx,
    .commands = ngx_http_handlebars_commands,
    .type = NGX_HTTP_MODULE,
    .init_master = NULL,
    .init_module = NULL,
    .init_process = NULL,
    .init_thread = NULL,
    .exit_thread = NULL,
    .exit_process = NULL,
    .exit_master = NULL,
    NGX_MODULE_V1_PADDING
};
