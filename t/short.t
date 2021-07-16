# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

#$Test::Nginx::LWP::LogLevel = 'debug';

run_tests();

__DATA__

=== TEST 1: content json
--- main_config
    load_module /etc/nginx/modules/ngx_http_handlebars_module.so;
--- config
    location /handlebars {
        default_type application/json;
        handlebars_template "{{a}}";
        handlebars_content text/html;
        return 200 '{"a":"b"}';
    }
--- request
    GET /handlebars
--- response_body chop
b
=== TEST 2: content json
--- main_config
    load_module /etc/nginx/modules/ngx_http_handlebars_module.so;
--- config
    location /handlebars {
        default_type application/json;
        handlebars_template '<ul>{{#people}}<li>{{firstName}} {{lastName}}</li>{{/people}}</ul>';
        handlebars_content text/html;
        return 200 '{"people":[{"firstName":"Yehuda","lastName":"Katz"},{"firstName":"Carl","lastName":"Lerche"},{"firstName":"Alan","lastName":"Johnson"}]}';
    }
--- request
    GET /handlebars
--- response_body chop
<ul><li>Yehuda Katz</li><li>Carl Lerche</li><li>Alan Johnson</li></ul>
=== TEST 3: variable json
--- main_config
    load_module /etc/nginx/modules/ngx_http_handlebars_module.so;
--- config
    location /handlebars {
        handlebars_template "{{a}}";
        handlebars_content text/html;
        handlebars_json '{"a":"b"}';
    }
--- request
    GET /handlebars
--- response_body chop
b
=== TEST 4: variable json
--- main_config
    load_module /etc/nginx/modules/ngx_http_handlebars_module.so;
--- config
    location /handlebars {
        handlebars_template '<ul>{{#people}}<li>{{firstName}} {{lastName}}</li>{{/people}}</ul>';
        handlebars_content text/html;
        handlebars_json '{"people":[{"firstName":"Yehuda","lastName":"Katz"},{"firstName":"Carl","lastName":"Lerche"},{"firstName":"Alan","lastName":"Johnson"}]}';
    }
--- request
    GET /handlebars
--- response_body chop
<ul><li>Yehuda Katz</li><li>Carl Lerche</li><li>Alan Johnson</li></ul>
=== TEST 5: eval json
--- main_config
    load_module /etc/nginx/modules/ngx_http_eval_module.so;
    load_module /etc/nginx/modules/ngx_http_handlebars_module.so;
--- config
    location /handlebars {
        eval $json {
            return 200 '{"a":"b"}';
        }
        handlebars_template "{{a}}";
        handlebars_content text/html;
        handlebars_json $json;
    }
--- request
    GET /handlebars
--- response_body chop
b
=== TEST 6: eval json
--- main_config
    load_module /etc/nginx/modules/ngx_http_eval_module.so;
    load_module /etc/nginx/modules/ngx_http_handlebars_module.so;
--- config
    location /handlebars {
        eval $json {
            return 200 '{"people":[{"firstName":"Yehuda","lastName":"Katz"},{"firstName":"Carl","lastName":"Lerche"},{"firstName":"Alan","lastName":"Johnson"}]}';
        }
        handlebars_template '<ul>{{#people}}<li>{{firstName}} {{lastName}}</li>{{/people}}</ul>';
        handlebars_content text/html;
        handlebars_json $json;
    }
--- request
    GET /handlebars
--- response_body chop
<ul><li>Yehuda Katz</li><li>Carl Lerche</li><li>Alan Johnson</li></ul>
=== TEST 7: eval template
--- main_config
    load_module /etc/nginx/modules/ngx_http_eval_module.so;
    load_module /etc/nginx/modules/ngx_http_handlebars_module.so;
--- config
    location /handlebars {
        eval $template {
            return 200 '{{a}}';
        }
        handlebars_template $template;
        handlebars_content text/html;
        handlebars_json '{"a":"b"}';
    }
--- request
    GET /handlebars
--- response_body chop
b
=== TEST 8: eval template
--- main_config
    load_module /etc/nginx/modules/ngx_http_eval_module.so;
    load_module /etc/nginx/modules/ngx_http_handlebars_module.so;
--- config
    location /handlebars {
        eval $template {
            return 200 '<ul>{{#people}}<li>{{firstName}} {{lastName}}</li>{{/people}}</ul>';
        }
        handlebars_template $template;
        handlebars_content text/html;
        handlebars_json '{"people":[{"firstName":"Yehuda","lastName":"Katz"},{"firstName":"Carl","lastName":"Lerche"},{"firstName":"Alan","lastName":"Johnson"}]}';
    }
--- request
    GET /handlebars
--- response_body chop
<ul><li>Yehuda Katz</li><li>Carl Lerche</li><li>Alan Johnson</li></ul>
