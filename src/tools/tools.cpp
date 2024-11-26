#include "tools.hpp"

std::string tools::getext(std::string f)
{
    size_t p = f.find_last_of('.');
    if (p != std::string::npos)return f.substr(p + 1);
    return std::string();
}
