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

=== TEST 1: Falsey
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{^boolean}}This should be rendered.{{/boolean}}\"";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
"\"This should be rendered.\""
=== TEST 2: Truthy
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{^boolean}}This should not be rendered.{{/boolean}}\"";
        handlebars_content text/html;
        return 200 '{
        "boolean": true
      }';
    }
--- request
    GET /test
--- response_body eval
"\"\""
=== TEST 3: Null is falsey
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{^null}}This should be rendered.{{/null}}\"";
        handlebars_content text/html;
        return 200 '{
        "null": null
      }';
    }
--- request
    GET /test
--- response_body eval
"\"This should be rendered.\""
=== TEST 4: Context
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{^context}}Hi {{name}}.{{/context}}\"";
        handlebars_content text/html;
        return 200 '{
        "context": {
          "name": "Joe"
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"\""
=== TEST 5: List
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{^list}}{{n}}{{/list}}\"";
        handlebars_content text/html;
        return 200 '{
        "list": [
          {
            "n": 1
          },
          {
            "n": 2
          },
          {
            "n": 3
          }
        ]
      }';
    }
--- request
    GET /test
--- response_body eval
"\"\""
=== TEST 6: Empty List
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{^list}}Yay lists!{{/list}}\"";
        handlebars_content text/html;
        return 200 '{
        "list": [
        ]
      }';
    }
--- request
    GET /test
--- response_body eval
"\"Yay lists!\""
=== TEST 7: Doubled
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "{{^bool}}\n* first\n{{/bool}}\n* {{two}}\n{{^bool}}\n* third\n{{/bool}}\n";
        handlebars_content text/html;
        return 200 '{
        "bool": false,
        "two": "second"
      }';
    }
--- request
    GET /test
--- response_body eval
"* first\n* second\n* third\n"
=== TEST 8: Nested (Falsey)
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "| A {{^bool}}B {{^bool}}C{{/bool}} D{{/bool}} E |";
        handlebars_content text/html;
        return 200 '{
        "bool": false
      }';
    }
--- request
    GET /test
--- response_body eval
"| A B C D E |"
=== TEST 9: Nested (Truthy)
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "| A {{^bool}}B {{^bool}}C{{/bool}} D{{/bool}} E |";
        handlebars_content text/html;
        return 200 '{
        "bool": true
      }';
    }
--- request
    GET /test
--- response_body eval
"| A  E |"
=== TEST 10: Context Misses
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "[{{^missing}}Cannot find key 'missing'!{{/missing}}]";
        handlebars_content text/html;
        return 200 '{
      }';
    }
--- request
    GET /test
--- response_body eval
"[Cannot find key 'missing'!]"
=== TEST 11: Dotted Names - Truthy
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{^a.b.c}}Not Here{{/a.b.c}}\" == \"\"";
        handlebars_content text/html;
        return 200 '{
        "a": {
          "b": {
            "c": true
          }
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"\" == \"\""
=== TEST 12: Dotted Names - Falsey
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{^a.b.c}}Not Here{{/a.b.c}}\" == \"Not Here\"";
        handlebars_content text/html;
        return 200 '{
        "a": {
          "b": {
            "c": false
          }
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"Not Here\" == \"Not Here\""
=== TEST 13: Dotted Names - Broken Chains
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{^a.b.c}}Not Here{{/a.b.c}}\" == \"Not Here\"";
        handlebars_content text/html;
        return 200 '{
        "a": {
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"Not Here\" == \"Not Here\""
=== TEST 14: Surrounding Whitespace
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template " | {{^boolean}}\t|\t{{/boolean}} | \n";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
" | \t|\t | \n"
=== TEST 15: Internal Whitespace
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template " | {{^boolean}} {{! Important Whitespace }}\n {{/boolean}} | \n";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
" |  \n  | \n"
=== TEST 16: Indented Inline Sections
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template " {{^boolean}}NO{{/boolean}}\n {{^boolean}}WAY{{/boolean}}\n";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
" NO\n WAY\n"
=== TEST 17: Standalone Lines
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "| This Is\n{{^boolean}}\n|\n{{/boolean}}\n| A Line\n";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
"| This Is\n|\n| A Line\n"
=== TEST 18: Standalone Indented Lines
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "| This Is\n  {{^boolean}}\n|\n  {{/boolean}}\n| A Line\n";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
"| This Is\n|\n| A Line\n"
=== TEST 19: Standalone Line Endings
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "|\r\n{{^boolean}}\r\n{{/boolean}}\r\n|";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
"|\r\n|"
=== TEST 20: Standalone Without Previous Line
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "  {{^boolean}}\n^{{/boolean}}\n/";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
"^\n/"
=== TEST 21: Standalone Without Newline
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "^{{^boolean}}\n/\n  {{/boolean}}";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
"^\n/\n"
=== TEST 22: Padding
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "|{{^ boolean }}={{/ boolean }}|";
        handlebars_content text/html;
        return 200 '{
        "boolean": false
      }';
    }
--- request
    GET /test
--- response_body eval
"|=|"
