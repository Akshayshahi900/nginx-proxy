#pragma once
#include<unordered_map>
#include<string.h>



struct HttpRequest{
  std::string method;
  std::string path;
  std::string version;

  std::unordered_map<std::string , std::string>headers;

};

