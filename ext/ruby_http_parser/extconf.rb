require 'mkmf'
 
http_parser_dir = File.expand_path(File.dirname(__FILE__) + '/../http-parser')
$CFLAGS << " -I#{http_parser_dir} "

dir_config("ruby_http_parser")
create_makefile("ruby_http_parser")
