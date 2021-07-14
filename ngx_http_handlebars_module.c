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
#include <json-c/json.h>
#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_str_t content;
} ngx_http_handlebars_context_t;

typedef struct {
    ngx_http_complex_value_t *content;
    ngx_http_complex_value_t *json;
    ngx_http_complex_value_t *template;
    ngx_uint_t compiler_flags;
} ngx_http_handlebars_location_t;

ngx_module_t ngx_http_handlebars_module;

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

static ngx_int_t ngx_http_handlebars_handler(ngx_http_request_t *r) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    r->headers_out.status = NGX_HTTP_OK;
    ngx_int_t rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only); else rc = ngx_http_output_filter(r, NULL);
    return rc;
}

static char *ngx_http_set_complex_value_slot_my(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_core_loc_conf_t *core_loc_conf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    if (!core_loc_conf->handler) core_loc_conf->handler = ngx_http_handlebars_handler;
    return ngx_http_set_complex_value_slot(cf, cmd, conf);
}

static ngx_command_t ngx_http_handlebars_commands[] = {
  { .name = ngx_string("handlebars_compiler_flags"),
    .type = NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    .set = ngx_conf_set_num_slot,
    .conf = NGX_HTTP_LOC_CONF_OFFSET,
    .offset = offsetof(ngx_http_handlebars_location_t, compiler_flags),
    .post = NULL },
  { .name = ngx_string("handlebars_content"),
    .type = NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    .set = ngx_http_set_complex_value_slot,
    .conf = NGX_HTTP_LOC_CONF_OFFSET,
    .offset = offsetof(ngx_http_handlebars_location_t, content),
    .post = NULL },
  { .name = ngx_string("handlebars_json"),
    .type = NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    .set = ngx_http_set_complex_value_slot,
    .conf = NGX_HTTP_LOC_CONF_OFFSET,
    .offset = offsetof(ngx_http_handlebars_location_t, json),
    .post = NULL },
  { .name = ngx_string("handlebars_template"),
    .type = NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    .set = ngx_http_set_complex_value_slot_my,
    .conf = NGX_HTTP_LOC_CONF_OFFSET,
    .offset = offsetof(ngx_http_handlebars_location_t, template),
    .post = NULL },
    ngx_null_command
};

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
    ngx_conf_merge_uint_value(conf->compiler_flags, prev->compiler_flags, 0);
    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_handlebars_header_filter(ngx_http_request_t *r) {
    ngx_http_handlebars_location_t *location = ngx_http_get_module_loc_conf(r, ngx_http_handlebars_module);
    if (!location->json && !(r->headers_out.content_type.len >= sizeof("application/json") - 1 && !ngx_strncasecmp(r->headers_out.content_type.data, (u_char *)"application/json", sizeof("application/json") - 1))) return ngx_http_next_header_filter(r);
    if (!location->template) return ngx_http_next_header_filter(r);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_http_handlebars_context_t *context = ngx_pcalloc(r->pool, sizeof(*context));
    if (!context) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!ngx_pcalloc"); return NGX_ERROR; }
    ngx_http_set_ctx(r, context, ngx_http_handlebars_module);
    context->content = r->headers_out.content_type;
    ngx_http_clear_content_length(r);
    ngx_http_clear_accept_ranges(r);
    ngx_http_weak_etag(r);
    if (location->content && ngx_http_complex_value(r, location->content, &r->headers_out.content_type) != NGX_OK) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_complex_value != NGX_OK"); return NGX_ERROR; }
    if (!r->headers_out.content_type.data) {
        ngx_http_core_loc_conf_t *core = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
        r->headers_out.content_type = core->default_type;
    }
    r->headers_out.content_type_len = r->headers_out.content_type.len;
    return ngx_http_next_header_filter(r);
}

static void handlebars_value_init_json_string_length(struct handlebars_context *ctx, struct handlebars_value *value, const char *json, size_t length) {
    enum json_tokener_error error;
    struct json_object *root;
    struct json_tokener *tok;
    if (!(tok = json_tokener_new())) handlebars_throw(ctx, HANDLEBARS_ERROR, "!json_tokener_new");
    do root = json_tokener_parse_ex(tok, json, length); while ((error = json_tokener_get_error(tok)) == json_tokener_continue);
    if (error != json_tokener_success) handlebars_throw(ctx, HANDLEBARS_ERROR, "!json_tokener_parse_ex and %s", json_tokener_error_desc(error));
    if (json_tokener_get_parse_end(tok) < length) handlebars_throw(ctx, HANDLEBARS_ERROR, "json_tokener_get_parse_end < %li", length);
    json_tokener_free(tok);
    handlebars_value_init_json_object(ctx, value, root);
    json_object_put(root);
}

