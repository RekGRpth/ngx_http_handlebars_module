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

=== TEST 1: No Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "Hello from {handlebarse}!\n";
        handlebars_content text/html;
        return 200 '{
      }';
    }
--- request
    GET /test
--- response_body eval
"Hello from {handlebarse}!\n"
=== TEST 2: Basic Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "Hello, {{subject}}!\n";
        handlebars_content text/html;
        return 200 '{
        "subject": "world"
      }';
    }
--- request
    GET /test
--- response_body eval
"Hello, world!\n"
=== TEST 3: HTML Escaping
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "These characters should be HTML escaped: {{forbidden}}\n";
        handlebars_content text/html;
        return 200 '{
        "forbidden": "& \\" < >"
      }';
    }
--- request
    GET /test
--- response_body eval
"These characters should be HTML escaped: &amp; &quot; &lt; &gt;\n"
=== TEST 4: Triple handlebarse
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "These characters should not be HTML escaped: {{{forbidden}}}\n";
        handlebars_content text/html;
        return 200 '{
        "forbidden": "& \\" < >"
      }';
    }
--- request
    GET /test
--- response_body eval
"These characters should not be HTML escaped: & \" < >\n"
=== TEST 5: Ampersand
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "These characters should not be HTML escaped: {{&forbidden}}\n";
        handlebars_content text/html;
        return 200 '{
        "forbidden": "& \\" < >"
      }';
    }
--- request
    GET /test
--- response_body eval
"These characters should not be HTML escaped: & \" < >\n"
=== TEST 6: Basic Integer Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{mph}} miles an hour!\"";
        handlebars_content text/html;
        return 200 '{
        "mph": 85
      }';
    }
--- request
    GET /test
--- response_body eval
"\"85 miles an hour!\""
=== TEST 7: Triple handlebarse Integer Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{{mph}}} miles an hour!\"";
        handlebars_content text/html;
        return 200 '{
        "mph": 85
      }';
    }
--- request
    GET /test
--- response_body eval
"\"85 miles an hour!\""
=== TEST 8: Ampersand Integer Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{&mph}} miles an hour!\"";
        handlebars_content text/html;
        return 200 '{
        "mph": 85
      }';
    }
--- request
    GET /test
--- response_body eval
"\"85 miles an hour!\""
=== TEST 9: Basic Decimal Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{power}} jiggawatts!\"";
        handlebars_content text/html;
        return 200 '{
        "power": 1.21
      }';
    }
--- request
    GET /test
--- response_body eval
"\"1.21 jiggawatts!\""
=== TEST 10: Triple handlebarse Decimal Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{{power}}} jiggawatts!\"";
        handlebars_content text/html;
        return 200 '{
        "power": 1.21
      }';
    }
--- request
    GET /test
--- response_body eval
"\"1.21 jiggawatts!\""
=== TEST 11: Ampersand Decimal Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{&power}} jiggawatts!\"";
        handlebars_content text/html;
        return 200 '{
        "power": 1.21
      }';
    }
--- request
    GET /test
--- response_body eval
"\"1.21 jiggawatts!\""
=== TEST 12: Basic Null Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "I ({{cannot}}) be seen!";
        handlebars_content text/html;
        return 200 '{
        "cannot": null
      }';
    }
--- request
    GET /test
--- response_body eval
"I () be seen!"
=== TEST 13: Triple handlebarse Null Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "I ({{{cannot}}}) be seen!";
        handlebars_content text/html;
        return 200 '{
        "cannot": null
      }';
    }
--- request
    GET /test
--- response_body eval
"I () be seen!"
=== TEST 14: Ampersand Null Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "I ({{&cannot}}) be seen!";
        handlebars_content text/html;
        return 200 '{
        "cannot": null
      }';
    }
--- request
    GET /test
--- response_body eval
"I () be seen!"
=== TEST 15: Basic Context Miss Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "I ({{cannot}}) be seen!";
        handlebars_content text/html;
        return 200 '{
      }';
    }
--- request
    GET /test
--- response_body eval
"I () be seen!"
=== TEST 16: Triple handlebarse Context Miss Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "I ({{{cannot}}}) be seen!";
        handlebars_content text/html;
        return 200 '{
      }';
    }
--- request
    GET /test
--- response_body eval
"I () be seen!"
=== TEST 17: Ampersand Context Miss Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "I ({{&cannot}}) be seen!";
        handlebars_content text/html;
        return 200 '{
      }';
    }
--- request
    GET /test
