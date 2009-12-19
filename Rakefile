require "rake/extensiontask"
require "spec/rake/spectask"

Rake::ExtensionTask.new("ruby_http_parser")

task :default => :spec

Spec::Rake::SpecTask.new do |t|
  t.spec_opts = %w(-fs -c)
  t.spec_files = FileList["spec/**/*_spec.rb"]
end