static ngx_int_t ngx_http_handlebars_process(ngx_http_request_t *r, ngx_str_t *json, ngx_str_t *template) {
    jmp_buf jmp;
    struct handlebars_ast_node *ast;
    struct handlebars_compiler *compiler;
    struct handlebars_context *ctx;
    struct handlebars_module *module;
    struct handlebars_parser *parser;
    struct handlebars_program *program;
    struct handlebars_string *buffer;
    struct handlebars_string *tmpl;
    struct handlebars_value *input;
    struct handlebars_value *partials;
    struct handlebars_vm *vm;
    volatile ngx_int_t rc = NGX_ERROR;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_http_handlebars_location_t *location = ngx_http_get_module_loc_conf(r, ngx_http_handlebars_module);
    ctx = handlebars_context_ctor();
    if (handlebars_setjmp_ex(ctx, &jmp)) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, handlebars_error_message(ctx)); goto clean; }
    compiler = handlebars_compiler_ctor(ctx);
    handlebars_compiler_set_flags(compiler, location->compiler_flags);
    parser = handlebars_parser_ctor(ctx);
    tmpl = handlebars_string_ctor(HBSCTX(parser), (const char *)template->data, template->len);
    if (location->compiler_flags & handlebars_compiler_flag_compat) tmpl = handlebars_preprocess_delimiters(ctx, tmpl, NULL, NULL);
    ast = handlebars_parse_ex(parser, tmpl, location->compiler_flags);
    program = handlebars_compiler_compile_ex(compiler, ast);
    module = handlebars_program_serialize(ctx, program);
    input = handlebars_value_ctor(ctx);
    handlebars_value_init_json_string_length(ctx, input, (const char *)json->data, json->len);
//    if (location->convert_input) handlebars_value_convert(input);
    partials = handlebars_value_partial_loader_init(ctx, handlebars_string_ctor(ctx, ".", sizeof(".") - 1), handlebars_string_ctor(ctx, "", sizeof("") - 1), handlebars_value_ctor(ctx));
    vm = handlebars_vm_ctor(ctx);
    handlebars_vm_set_flags(vm, location->compiler_flags);
    handlebars_vm_set_partials(vm, partials);
    buffer = handlebars_vm_execute(vm, module, input);
    handlebars_vm_dtor(vm);
    handlebars_value_dtor(input);
    handlebars_value_dtor(partials);
    if (!buffer) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!buffer"); goto clean; }
    ngx_str_t output = {hbs_str_len(buffer), (u_char *)hbs_str_val(buffer)};
    ngx_chain_t *chain = ngx_alloc_chain_link(r->pool);
    if (!chain) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!ngx_alloc_chain_link"); goto clean; }
    chain->next = NULL;
    ngx_buf_t *buf = chain->buf = ngx_create_temp_buf(r->pool, output.len);
    if (!buf) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!ngx_create_temp_buf"); goto clean; }
    buf->memory = 1;
    buf->last = ngx_copy(buf->last, output.data, output.len);
    if (buf->last != buf->end) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "buf->last != buf->end"); goto clean; }
    if (r == r->main && !r->post_action) {
        buf->last_buf = 1;
    } else {
        buf->sync = 1;
        buf->last_in_chain = 1;
    }
    rc = ngx_http_next_body_filter(r, chain);
clean:
    handlebars_context_dtor(ctx);
    return rc;
}

