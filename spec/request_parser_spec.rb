require File.dirname(__FILE__) + "/spec_helper"

describe Net::HTTP::RequestParser do
  before do
    @parser = Net::HTTP::RequestParser.new
  end
  
  it "should parse GET" do
    env = nil
    body = ""
    
    @parser.on_headers_complete = proc { |e| env = e }
    @parser.on_body = proc { |chunk| body << chunk }
    
    @parser << "GET /test?ok=1 HTTP/1.1\r\n" +
               "User-Agent: curl/7.18.0\r\n" +
               "Host: 0.0.0.0:5000\r\n" +
               "Accept: */*\r\n" +
               "Content-Length: 5\r\n" +
               "\r\n" +
               "World"
    
    env["PATH_INFO"].should == "/test"
    env["HTTP_HOST"].should == "0.0.0.0:5000"
    body.should == "World"
  end
end