Gem::Specification.new do |s|
  s.name = "ruby_http_parser"
  s.version = "0.0.1"
  s.summary = "Ruby bindings to http://github.com/ry/http-parser"
  
  s.author = "Marc-Andre Cournoyer"
  s.email = "macournoyer@gmail.com"
  s.files = Dir["**/*"]
  s.homepage = "http://github.com/macournoyer/ruby_http_parser"
  
  s.require_paths = ["lib"]
  s.extensions    = ["ext/ruby_http_parser/extconf.rb"]
end