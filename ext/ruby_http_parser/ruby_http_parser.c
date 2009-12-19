#include "ruby.h"
#include "ext_help.h"
#include "http_parser.c"

#define HEADER_PREFIX         "HTTP_"
#define DEF_CONST(N, val)     N = rb_obj_freeze(rb_str_new2(val)); rb_global_variable(&N)
#define GET_WRAPPER(N, from)  ParserWrapper *N = (ParserWrapper *)(from)->data;

#define HASH_CAT(h, k, ptr, len) \
  do { \
    VALUE __v = rb_hash_aref(h, k); \
    if (__v != Qnil) { \
      rb_str_cat(__v, ptr, len); \
    } else { \
      rb_hash_aset(h, k, rb_str_new(ptr, len)); \
    } \
  } while(0)


// Stolen from Ebb
static char upcase[] =
  "\0______________________________"
  "_________________0123456789_____"
  "__ABCDEFGHIJKLMNOPQRSTUVWXYZ____"
  "__ABCDEFGHIJKLMNOPQRSTUVWXYZ____"
  "________________________________"
  "________________________________"
  "________________________________"
  "________________________________";

typedef struct ParserWrapper {
  http_parser parser;
  VALUE env;
  VALUE on_message_complete;
  VALUE on_headers_complete;
  VALUE on_body;
  VALUE last_field_name;
  const char *last_field_name_at;
  size_t last_field_name_length;
} ParserWrapper;

static VALUE eParserError;
static VALUE sCall;
static VALUE sPathInfo;
static VALUE sQueryString;
static VALUE sURL;
static VALUE sFragment;

/** Callbacks **/

int on_message_begin(http_parser *parser) {
  GET_WRAPPER(wrapper, parser);
  // Init env Rack hash
  if (wrapper->env != Qnil)
    rb_gc_unregister_address(&wrapper->env);
  wrapper->env = rb_hash_new();
  rb_gc_register_address(&wrapper->env);
  
  return 0;
}

int on_path(http_parser *parser, const char *at, size_t length) {
  GET_WRAPPER(wrapper, parser);
  HASH_CAT(wrapper->env, sPathInfo, at, length);
  return 0;
}

int on_query_string(http_parser *parser, const char *at, size_t length) {
  GET_WRAPPER(wrapper, parser);
  HASH_CAT(wrapper->env, sQueryString, at, length);
  return 0;
}

int on_url(http_parser *parser, const char *at, size_t length) {
  GET_WRAPPER(wrapper, parser);
  HASH_CAT(wrapper->env, sURL, at, length);
  return 0;
}

int on_fragment(http_parser *parser, const char *at, size_t length) {
  GET_WRAPPER(wrapper, parser);
  HASH_CAT(wrapper->env, sFragment, at, length);
  return 0;
}

int on_header_field(http_parser *parser, const char *at, size_t length) {
  GET_WRAPPER(wrapper, parser);
  
  wrapper->last_field_name = Qnil;
  
  if (wrapper->last_field_name_at == NULL) {
    wrapper->last_field_name_at = at;
    wrapper->last_field_name_length = length;
  } else {
    wrapper->last_field_name_length += length;
  }
  
  return 0;
}

int on_header_value(http_parser *parser, const char *at, size_t length) {
  GET_WRAPPER(wrapper, parser);
  
  VALUE name = Qnil;
  
  if (wrapper->last_field_name == Qnil) {
    wrapper->last_field_name = rb_str_new(HEADER_PREFIX, sizeof(HEADER_PREFIX) - 1 + wrapper->last_field_name_length);
    
    // normalize header name
    size_t name_length = wrapper->last_field_name_length;
    const char *name_at = wrapper->last_field_name_at;
    char *name_ptr = RSTRING_PTR(wrapper->last_field_name) + sizeof(HEADER_PREFIX) - 1;
    int i;
    for(i = 0; i < name_length; i++) {
      char *ch = name_ptr + i;
      *ch = upcase[(int)name_at[i]];
    }
    
    wrapper->last_field_name_at = NULL;
    wrapper->last_field_name_length = 0;
  }
  
  HASH_CAT(wrapper->env, wrapper->last_field_name, at, length);
  
  return 0;
}

int on_headers_complete(http_parser *parser) {
  GET_WRAPPER(wrapper, parser);
  
  if (wrapper->on_headers_complete != Qnil) {
    rb_funcall(wrapper->on_headers_complete, sCall, 1, wrapper->env);
  }
  
  return 0;
}

int on_body(http_parser *parser, const char *at, size_t length) {
  GET_WRAPPER(wrapper, parser);
  
  if (wrapper->on_body != Qnil) {
    rb_funcall(wrapper->on_body, sCall, 1, rb_str_new(at, length));
  }
  
  return 0;
}

