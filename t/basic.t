# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

#$Test::Nginx::LWP::LogLevel = 'debug';

our $main_config = <<'_EOC_';
    load_module /etc/nginx/modules/ngx_http_handlebars_module.so;
_EOC_

no_shuffle();
run_tests();

__DATA__

=== TEST 1: most basic
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "{{foo}}";
        handlebars_content text/html;
        return 200 '{
			"foo": "foo"
		}';
    }
--- request
    GET /test
--- response_body eval
"foo"
=== TEST 2: escaping
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\\{{foo}}";
        handlebars_content text/html;
        return 200 '{
			"foo": "food"
		}';
    }
--- request
    GET /test
--- response_body eval
"{{foo}}"
=== TEST 3: escaping 01
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "content \\{{foo}}";
        handlebars_content text/html;
        return 200 '{
			"foo": "food"
		}';
    }
--- request
    GET /test
--- response_body eval
"content {{foo}}"
=== TEST 4: escaping 02
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\\\\{{foo}}";
        handlebars_content text/html;
        return 200 '{
			"foo": "food"
		}';
    }
--- request
    GET /test
--- response_body eval
"\\food"
=== TEST 5: escaping 03
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "content \\\\{{foo}}";
        handlebars_content text/html;
        return 200 '{
			"foo": "food"
		}';
    }
--- request
    GET /test
--- response_body eval
"content \\food"
=== TEST 6: escaping 04
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\\\\ {{foo}}";
        handlebars_content text/html;
        return 200 '{
			"foo": "food"
		}';
    }
--- request
    GET /test
--- response_body eval
"\\\\ food"
=== TEST 7: compiling with a basic context
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "Goodbye\n{{cruel}}\n{{world}}!";
        handlebars_content text/html;
        return 200 '{
			"cruel": "cruel",
			"world": "world"
		}';
    }
--- request
    GET /test
--- response_body eval
"Goodbye\ncruel\nworld!"
=== TEST 8: compiling with a string context
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "{{.}}{{length}}";
        handlebars_content text/html;
        return 200 '"bye"';
    }
--- request
    GET /test
--- response_body eval
"bye"
=== TEST 9: compiling with an undefined context
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "Goodbye\n{{cruel}}\n{{world.bar}}!";
        handlebars_content text/html;
        return 200 'null';
    }
--- request
    GET /test
--- response_body eval
"Goodbye\n\n!"
=== TEST 10: compiling with an undefined context 01
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "{{#unless foo}}Goodbye{{../test}}{{test2}}{{/unless}}";
        handlebars_content text/html;
        return 200 'null';
    }
--- request
    GET /test
--- response_body eval
"Goodbye"
=== TEST 11: comments
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "{{! Goodbye}}Goodbye\n{{cruel}}\n{{world}}!";
        handlebars_content text/html;
        return 200 '{
			"cruel": "cruel",
			"world": "world"
		}';
    }
--- request
    GET /test
--- response_body eval
"Goodbye\ncruel\nworld!"
=== TEST 12: comments 01
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "    {{~! comment ~}}      blah";
        handlebars_content text/html;
        return 200 '{
		}';
    }
--- request
    GET /test
--- response_body eval
"blah"
=== TEST 13: comments 02
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "    {{~!-- long-comment --~}}      blah";
        handlebars_content text/html;
        return 200 '{
		}';
    }
--- request
    GET /test
--- response_body eval
"blah"
