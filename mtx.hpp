#pragma once
#define _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
#include <mutex>
#include <memory>


static std::shared_ptr<std::mutex>  mtx;
