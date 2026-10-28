#pragma once
#include "resource.h"
#include "vendor/json.hpp"
inline const nlohmann::json& menuLimits(){
    static const auto limits=[](){
        const auto module=GetModuleHandleW(nullptr);const auto resource=FindResourceW(module,MAKEINTRESOURCEW(IDR_LIMITS),RT_RCDATA);
        if(!resource)throw std::runtime_error("Missing menu limits resource");
        const auto loaded=LoadResource(module,resource);const auto* bytes=static_cast<const char*>(LockResource(loaded));
        return nlohmann::json::parse(bytes,bytes+SizeofResource(module,resource));
    }();
    return limits;
}