static ngx_int_t ngx_http_handlebars_body_filter_internal(ngx_http_request_t *r, ngx_chain_t *in) {
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_http_handlebars_location_t *location = ngx_http_get_module_loc_conf(r, ngx_http_handlebars_module);
    ngx_str_t json;
    if (in) {
        json.len = 0;
        for (ngx_chain_t *cl = in; cl; cl = cl->next) {
            if (!ngx_buf_in_memory(cl->buf)) continue;
            json.len += cl->buf->last - cl->buf->pos;
        }
        if (!json.len) return ngx_http_next_body_filter(r, in);
        if (!(json.data = ngx_pnalloc(r->pool, json.len + 1))) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!ngx_pnalloc"); return NGX_ERROR; }
        u_char *p = json.data;
        size_t len;
        for (ngx_chain_t *cl = in; cl; cl = cl->next) {
            if (!ngx_buf_in_memory(cl->buf)) continue;
            if (!(len = cl->buf->last - cl->buf->pos)) continue;
            p = ngx_copy(p, cl->buf->pos, len);
        }
        *p = '\0';
    } else if (location->json) {
        if (ngx_http_complex_value(r, location->json, &json) != NGX_OK) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_complex_value != NGX_OK"); return NGX_ERROR; }
    } else { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!in && !json"); return NGX_ERROR; }
    ngx_str_t template;
    if (ngx_http_complex_value(r, location->template, &template) != NGX_OK) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_complex_value != NGX_OK"); return NGX_ERROR; }
    return ngx_http_handlebars_process(r, &json, &template);
}

#if (NGX_THREADS)
static void ngx_http_handlebars_thread_event_handler(ngx_event_t *ev) {
    ngx_http_request_t *r = ev->data;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_connection_t *c = r->connection;
    r->main->blocked--;
    r->aio = 0;
    if (r->done) c->write->handler(c->write); else {
        r->write_event_handler(r);
        ngx_http_run_posted_requests(c);
    }
}

static ngx_int_t ngx_http_handlebars_thread_handler(ngx_thread_task_t *task, ngx_file_t *file) {
    ngx_http_request_t *r = file->thread_ctx;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_http_core_loc_conf_t *core_loc_conf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    ngx_thread_pool_t *thread_pool = core_loc_conf->thread_pool;
    if (!thread_pool) {
        ngx_str_t name;
        if (ngx_http_complex_value(r, core_loc_conf->thread_pool_value, &name) != NGX_OK) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_complex_value != NGX_OK"); return NGX_ERROR; }
        if (!(thread_pool = ngx_thread_pool_get((ngx_cycle_t *)ngx_cycle, &name))) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "thread pool \"%V\" not found", &name); return NGX_ERROR; }
    }
    task->event.data = r;
    task->event.handler = ngx_http_handlebars_thread_event_handler;
    if (ngx_thread_task_post(thread_pool, task) != NGX_OK) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_thread_task_post != NGX_OK"); return NGX_ERROR; }
    r->main->blocked++;
    r->aio = 1;
    return NGX_OK;
}
#endif

static ngx_int_t ngx_http_handlebars_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
    ngx_http_handlebars_location_t *location = ngx_http_get_module_loc_conf(r, ngx_http_handlebars_module);
    ngx_http_handlebars_context_t *context = ngx_http_get_module_ctx(r, ngx_http_handlebars_module);
    if (!context) return ngx_http_next_body_filter(r, in);
    if (!location->json && !(in && context->content.len >= sizeof("application/json") - 1 && !ngx_strncasecmp(context->content.data, (u_char *)"application/json", sizeof("application/json") - 1))) return ngx_http_next_body_filter(r, in);
    if (!location->template) return ngx_http_next_body_filter(r, in);
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "%s", __func__);
    ngx_http_core_loc_conf_t *core_loc_conf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
#if (NGX_THREADS)
    if (core_loc_conf->aio != NGX_HTTP_AIO_THREADS)
#endif
    return ngx_http_handlebars_body_filter_internal(r, in);
#if (NGX_THREADS)
    ngx_output_chain_ctx_t *ctx = ngx_pcalloc(r->pool, sizeof(*ctx));
    if (!ctx) { ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "!ngx_pcalloc"); return NGX_ERROR; }
    ctx->pool = r->pool;
    ctx->output_filter = (ngx_output_chain_filter_pt)ngx_http_handlebars_body_filter_internal;
    ctx->filter_ctx = r;
    ctx->thread_handler = ngx_http_handlebars_thread_handler;
    ctx->aio = r->aio;
    return ngx_output_chain(ctx, in);
#endif
}

static ngx_int_t ngx_http_handlebars_postconfiguration(ngx_conf_t *cf) {
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_handlebars_header_filter;
    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_handlebars_body_filter;
    return NGX_OK;
}

static ngx_http_module_t ngx_http_handlebars_ctx = {
    .preconfiguration = NULL,
    .postconfiguration = ngx_http_handlebars_postconfiguration,
    .create_main_conf = NULL,
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