--- response_body eval
"I () be seen!"
=== TEST 18: Dotted Names - Basic Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{person.name}}\" == \"{{#person}}{{name}}{{/person}}\"";
        handlebars_content text/html;
        return 200 '{
        "person": {
          "name": "Joe"
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"Joe\" == \"Joe\""
=== TEST 19: Dotted Names - Triple handlebarse Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{{person.name}}}\" == \"{{#person}}{{{name}}}{{/person}}\"";
        handlebars_content text/html;
        return 200 '{
        "person": {
          "name": "Joe"
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"Joe\" == \"Joe\""
=== TEST 20: Dotted Names - Ampersand Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{&person.name}}\" == \"{{#person}}{{&name}}{{/person}}\"";
        handlebars_content text/html;
        return 200 '{
        "person": {
          "name": "Joe"
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"Joe\" == \"Joe\""
=== TEST 21: Dotted Names - Arbitrary Depth
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{a.b.c.d.e.name}}\" == \"Phil\"";
        handlebars_content text/html;
        return 200 '{
        "a": {
          "b": {
            "c": {
              "d": {
                "e": {
                  "name": "Phil"
                }
              }
            }
          }
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"Phil\" == \"Phil\""
=== TEST 22: Dotted Names - Broken Chains
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{a.b.c}}\" == \"\"";
        handlebars_content text/html;
        return 200 '{
        "a": {
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"\" == \"\""
=== TEST 23: Dotted Names - Broken Chain Resolution
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{a.b.c.name}}\" == \"\"";
        handlebars_content text/html;
        return 200 '{
        "a": {
          "b": {
          }
        },
        "c": {
          "name": "Jim"
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"\" == \"\""
=== TEST 24: Dotted Names - Initial Resolution
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{#a}}{{b.c.d.e.name}}{{/a}}\" == \"Phil\"";
        handlebars_content text/html;
        return 200 '{
        "a": {
          "b": {
            "c": {
              "d": {
                "e": {
                  "name": "Phil"
                }
              }
            }
          }
        },
        "b": {
          "c": {
            "d": {
              "e": {
                "name": "Wrong"
              }
            }
          }
        }
      }';
    }
--- request
    GET /test
--- response_body eval
"\"Phil\" == \"Phil\""
=== TEST 25: Dotted Names - Context Precedence
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "{{#a}}{{b.c}}{{/a}} ";
        handlebars_content text/html;
        return 200 '{
        "a": {
          "b": {
          }
        },
        "b": {
          "c": "ERROR"
        }
      }';
    }
--- request
    GET /test
--- response_body eval
" "
=== TEST 26: Implicit Iterators - Basic Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "Hello, {{.}}!\n";
        handlebars_content text/html;
        return 200 '"world"';
    }
--- request
    GET /test
--- response_body eval
"Hello, world!\n"
=== TEST 27: Implicit Iterators - HTML Escaping
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "These characters should be HTML escaped: {{.}}\n";
        handlebars_content text/html;
        return 200 '"& \\" < >"';
    }
--- request
    GET /test
--- response_body eval
"These characters should be HTML escaped: &amp; &quot; &lt; &gt;\n"
=== TEST 28: Implicit Iterators - Triple handlebarse
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "These characters should not be HTML escaped: {{{.}}}\n";
        handlebars_content text/html;
        return 200 '"& \\" < >"';
    }
--- request
    GET /test
--- response_body eval
"These characters should not be HTML escaped: & \" < >\n"
=== TEST 29: Implicit Iterators - Ampersand
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "These characters should not be HTML escaped: {{&.}}\n";
        handlebars_content text/html;
        return 200 '"& \\" < >"';
    }
--- request
    GET /test
--- response_body eval
"These characters should not be HTML escaped: & \" < >\n"
=== TEST 30: Implicit Iterators - Basic Integer Interpolation
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "\"{{.}} miles an hour!\"";
        handlebars_content text/html;
        return 200 '"85"';
    }
--- request
    GET /test
--- response_body eval
"\"85 miles an hour!\""
=== TEST 31: Interpolation - Surrounding Whitespace
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "| {{string}} |";
        handlebars_content text/html;
        return 200 '{
        "string": "---"
      }';
    }
--- request
    GET /test
--- response_body eval
"| --- |"
=== TEST 32: Triple handlebarse - Surrounding Whitespace
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "| {{{string}}} |";
        handlebars_content text/html;
        return 200 '{
        "string": "---"
      }';
    }
--- request
    GET /test
--- response_body eval
"| --- |"
=== TEST 33: Ampersand - Surrounding Whitespace
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "| {{&string}} |";
        handlebars_content text/html;
        return 200 '{
        "string": "---"
      }';
    }
--- request
    GET /test
--- response_body eval
"| --- |"
=== TEST 34: Interpolation - Standalone
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "  {{string}}\n";
        handlebars_content text/html;
        return 200 '{
        "string": "---"
      }';
    }
--- request
    GET /test
--- response_body eval
"  ---\n"
=== TEST 35: Triple handlebarse - Standalone
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "  {{{string}}}\n";
        handlebars_content text/html;
        return 200 '{
        "string": "---"
      }';
    }
--- request
    GET /test
--- response_body eval
"  ---\n"
=== TEST 36: Ampersand - Standalone
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "  {{&string}}\n";
        handlebars_content text/html;
        return 200 '{
        "string": "---"
      }';
    }
--- request
    GET /test
--- response_body eval
"  ---\n"
=== TEST 37: Interpolation With Padding
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "|{{ string }}|";
        handlebars_content text/html;
        return 200 '{
        "string": "---"
      }';
    }
--- request
    GET /test
--- response_body eval
"|---|"
=== TEST 38: Triple handlebarse With Padding
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "|{{{ string }}}|";
        handlebars_content text/html;
        return 200 '{
        "string": "---"
      }';
    }
--- request
    GET /test
--- response_body eval
"|---|"
=== TEST 39: Ampersand With Padding
--- main_config eval: $::main_config
--- config
    location /test {
        default_type application/json;
        handlebars_template "|{{& string }}|";
        handlebars_content text/html;
        return 200 '{
        "string": "---"
      }';
    }
--- request
    GET /test
--- response_body eval
"|---|"
