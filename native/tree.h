#pragma once
#include <cmath>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include "vendor/json.hpp"

namespace tree {
using Json=nlohmann::json;
constexpr double PI=3.14159265358979323846;
constexpr float HALF_WIDTH=60, HALF_HEIGHT=18;
struct Point { float x=0,y=0; };
struct Node { Json item; Point point,local; std::string parent; bool group=false; std::vector<std::string> chain; };
struct Edge { Point a,b; std::vector<std::string> chain; };
struct Scene { std::vector<Node> nodes;std::vector<Edge> edges; };
inline bool fixed(const std::string& id) {
    static const std::set<std::string> ids={"sys-prog","sys-settings","sys-cmd","sys-regedit","sys-disks","sys-devices","sys-control"};
    return ids.count(id)!=0;
}
inline bool positionFixed(const std::string&) {return false;}
inline Point polar(int position,float radius) {
    const double angle=-PI/2+(position-1)*2*PI/7;
    return {radius*static_cast<float>(std::cos(angle)),radius*static_cast<float>(std::sin(angle))};
}
inline Point localPosition(const Json& config,const Json& item,bool root,int index=0,int count=7) {
    const auto id=item.at("id").get<std::string>();
    if(!positionFixed(id)&&config.contains("layout")&&config["layout"].contains("nodes")&&config["layout"]["nodes"].contains(id)) {
        const auto& point=config["layout"]["nodes"][id];return {point["x"].get<float>(),point["y"].get<float>()};
    }
    float radius=root?185.0f:175.0f;
    const std::string parent=root?"@root":item.contains("path")?item["groupId"].get<std::string>():item["parentId"].get<std::string>();
    if(config.contains("layout")&&config["layout"].contains("branches")&&config["layout"]["branches"].contains(parent))radius=config["layout"]["branches"][parent].value("radius",radius);
    if(!(config.contains("layout")&&config["layout"].contains("branches")&&config["layout"]["branches"].contains(parent)&&config["layout"]["branches"][parent].contains("radius"))&&count>1)radius=std::max(radius,static_cast<float>((HALF_WIDTH*2+16)/(2*std::sin(PI/count))));
    const double angle=-PI/2+index*2*PI/std::max(1,count);return {radius*static_cast<float>(std::cos(angle)),radius*static_cast<float>(std::sin(angle))};
}
inline void children(const Json& config,const std::string& parent,Point hub,const std::vector<std::string>& ancestors,const std::set<std::string>* expanded,Scene& scene,int depth=0) {
    if(depth>static_cast<int>(config.at("groups").size()))return;
    std::vector<Json> items;
    for(const auto& g:config.at("groups"))if((g["parentId"].is_null()?std::string{}:g["parentId"].get<std::string>())==parent)items.push_back(g);
    for(const auto& a:config.at("apps"))if(a["groupId"]==parent)items.push_back(a);
    std::sort(items.begin(),items.end(),[](const Json& a,const Json& b){return a["position"].get<int>()<b["position"].get<int>();});
    const auto defaultLength=config.contains("launcher")?config["launcher"].value("branchLength",220.0f):220.0f;
    for(size_t index=0;index<items.size();++index) {const auto& item=items[index];
        const std::string id=item["id"].get<std::string>();const bool group=!item.contains("path");
        const auto local=localPosition(config,item,parent.empty(),static_cast<int>(index),static_cast<int>(items.size()));
        const Point point{hub.x+local.x,hub.y+local.y};auto chain=ancestors;if(group)chain.push_back(id);
        Point origin=hub;if(group&&!parent.empty())for(const auto& node:scene.nodes)if(node.item["id"]==parent){origin=node.point;break;}
        scene.nodes.push_back({item,point,local,parent,group,chain});if(!parent.empty())scene.edges.push_back({origin,point,chain});
        if(group&&(!expanded||expanded->count(id))) {
            float norm=std::hypot(local.x,local.y);auto direction=local;
            if(norm<=1){direction=polar(item["position"].get<int>(),1);norm=1;}
            float length=defaultLength;double angle=std::atan2(direction.y,direction.x);
            if(config.contains("layout")&&config["layout"].contains("branches")&&config["layout"]["branches"].contains(id)) {
                const auto& settings=config["layout"]["branches"][id];length=settings.value("length",length);angle=settings.value("angle",angle);
            }
            const Point branch{point.x+static_cast<float>(std::cos(angle)*length),point.y+static_cast<float>(std::sin(angle)*length)};
            if(std::any_of(config["apps"].begin(),config["apps"].end(),[&](const auto& app){return app["groupId"]==id;}))scene.edges.push_back({point,branch,chain});children(config,id,branch,chain,expanded,scene,depth+1);
        }
    }
}
inline std::vector<std::pair<std::string,std::string>> rootLinks(const Json& config){
    std::vector<Json> roots;for(const auto& g:config.at("groups"))if(g["parentId"].is_null())roots.push_back(g);std::sort(roots.begin(),roots.end(),[](const auto& a,const auto& b){return a["position"].template get<int>()<b["position"].template get<int>();});
    std::set<std::string> ids;for(const auto& g:roots)ids.insert(g["id"].get<std::string>());std::vector<std::pair<std::string,std::string>> links;std::set<std::pair<std::string,std::string>> unique;
    const bool custom=config.contains("layout")&&config["layout"].contains("rootLinks")&&config["layout"]["rootLinks"].is_array();
    if(!custom){for(size_t i=0;i<roots.size();++i)for(size_t j=i+1;j<roots.size();++j)links.push_back({roots[i]["id"].get<std::string>(),roots[j]["id"].get<std::string>()});return links;}
    for(const auto& pair:config["layout"]["rootLinks"]){if(!pair.is_array()||pair.size()!=2||!pair[0].is_string()||!pair[1].is_string())continue;const auto a=pair[0].get<std::string>(),b=pair[1].get<std::string>();if(a==b||!ids.count(a)||!ids.count(b))continue;const std::pair<std::string,std::string> key{std::min(a,b),std::max(a,b)};if(unique.insert(key).second)links.push_back({a,b});}
    if(!roots.empty()){const auto first=roots[0]["id"].get<std::string>();std::set<std::string> reached{first};const auto visit=[&]{bool changed=true;while(changed){changed=false;for(const auto& pair:links){if(reached.count(pair.first)&&reached.insert(pair.second).second)changed=true;if(reached.count(pair.second)&&reached.insert(pair.first).second)changed=true;}}};visit();for(const auto& root:roots){const auto id=root["id"].get<std::string>();if(!reached.count(id)){links.push_back({first,id});visit();}}}
    return links;
}
inline Scene makeScene(const Json& config,const std::set<std::string>* expanded=nullptr) {
    Scene scene;children(config,"",{}, {},expanded,scene);
    for(const auto& link:rootLinks(config)){Point a{},b{};bool hasA=false,hasB=false;for(const auto& node:scene.nodes){if(node.item["id"]==link.first){a=node.point;hasA=true;}if(node.item["id"]==link.second){b=node.point;hasB=true;}}if(hasA&&hasB)scene.edges.push_back({a,b,{}});}
    return scene;
}
inline float segmentDistance(Point p,Point a,Point b) {
    const float dx=b.x-a.x,dy=b.y-a.y;
    const float square=dx*dx+dy*dy;
    const float t=square>0?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/square,0.0f,1.0f):0;
    return std::hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);
}
inline bool contains(const std::vector<std::string>& chain,const std::string& group){return group.empty()||std::find(chain.begin(),chain.end(),group)!=chain.end();}
inline bool inside(const Scene& scene,Point point,float corridor,const std::string& group="") {
    for(const auto& node:scene.nodes)if(contains(node.chain,group)&&std::abs(point.x-node.point.x)<=HALF_WIDTH+8&&std::abs(point.y-node.point.y)<=HALF_HEIGHT+8)return true;
    for(const auto& edge:scene.edges)if(contains(edge.chain,group)&&segmentDistance(point,edge.a,edge.b)<=corridor)return true;
    return false;
}
inline int hit(const Scene& scene,Point point) {
    for(int i=static_cast<int>(scene.nodes.size())-1;i>=0;--i)if(std::abs(point.x-scene.nodes[i].point.x)<=HALF_WIDTH&&std::abs(point.y-scene.nodes[i].point.y)<=HALF_HEIGHT)return i;
    return -1;
}
}
