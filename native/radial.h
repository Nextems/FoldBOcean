#pragma once
#include "tree.h"
#include "menu-limits.h"
namespace radial {
constexpr float WIDTH=60;
struct Panel {std::string id;int depth;tree::Point center;float radius,inner;};
struct Sector {tree::Json item;std::string parent;int depth,index;tree::Point center,label;float radius,inner;double start,sweep,angle;bool group;std::vector<std::string> chain;std::string color;float layerFactor=1;};
struct Scene {std::vector<Panel> panels;std::vector<Sector> sectors;};
inline float factor(int depth,float step=6){return 1-step/100*std::min(depth,2); }
inline float thickness(int depth,float step=6){return WIDTH*factor(depth,step);}
inline float innerRadius(int depth,float step=6){float inner=70;for(int d=0;d<depth;++d)inner+=thickness(d,step)+10;return inner;}
inline float radius(int depth,float step=6){return innerRadius(depth,step)+thickness(depth,step);}
inline float iconSize(int depth){const float sizes[]={28,24,20};return sizes[std::min(depth,2)];}
inline float iconRadius(const Sector& s){const float half=iconSize(s.depth)*s.layerFactor/2;return std::sqrt((s.radius-4)*(s.radius-4)-half*half)-half;}
inline int labelLimit(int depth){const int limits[]={8,12,10};return limits[std::min(depth,2)];}
inline float labelSize(int depth){const float sizes[]={16,15,14.5f};return sizes[std::min(depth,2)];}
inline int capacity(int depth){return menuLimits().at("capacities").at(std::min(depth,2)).get<int>();}
inline std::vector<tree::Json> items(const tree::Json& config,const std::string& parent){std::vector<tree::Json> result;for(const auto& g:config.at("groups"))if((g["parentId"].is_null()?"@root":g["parentId"].get<std::string>())==parent)result.push_back(g);for(const auto& a:config.at("apps"))if(a["groupId"]==parent)result.push_back(a);std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a["position"].template get<int>()<b["position"].template get<int>();});return result;}
inline Scene makeScene(const tree::Json& config,const std::vector<std::string>& path={},float =1200,float =800){
    Scene scene;std::string parent="@root";tree::Point center{};std::vector<std::string> chain;
    const auto style=config.value("radial",tree::Json::object());const auto colors=style.value("colors",tree::Json::object()),rotations=style.value("rotation",tree::Json::object());
    for(size_t depth=0;depth<=path.size();++depth){
        const float layerStep=config.value("appearance",tree::Json::object()).value("ringStep",6.f),outer=radius(static_cast<int>(depth),layerStep),inner=innerRadius(static_cast<int>(depth),layerStep);const auto children=items(config,parent);const double step=children.empty()?2*tree::PI:2*tree::PI/children.size(),start=-tree::PI/2-step/2+rotations.value(parent,0.0);
        scene.panels.push_back({parent,static_cast<int>(depth),center,outer,inner});int selected=-1;
        for(size_t i=0;i<children.size();++i){const auto& item=children[i];const bool group=!item.contains("path");const auto id=item["id"].get<std::string>();const double angle=start+(i+0.5)*step;auto ancestry=chain;if(group)ancestry.push_back(id);
            scene.sectors.push_back({item,parent,static_cast<int>(depth),static_cast<int>(i),center,{center.x+static_cast<float>(std::cos(angle)*(outer-thickness(static_cast<int>(depth),layerStep)/2)),center.y+static_cast<float>(std::sin(angle)*(outer-thickness(static_cast<int>(depth),layerStep)/2))},outer,inner,start+i*step,step,angle,group,ancestry,colors.value(id,style.value("color",std::string("#70818b")))});
            scene.sectors.back().layerFactor=factor(static_cast<int>(depth),layerStep)/factor(static_cast<int>(depth));
            if(depth<path.size()&&group&&id==path[depth])selected=static_cast<int>(scene.sectors.size())-1;
        }
        if(selected<0)break;
        const auto sector=scene.sectors[selected];center={};parent=sector.item["id"].get<std::string>();chain=sector.chain;
    }
    return scene;
}
inline bool hitSector(const Sector& sector,tree::Point point,float zoom=1,float shift=0){const float x=point.x-sector.center.x,y=point.y-sector.center.y,r=std::hypot(x,y);if(r<(sector.inner+shift)*zoom||r>(sector.radius+shift)*zoom)return false;double angle=std::fmod(std::atan2(y,x)-sector.start,2*tree::PI);if(angle<0)angle+=2*tree::PI;return angle<sector.sweep;}
inline int maximumPanelDepth(const tree::Json& config){
    int maximum=0;std::map<std::string,std::string> parents;
    for(const auto& group:config.at("groups"))parents[group.at("id").get<std::string>()]=group.at("parentId").is_null()?"":group.at("parentId").get<std::string>();
    for(const auto& pair:parents){int depth=1;auto parent=pair.second;while(!parent.empty()&&depth<4){const auto found=parents.find(parent);if(found==parents.end())break;parent=found->second;++depth;}maximum=std::max(maximum,depth);}
    return maximum;
}
inline int hit(const Scene& scene,tree::Point point){for(int i=static_cast<int>(scene.sectors.size())-1;i>=0;--i)if(hitSector(scene.sectors[i],point))return i;return -1;}
inline bool inside(const Scene& scene,tree::Point point,float margin=0){for(const auto& p:scene.panels)if(std::hypot(point.x-p.center.x,point.y-p.center.y)<=p.radius+margin)return true;return false;}
}
