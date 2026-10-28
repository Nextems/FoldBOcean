#pragma once
#include "animation.h"
namespace tree {
struct Connection {Point start,end,c1,c2,join;bool curved=false;};
inline bool samePoint(Point a,Point b){return std::hypot(a.x-b.x,a.y-b.y)<0.01f;}
inline Point toward(Point a,Point b,float distance){const float length=std::hypot(b.x-a.x,b.y-a.y);return {a.x+(b.x-a.x)*distance/length,a.y+(b.y-a.y)*distance/length};}
inline std::vector<Connection> roundedConnections(const Frame& frame) {
    std::vector<Connection> paths;for(const auto& edge:frame.edges)paths.push_back({edge.first,edge.second});
    for(size_t i=0;i<frame.edges.size();++i){
        const auto& edge=frame.edges[i];const Point hub=edge.second;
        if(std::any_of(frame.points.begin(),frame.points.end(),[&](Point p){return samePoint(p,hub);}))continue;
        const float length=std::hypot(hub.x-edge.first.x,hub.y-edge.first.y);if(length<0.01f)continue;
        std::vector<size_t> children;float radius=std::min(32.0f,length*0.4f);
        for(size_t j=0;j<frame.edges.size();++j)if(j!=i&&samePoint(frame.edges[j].first,hub)){
            const auto end=frame.edges[j].second;const float childLength=std::hypot(end.x-hub.x,end.y-hub.y);if(childLength<0.01f)continue;
            radius=std::min(radius,childLength*0.4f);children.push_back(j);
        }
        if(children.empty())continue;
        const auto start=toward(hub,edge.first,radius);paths[i].end=start;
        for(const auto child:children){auto& path=paths[child];const auto join=toward(hub,frame.edges[child].second,radius);
            path.start=start;path.join=join;path.c1={start.x+(hub.x-start.x)*2/3,start.y+(hub.y-start.y)*2/3};path.c2={join.x+(hub.x-join.x)*2/3,join.y+(hub.y-join.y)*2/3};path.curved=true;
        }
    }
    return paths;
}
}
void paintConnections(Gdiplus::Graphics& graphics,const tree::Frame& frame) {
    const auto paths=tree::roundedConnections(frame);
    for(size_t i=0;i<paths.size();++i){
        const auto& path=paths[i];Gdiplus::Pen pen(animatedTint("branches","edge-"+std::to_string(i),Gdiplus::Color(static_cast<BYTE>(255*frame.edgeAlpha[i]),123,127,133)),1.4f);
        Gdiplus::GraphicsPath shape;
        if(path.curved){shape.AddBezier(path.start.x,path.start.y,path.c1.x,path.c1.y,path.c2.x,path.c2.y,path.join.x,path.join.y);shape.AddLine(path.join.x,path.join.y,path.end.x,path.end.y);}
        else shape.AddLine(path.start.x,path.start.y,path.end.x,path.end.y);
        graphics.DrawPath(&pen,&shape);
    }
}
