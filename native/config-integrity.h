#pragma once
void validateEntries(const json& data){
    if(data.value("version",0)!=1||!data.at("groups").is_array()||!data.at("apps").is_array())throw std::runtime_error("Unsupported config");
    for(const auto* key:{"launcher","radial","webStyle","rgb","appearance","layout"})if(data.contains(key)&&!data.at(key).is_object())throw std::runtime_error("Invalid config section");
    std::set<std::string> ids;
    const auto asciiAlphaNum=[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');};
    for(const auto* collection:{"groups","apps"})for(const auto& item:data.at(collection)){
        if(item.contains("noIcon")&&!item["noIcon"].is_boolean())throw std::runtime_error("Invalid label option");
        const auto id=item.at("id").get<std::string>();
        if(id.empty()||id.size()>menuLimits().at("maxId").get<size_t>()||!asciiAlphaNum(id.front())||!std::all_of(id.begin(),id.end(),[&](unsigned char c){return asciiAlphaNum(c)||c=='-'||c=='_';}))throw std::runtime_error("Invalid ID");
        if(!ids.insert(id).second)throw std::runtime_error("Duplicate ID");
        const auto name=wide(item.at("name").get<std::string>());
        size_t count=0;for(size_t i=0;i<name.size();++i,++count)if(name[i]>=0xd800&&name[i]<=0xdbff&&i+1<name.size())++i;
        if(name.find_first_not_of(L" \t\r\n\f\v")==std::wstring::npos||name.find(L'\0')!=std::wstring::npos||count>menuLimits().at("maxName").get<size_t>())throw std::runtime_error("Invalid name");
        if(!item.at("position").is_number_integer())throw std::runtime_error("Position must be an integer");
        const bool app=std::string(collection)=="apps";const auto& parent=item.at(app?"groupId":"parentId");
        if(!parent.is_string()&&(app||!parent.is_null()))throw std::runtime_error("Invalid parent");
        if(app){const auto path=wide(item.at("path").get<std::string>());if(path.find_first_not_of(L" \t\r\n\f\v")==std::wstring::npos||path.find(L'\0')!=std::wstring::npos||path.size()>menuLimits().at("maxPath").get<size_t>())throw std::runtime_error("Invalid application path");}
    }
}