int on_message_complete(http_parser *parser) {
  GET_WRAPPER(wrapper, parser);
  
  if (wrapper->on_message_complete != Qnil) {
    rb_funcall(wrapper->on_message_complete, sCall, 1, wrapper->env);
  }
  
  return 0;
}


/** Usual ruby C stuff **/

void Parser_free(void *data) {
  if(data) {
    ParserWrapper *wrapper = (ParserWrapper *) data;
    if (wrapper->env != Qnil)
      rb_gc_unregister_address(&wrapper->env);
    free(data);
  }
}

VALUE Parser_alloc(VALUE klass) {
  ParserWrapper *wrapper = ALLOC_N(ParserWrapper, 1);
  http_parser_init(&wrapper->parser);
  
  wrapper->env = Qnil;
  wrapper->on_message_complete = Qnil;
  wrapper->on_headers_complete = Qnil;
  wrapper->on_body = Qnil;
  
  wrapper->last_field_name = Qnil;
  wrapper->last_field_name_at = NULL;
  wrapper->last_field_name_length = 0;
  
  // Init callbacks
  wrapper->parser.on_message_begin = on_message_begin;
  wrapper->parser.on_path = on_path;
  wrapper->parser.on_query_string = on_query_string;
  wrapper->parser.on_url = on_url;
  wrapper->parser.on_fragment = on_fragment;
  wrapper->parser.on_header_field = on_header_field;
  wrapper->parser.on_header_value = on_header_value;
  wrapper->parser.on_headers_complete = on_headers_complete;
  wrapper->parser.on_body = on_body;
  wrapper->parser.on_message_complete = on_message_complete;
  
  wrapper->parser.data = wrapper;

  return Data_Wrap_Struct(klass, NULL, Parser_free, wrapper);
}

VALUE Parser_parse_requests(VALUE self, VALUE data) {
  ParserWrapper *wrapper = NULL;
  char *ptr = RSTRING_PTR(data);
  long len = RSTRING_LEN(data);
  
  DATA_GET(self, ParserWrapper, wrapper);
  
  size_t nparsed = http_parse_requests(&wrapper->parser, ptr, len);
  
  if (nparsed != len) {
    rb_raise(eParserError, "Invalid request");
  }
}

VALUE Parser_parse_responses(VALUE self, VALUE data) {
  ParserWrapper *wrapper = NULL;
  char *ptr = RSTRING_PTR(data);
  long len = RSTRING_LEN(data);
  
  DATA_GET(self, ParserWrapper, wrapper);
  
  size_t nparsed = http_parse_responses(&wrapper->parser, ptr, len);
  
  if (nparsed != len) {
    rb_raise(eParserError, "Invalid response");
  }
}

VALUE Parser_set_on_headers_complete(VALUE self, VALUE callback) {
  ParserWrapper *wrapper = NULL;
  DATA_GET(self, ParserWrapper, wrapper);
  
  wrapper->on_headers_complete = callback;
  return callback;
}

VALUE Parser_set_on_message_complete(VALUE self, VALUE callback) {
  ParserWrapper *wrapper = NULL;
  DATA_GET(self, ParserWrapper, wrapper);
  
  wrapper->on_message_complete = callback;
  return callback;
}

VALUE Parser_set_on_body(VALUE self, VALUE callback) {
  ParserWrapper *wrapper = NULL;
  DATA_GET(self, ParserWrapper, wrapper);
  
  wrapper->on_body = callback;
  return callback;
}


void Init_ruby_http_parser() {
  VALUE mHTTP = rb_define_module_under(rb_define_module("Net"), "HTTP");
  VALUE cParser = rb_define_class_under(mHTTP, "Parser", rb_cObject);
  VALUE cRequestParser = rb_define_class_under(mHTTP, "RequestParser", cParser);
  VALUE cResponseParser = rb_define_class_under(mHTTP, "ResponseParser", cParser);
  
  eParserError = rb_define_class_under(mHTTP, "ParseError", rb_eIOError);
  sCall = rb_intern("call");
  
  // String constants
  DEF_CONST(sPathInfo, "PATH_INFO");
  DEF_CONST(sQueryString, "QUERY_STRING");
  DEF_CONST(sURL, "REQUEST_URI");
  DEF_CONST(sFragment, "FRAGMENT");
  
  rb_define_alloc_func(cParser, Parser_alloc);
  rb_define_method(cParser, "on_message_complete=", Parser_set_on_message_complete, 1);
  rb_define_method(cParser, "on_headers_complete=", Parser_set_on_headers_complete, 1);
  rb_define_method(cParser, "on_body=", Parser_set_on_body, 1);

  rb_define_method(cRequestParser, "<<", Parser_parse_requests, 1);
  rb_define_method(cResponseParser, "<<", Parser_parse_responses, 1);
}