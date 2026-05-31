#include<iostream>
#include<string>
#include<unordered_map>
#include<sstream>
#include "request.h"


HttpRequest parseRequest(const std::string &raw){
  HttpRequest req;

  std::istringstream stream(raw);
  
  //Request Line
  std::string requestLine;
  std::getline(stream , requestLine);

  if(!requestLine.empty() && requestLine.back() == '\r'){
    requestLine.pop_back();
  }
  std::istringstream firstLine(requestLine);

  firstLine >> req.method >>req.path >> req.version;

  // headers
  std::string line;


  while(std::getline(stream , line)){
    if(!line.empty()&& line.back() == '\r'){
      line.pop_back();
    }

    if(line.empty()) break;

    size_t pos = line.find(':');

    if(pos == std::string::npos) continue;

    std::string key = line.substr(0 , pos);
    
    std::string value = line.substr(pos+1);

    if(!value.empty() && value.front() == ' '){
      value.erase(0 , 1);
    }

    req.headers[key] = value;
  
  }
  return req;
}
/*
int main(){
  std::string raw = "GET /hello HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "User-Agent: curl\r\n"
                    "\r\n";
  HttpRequest req= parseRequest(raw);

  std::cout << "Method: " << req.method << '\n';

  std::cout << "Path: " << req.path << '\n';

  std::cout << "Version: " << req.version << '\n';

  for (auto & [k , v] :req.headers){
    std::cout << k << " => " << v << '\n';
  }

} */
