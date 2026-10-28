#pragma once
#include "radial.h"
#include "menu-motion.h"
#include "layered-surface.h"
#include <cfloat>

LayeredSurface renderSurface;
struct ArcLabelLayout {std::vector<std::wstring> glyphs;std::vector<float> widths;float total=0;};
std::map<std::pair<std::wstring,float>,ArcLabelLayout> arcLabels;
struct ArcLabelRaster {std::unique_ptr<Bitmap> image;RectF bounds;float scale=0,inner=0;double angle=0;};
std::map<std::string,ArcLabelRaster> arcRasters;
const json& configSection(const char* key){static const json empty=json::object();const auto found=configuration.find(key);return found==configuration.end()?empty:*found;}
UINT animationTimerInterval=0;
void startAnimationTimer(UINT interval){if(windowHandle&&animationTimerInterval!=interval){if(SetTimer(windowHandle,4,interval,nullptr))animationTimerInterval=interval;}}
void stopAnimationTimer(){if(windowHandle)KillTimer(windowHandle,4);animationTimerInterval=0;}
tree::Scene scene;
radial::Scene rings;
std::vector<tree::Reveal> reveals;
std::set<std::string> expanded;
std::map<std::string,ULONGLONG> collapseAt,ringOpened;
std::map<std::string,motion::RingTransition> ringTransitions;
float worldMinX=0,worldMinY=0;
int canvasWidth=720,canvasHeight=720;
std::unique_ptr<Bitmap> blurredBackground;
POINT openedPointer{};bool hoverArmed=true;
#include "tooltips.h"
int preference(const char* key,int fallback){return configuration.contains("launcher")?configuration["launcher"].value(key,fallback):fallback;}
tree::Point worldPoint(POINT point){ScreenToClient(windowHandle,&point);return {point.x/scale+worldMinX,point.y/scale+worldMinY};}
bool webView(){return configuration.contains("launcher")&&configuration["launcher"].value("view",std::string("radial"))=="web";}
ULONGLONG openingDuration(){return static_cast<ULONGLONG>(configSection("appearance").value("openMs",450.0));}
void clearWrappedLabels();
void updateEntries(){
    clearTooltips();entries.clear();arcLabels.clear();arcRasters.clear();clearWrappedLabels();hover=-1;
    if(webView()){rings={};scene=tree::makeScene(configuration,&expanded);for(const auto& node:scene.nodes){const auto& item=node.item;entries.push_back({item["id"],wide(item["name"].get<std::string>()),node.group?L"":wide(item.value("path",std::string{})),item["position"],node.group,item.value("noIcon",false)});}}
    else {rings=radial::makeScene(configuration,navigation);scene={};
    for(const auto& sector:rings.sectors){const auto& item=sector.item;entries.push_back({item["id"],wide(item["name"].get<std::string>()),sector.group?L"":wide(item.value("path",std::string{})),item["position"],sector.group,item.value("noIcon",false)});const auto placed=motion::scaledRing(sector,1,motion::compactShift(sector.depth,rings.panels.size(),configSection("appearance").value("ringStep",6.f)));scene.nodes.push_back({item,placed.label,{},sector.parent=="@root"?"":sector.parent,sector.group,sector.chain});}
    }
    // Queue shell extraction outside the UI thread; animate immediately.
    syncIcons();syncTooltips();const auto now=GetTickCount64();std::set<std::string> present;
    for(const auto& panel:rings.panels){present.insert(panel.id);const bool submerged=motion::sunk(panel.depth,rings.panels.size());const float sinkZoom=rings.panels.front().radius*.98f/panel.radius,shift=motion::compactShift(panel.depth,rings.panels.size(),configSection("appearance").value("ringStep",6.f));
        if(!ringOpened.count(panel.id)){ringOpened[panel.id]=now;ringTransitions[panel.id]={};ringTransitions[panel.id].duration=openingDuration();ringTransitions[panel.id].from.shift=shift;ringTransitions[panel.id].to.shift=shift;if(shift<0){auto& transition=ringTransitions[panel.id];transition.from.shift+=radial::thickness(std::max(1,static_cast<int>(rings.panels.size())-4),configSection("appearance").value("ringStep",6.f))+10;transition.alphaDuration=openingDuration();transition.duration=std::max(openingDuration(),motion::SINK_MS);}if(submerged){auto& transition=ringTransitions[panel.id];transition.to={sinkZoom,0};transition.submerged=true;}}
        else motion::retarget(ringTransitions[panel.id],ringOpened[panel.id],now,submerged,sinkZoom,shift);
    }
    for(auto it=ringOpened.begin();it!=ringOpened.end();)if(!present.count(it->first))it=ringOpened.erase(it);else ++it;
    for(auto it=ringTransitions.begin();it!=ringTransitions.end();)if(!present.count(it->first))it=ringTransitions.erase(it);else ++it;
    startAnimationTimer(16);
}
void text(Graphics& g,const std::wstring& value,RectF rect,float size,Color color,bool bold=false,const wchar_t* family=L"Segoe UI",bool fit=false){Font font(family,size,bold?FontStyleBold:FontStyleRegular,UnitPixel);SolidBrush brush(color);StringFormat format;format.SetAlignment(StringAlignmentCenter);format.SetLineAlignment(StringAlignmentCenter);format.SetTrimming(fit?StringTrimmingNone:StringTrimmingEllipsisCharacter);format.SetFormatFlags(StringFormatFlagsNoWrap);if(fit){RectF bounds;g.MeasureString(value.c_str(),-1,&font,PointF(0,0),&format,&bounds);const float fittedSize=bounds.Width>rect.Width?size*rect.Width/bounds.Width:size;Font fitted(family,fittedSize,bold?FontStyleBold:FontStyleRegular,UnitPixel);g.DrawString(value.c_str(),-1,&fitted,rect,&format,&brush);}else g.DrawString(value.c_str(),-1,&font,rect,&format,&brush);}
void sectorShape(GraphicsPath& path,const radial::Sector& s){
    path.SetFillMode(FillModeAlternate);const float x=s.center.x,y=s.center.y,r=s.radius,inner=s.inner;
    if(s.sweep>=2*PI-0.001){path.AddEllipse(x-r,y-r,r*2,r*2);path.AddEllipse(x-inner,y-inner,inner*2,inner*2);return;}
    const auto& style=configSection("radial");const double gap=std::min(0.018,s.sweep*0.04),a=s.start+gap,b=s.start+s.sweep-gap;
    const float corner=style.value("rounded",true)?std::max(0.f,std::min({style.value("roundness",6.f),(r-inner)/3,static_cast<float>((b-a)*inner/4)})):0;
    const auto point=[&](float radius,double angle){return PointF(x+static_cast<float>(std::cos(angle)*radius),y+static_cast<float>(std::sin(angle)*radius));};
    const auto quad=[&](PointF from,PointF control,PointF to){path.AddBezier(from,PointF(from.X+(control.X-from.X)*2/3,from.Y+(control.Y-from.Y)*2/3),PointF(to.X+(control.X-to.X)*2/3,to.Y+(control.Y-to.Y)*2/3),to);};
    const float degrees=static_cast<float>(180/PI);
    if(corner>0.01f){const double outer=corner/r,inside=corner/inner;path.AddArc(x-r,y-r,r*2,r*2,static_cast<float>(a+outer)*degrees,static_cast<float>(b-a-2*outer)*degrees);quad(point(r,b-outer),point(r,b),point(r-corner,b));path.AddLine(point(r-corner,b),point(inner+corner,b));quad(point(inner+corner,b),point(inner,b),point(inner,b-inside));path.AddArc(x-inner,y-inner,inner*2,inner*2,static_cast<float>(b-inside)*degrees,static_cast<float>(a-b+2*inside)*degrees);quad(point(inner,a+inside),point(inner,a),point(inner+corner,a));path.AddLine(point(inner+corner,a),point(r-corner,a));quad(point(r-corner,a),point(r,a),point(r,a+outer));path.CloseFigure();return;}
    const float start=static_cast<float>(a)*degrees,sweep=static_cast<float>(b-a)*degrees;path.AddArc(x-r,y-r,r*2,r*2,start,sweep);path.AddLine(point(r,b),point(inner,b));path.AddArc(x-inner,y-inner,inner*2,inner*2,start+sweep,-sweep);path.CloseFigure();
}
void drawArcLabelRaw(Graphics& g,const radial::Sector& sector,const std::wstring& name,float alpha){
    const int limits[]={8,12,10};const float sizes[]={16,15,14.5f};const int level=std::min(sector.depth,2);const float size=sizes[level]*sector.layerFactor;const bool lower=std::sin(sector.angle)>0.1;const float radius=sector.inner+(lower?size+8:12),sign=lower?-1.f:1.f;
    Font font(L"Segoe UI",size,FontStyleBold,UnitPixel);StringFormat measureFormat(StringFormat::GenericTypographic());measureFormat.SetFormatFlags(measureFormat.GetFormatFlags()|StringFormatFlagsMeasureTrailingSpaces);const auto format=&measureFormat;const auto label=menuLabel(name,limits[level]);
    auto [cached,inserted]=arcLabels.try_emplace({label,size});auto& layout=cached->second;
    if(inserted)for(size_t i=0;i<label.size();){const size_t begin=i++;if(label[begin]>=0xd800&&label[begin]<=0xdbff&&i<label.size()&&label[i]>=0xdc00&&label[i]<=0xdfff)++i;layout.glyphs.push_back(label.substr(begin,i-begin));RectF bounds;g.MeasureString(layout.glyphs.back().c_str(),static_cast<INT>(layout.glyphs.back().size()),&font,PointF(0,0),format,&bounds);layout.widths.push_back(bounds.Width);layout.total+=bounds.Width;}
    const auto& glyphs=layout.glyphs;const auto& widths=layout.widths;const float total=layout.total;
    const float available=radius*static_cast<float>(std::min(sector.sweep-0.09,PI*0.95));const float fit=total>0?std::min(1.f,available/total):1;float offset=-total/2;
    SolidBrush shadow(Color(static_cast<BYTE>(220*alpha),8,12,16)),ink(Color(static_cast<BYTE>(255*alpha),249,251,253));
    for(size_t i=0;i<glyphs.size();++i){const double angle=sector.angle+sign*(offset+widths[i]/2)*fit/radius;const auto state=g.Save();g.TranslateTransform(sector.center.x+static_cast<float>(std::cos(angle)*radius),sector.center.y+static_cast<float>(std::sin(angle)*radius));g.RotateTransform(static_cast<float>((angle+sign*PI/2)*180/PI));g.ScaleTransform(fit,fit);const PointF position(-widths[i]/2,-size);g.DrawString(glyphs[i].c_str(),static_cast<INT>(glyphs[i].size()),&font,PointF(position.X,position.Y+1),format,&shadow);g.DrawString(glyphs[i].c_str(),static_cast<INT>(glyphs[i].size()),&font,position,format,&ink);g.Restore(state);offset+=widths[i];}
}
void drawArcLabel(Graphics& g,const radial::Sector& sector,const std::wstring& name,float alpha){
    if(animationTimerInterval!=33||alpha<1){drawArcLabelRaw(g,sector,name,alpha);return;}
    const float zoom=scale;auto& cached=arcRasters[sector.item.at("id").get<std::string>()];
    if(!cached.image||cached.scale!=zoom||cached.inner!=sector.inner||cached.angle!=sector.angle){
        const int limits[]={8,12,10};const float sizes[]={16,15,14.5f};const int level=std::min(sector.depth,2);const float size=sizes[level]*sector.layerFactor;
        const auto measured=arcLabels.find({menuLabel(name,limits[level]),size});
        if(measured==arcLabels.end()){drawArcLabelRaw(g,sector,name,alpha);return;}
        const float radius=sector.inner+(std::sin(sector.angle)>0.1?size+8:12),available=radius*static_cast<float>(std::min(sector.sweep-0.09,PI*.95));
        const float arc=std::min(measured->second.total,available)/(2*radius),padding=size*1.6f+3;
        float left=FLT_MAX,top=FLT_MAX,right=-FLT_MAX,bottom=-FLT_MAX;
        for(int i=0;i<=32;++i){const double angle=sector.angle-arc+2*arc*i/32;const float x=sector.center.x+static_cast<float>(std::cos(angle))*radius,y=sector.center.y+static_cast<float>(std::sin(angle))*radius;left=std::min(left,x-padding);right=std::max(right,x+padding);top=std::min(top,y-padding);bottom=std::max(bottom,y+padding);}
        left=std::floor(left*zoom)/zoom;top=std::floor(top*zoom)/zoom;
        const int width=static_cast<int>(std::ceil((right-left)*zoom)),height=static_cast<int>(std::ceil((bottom-top)*zoom));
        cached.image=std::make_unique<Bitmap>(width,height,PixelFormat32bppPARGB);cached.bounds={left,top,width/zoom,height/zoom};cached.scale=zoom;cached.inner=sector.inner;cached.angle=sector.angle;
        {Graphics raster(cached.image.get());raster.Clear(Color(0,0,0,0));Matrix transform(zoom,0,0,zoom,-left*zoom,-top*zoom);raster.SetTransform(&transform);raster.SetSmoothingMode(SmoothingModeAntiAlias);raster.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);drawArcLabelRaw(raster,sector,name,1);}
    }
    g.DrawImage(cached.image.get(),cached.bounds,0,0,static_cast<REAL>(cached.image->GetWidth()),static_cast<REAL>(cached.image->GetHeight()),UnitPixel);
}
Color tint(const std::string& hex,BYTE alpha){try{const auto rgb=std::stoul(hex.substr(1),nullptr,16);return Color(alpha,static_cast<BYTE>(rgb>>16),static_cast<BYTE>(rgb>>8),static_cast<BYTE>(rgb));}catch(...){return Color(alpha,112,129,139);}}
#include "block-label.h"
#include "rgb.h"
void captureBackground(){
    blurredBackground.reset();RECT rect{};GetWindowRect(windowHandle,&rect);HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,canvasWidth,canvasHeight);auto old=SelectObject(dc,bitmap);
    const BOOL captured=BitBlt(dc,0,0,canvasWidth,canvasHeight,screen,rect.left,rect.top,SRCCOPY);SelectObject(dc,old);DeleteDC(dc);ReleaseDC(nullptr,screen);
    if(captured){Bitmap source(bitmap,nullptr);const auto style=configuration.value(webView()?"webStyle":"radial",json::object());const int amount=static_cast<int>(style.value("blur",12.0));const int divisor=std::max(1,1+amount);const int w=std::max(1,canvasWidth/divisor),h=std::max(1,canvasHeight/divisor);blurredBackground=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB);Graphics g(blurredBackground.get());g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.DrawImage(&source,0,0,w,h);}
    DeleteObject(bitmap);
}
#include "web-runtime.h"
void paintScene(Graphics& g){
    g.Clear(Color(0,0,0,0));const float zoom=scale;Matrix transform(zoom,0,0,zoom,-worldMinX*zoom,-worldMinY*zoom);g.SetTransform(&transform);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    if(webView()){paintWebScene(g);return;}
    const auto now=GetTickCount64();
    for(size_t i=0;i<rings.sectors.size();++i){const auto& target=rings.sectors[i];const auto& item=entries[i];const auto value=ringOpened.count(target.parent)?motion::sample(ringTransitions[target.parent],ringOpened[target.parent],now):motion::RingValue{};const float alpha=value.alpha;if(alpha<=0)continue;const auto s=motion::scaledRing(target,value.zoom,value.shift);
        GraphicsPath path;sectorShape(path,s);const auto saved=g.Save();g.SetClip(&path);
        if(blurredBackground){ColorMatrix matrix={1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,alpha,0,0,0,0,0,1};ImageAttributes attributes;attributes.SetColorMatrix(&matrix);g.DrawImage(blurredBackground.get(),RectF(worldMinX,worldMinY,canvasWidth/zoom,canvasHeight/zoom),0,0,static_cast<REAL>(blurredBackground->GetWidth()),static_cast<REAL>(blurredBackground->GetHeight()),UnitPixel,&attributes);}
        SolidBrush dark(Color(static_cast<BYTE>((blurredBackground?135:195)*alpha),17,20,24));g.FillPath(&dark,&path);SolidBrush color(animatedTint("radial",item.id,tint(s.color,static_cast<BYTE>((static_cast<int>(i)==hover||expanded.count(item.id)?95:43)*alpha))));g.FillPath(&color,&path);g.Restore(saved);
        Pen border(animatedTint("radial",item.id,tint(s.color,static_cast<BYTE>((static_cast<int>(i)==hover||expanded.count(item.id)?240:140)*alpha))),static_cast<int>(i)==hover?1.8f:1.f);g.DrawPath(&border,&path);
        if(item.noIcon||missingProgram(item)){drawRingBlockLabel(g,s,item,alpha);continue;}
        const float factor=radial::factor(s.depth),iconSize=radial::iconSize(s.depth)*s.layerFactor,iconRadius=radial::iconRadius(s);const PointF location(s.center.x+static_cast<float>(std::cos(s.angle)*iconRadius),s.center.y+static_cast<float>(std::sin(s.angle)*iconRadius));const auto icon=iconCache.find(item.group?"@folder":item.id);
        const auto iconState=g.Save();g.SetClip(&path,CombineModeIntersect);g.TranslateTransform(location.X,location.Y);g.RotateTransform(static_cast<float>((s.angle+(std::sin(s.angle)>0.1?-PI/2:PI/2))*180/PI));
        if(icon!=iconCache.end()&&icon->second.image){ColorMatrix matrix={1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,alpha,0,0,0,0,0,1};ImageAttributes attributes;attributes.SetColorMatrix(&matrix);attributes.SetWrapMode(WrapModeTileFlipXY);g.SetPixelOffsetMode(PixelOffsetModeHighQuality);g.DrawImage(icon->second.image.get(),RectF(-iconSize/2,-iconSize/2,iconSize,iconSize),0,0,static_cast<REAL>(icon->second.image->GetWidth()),static_cast<REAL>(icon->second.image->GetHeight()),UnitPixel,&attributes);}else text(g,item.group?L"▱":L"↗",{-15,-15,30,30},24*factor,Color(static_cast<BYTE>(230*alpha),220,224,229));
        g.Restore(iconState);drawArcLabel(g,s,item.name,alpha);
    }
}
void render(){if(!visible||!renderSurface.ensure(canvasWidth,canvasHeight))return;{Graphics g(renderSurface.image());paintScene(g);}renderSurface.present(windowHandle);if(!animationTimerInterval)renderSurface.reset();}
void hide(){visible=false;syncHookState();demoMode=false;hover=-1;for(int timer:{1,3})KillTimer(windowHandle,timer);stopAnimationTimer();reveals.clear();clearTooltips();cancelPendingIcons();if(windowHandle)SetTimer(windowHandle,5,30000,nullptr);blurredBackground.reset();renderSurface.reset();arcLabels.clear();arcRasters.clear();clearWrappedLabels();expanded.clear();navigation.clear();collapseAt.clear();ringOpened.clear();ringTransitions.clear();scene={};rings={};entries.clear();ShowWindow(windowHandle,SW_HIDE);SetWindowLongPtrW(windowHandle,GWL_EXSTYLE,(GetWindowLongPtrW(windowHandle,GWL_EXSTYLE)&~WS_EX_APPWINDOW)|WS_EX_TOOLWINDOW);if(GetForegroundWindow()==windowHandle&&IsWindow(previousWindow))SetForegroundWindow(previousWindow);}
void prepareViewport(POINT){
    if(webView()){prepareWebViewport();return;}
    const auto monitor=launcherMonitor();const int width=monitor.rcWork.right-monitor.rcWork.left,height=monitor.rcWork.bottom-monitor.rcWork.top;const int size=std::min({width,height,1080});canvasWidth=canvasHeight=size;
    // Keep the center fixed and reserve room for multiple rings; add gentle zoom only for unusually deep menus.
    const float step=configSection("appearance").value("ringStep",6.f),extent=std::max(360.f,radial::radius(std::min(4,static_cast<int>(navigation.size())),step)+16);scale=std::min(configSection("appearance").value("radial",100.f)/100,size/(extent*2));
    // Reserve the deepest reachable (packed) ring, without allocating invisible
    // full-screen margins. The center and the user's visual scale stay fixed.
    const float reserve=radial::radius(radial::maximumPanelDepth(configuration),step)+18;
    canvasWidth=canvasHeight=std::min(size,static_cast<int>(std::ceil(reserve*2*scale)));
    worldMinX=-canvasWidth/(2*scale);worldMinY=worldMinX;const auto bounds=centeredBounds(monitor.rcWork,canvasWidth,canvasHeight);SetWindowPos(windowHandle,HWND_TOPMOST,bounds.left,bounds.top,canvasWidth,canvasHeight,SWP_NOACTIVATE);
}
void show(bool demo=false){if(paused&&!demo)return;if(configDirty)loadConfiguration();if(fullscreenBlocksInvocation())return;KillTimer(windowHandle,5);ShowWindow(windowHandle,SW_HIDE);navigation.clear();expanded.clear();collapseAt.clear();reveals.clear();ringOpened.clear();ringTransitions.clear();demoMode=demo;bindHeld=!demo;hoverArmed=true;hover=-1;POINT p{};GetCursorPos(&p);previousWindow=GetForegroundWindow();prepareViewport(p);if(webView()&&configSection("webStyle").value("blur",0.0)==0)blurredBackground.reset();else captureBackground();visible=true;syncHookState();if(demo)SetWindowLongPtrW(windowHandle,GWL_EXSTYLE,(GetWindowLongPtrW(windowHandle,GWL_EXSTYLE)&~WS_EX_TOOLWINDOW)|WS_EX_APPWINDOW);updateEntries();if(webView())reveals.push_back({"",{worldMinX+canvasWidth/(2*scale),worldMinY+canvasHeight/(2*scale)},GetTickCount64(),0,false,0,openingDuration()});render();ShowWindow(windowHandle,SW_SHOWNOACTIVATE);SetForegroundWindow(windowHandle);}
int hitRing(tree::Point p){const auto now=GetTickCount64();for(int i=static_cast<int>(rings.sectors.size())-1;i>=0;--i){const auto& sector=rings.sectors[i];if(motion::sunk(sector.depth,rings.panels.size()))continue;const auto value=ringOpened.count(sector.parent)?motion::sample(ringTransitions[sector.parent],ringOpened[sector.parent],now):motion::RingValue{};if(value.alpha<.5f)continue;if(radial::hitSector(sector,p,value.zoom,value.shift))return i;}return -1;}
int hit(POINT point){return webView()?(reveals.empty()?tree::hit(scene,worldPoint(point)):tree::hit(tree::animationFrame(scene,reveals,GetTickCount64()),worldPoint(point))):hitRing(worldPoint(point));}
bool menuInside(tree::Point point,float margin=0){return webView()?tree::inside(tree::animationFrame(scene,reveals,GetTickCount64()),point,margin):hitRing(point)>=0;}
size_t pendingRingKeep=0;
size_t ringKeepAt(tree::Point point){const int index=hitRing(point);if(index<0)return navigation.size();const auto& sector=rings.sectors[index];return motion::parentKeep(sector.depth,sector.group&&std::find(navigation.begin(),navigation.end(),entries[index].id)!=navigation.end());}
bool insideRingMenu(tree::Point point,float margin=0){const float r=std::hypot(point.x,point.y);const auto now=GetTickCount64();for(const auto& panel:rings.panels)if(!motion::sunk(panel.depth,rings.panels.size())){const auto value=motion::sample(ringTransitions[panel.id],ringOpened[panel.id],now);if(value.alpha>.01f&&r<=(panel.radius+value.shift)*value.zoom+margin)return true;}return false;}
bool closingBranch(const std::string& id);
void resumeBranch(const std::string& id);
void scheduleCollapse(tree::Point point){
    if(webView()){const auto now=GetTickCount64();const auto frame=tree::animationFrame(scene,reveals,now);for(const auto& id:expanded){if(tree::inside(scene,point,32,id)||tree::inside(scene,frame,point,32,id)){collapseAt.erase(id);resumeBranch(id);}else if(!closingBranch(id)&&!collapseAt.count(id))collapseAt[id]=now+preference("closeDelayMs",350);}KillTimer(windowHandle,3);if(!collapseAt.empty()){ULONGLONG closest=collapseAt.begin()->second;for(const auto& pair:collapseAt)closest=std::min(closest,pair.second);SetTimer(windowHandle,3,static_cast<UINT>(closest>now?closest-now:1),nullptr);}return;}KillTimer(windowHandle,3);if(navigation.empty()){collapseAt.clear();return;}const auto now=GetTickCount64();const auto keep=ringKeepAt(point);
    if(keep<navigation.size()){collapseAt.erase("rings");if(!collapseAt.count("parent-rings")||pendingRingKeep!=keep){pendingRingKeep=keep;collapseAt["parent-rings"]=now+preference("closeDelayMs",350);}}
    else if(insideRingMenu(point,16)){collapseAt.clear();return;}
    else {collapseAt.erase("parent-rings");if(!collapseAt.count("rings"))collapseAt["rings"]=now+preference("closeDelayMs",350);}
    if(!collapseAt.empty()){const auto deadline=collapseAt.begin()->second;SetTimer(windowHandle,3,static_cast<UINT>(deadline>now?deadline-now:1),nullptr);}}
void finishCollapseGroup(const std::string& id){
    if(webView()){for(const auto& node:scene.nodes)if(node.group&&tree::contains(node.chain,id)){expanded.erase(node.item["id"].get<std::string>());collapseAt.erase(node.item["id"].get<std::string>());}auto found=std::find(navigation.begin(),navigation.end(),id);if(found!=navigation.end())navigation.erase(found,navigation.end());reveals.erase(std::remove_if(reveals.begin(),reveals.end(),[&](const auto& r){return !r.group.empty()&&!expanded.count(r.group);}),reveals.end());updateEntries();return;}auto found=std::find(navigation.begin(),navigation.end(),id);if(found!=navigation.end())navigation.erase(found,navigation.end());expanded=std::set<std::string>(navigation.begin(),navigation.end());updateEntries();}
bool closingBranch(const std::string& id){return std::any_of(reveals.begin(),reveals.end(),[&](const auto& r){return r.group==id&&r.closing;});}
void resumeBranch(const std::string& id){const auto now=GetTickCount64();for(auto& r:reveals)if(r.group==id&&r.closing){const float progress=tree::revealProgress(r,now);r.started=now;r.closing=false;r.from=progress;r.duration=openingDuration();collapseAt.erase(id);startAnimationTimer(16);}}
void collapseGroup(const std::string& id){if(!webView()){finishCollapseGroup(id);return;}if(!expanded.count(id)||closingBranch(id))return;const auto found=std::find_if(scene.nodes.begin(),scene.nodes.end(),[&](const auto& n){return n.item["id"]==id;});if(found==scene.nodes.end())return;const auto now=GetTickCount64();float from=1;for(const auto& r:reveals)if(r.group==id)from=tree::revealProgress(r,now);reveals.erase(std::remove_if(reveals.begin(),reveals.end(),[&](const auto& r){return r.group==id;}),reveals.end());reveals.push_back({id,found->point,now,static_cast<int>(found->chain.size()),true,from});collapseAt.erase(id);startAnimationTimer(16);}
void enterGroup(int index){if(index<0||index>=static_cast<int>(entries.size())||!entries[index].group)return;const auto id=entries[index].id;if(webView()){if(expanded.count(id)){resumeBranch(id);return;}const auto origin=scene.nodes[index].point;navigation=scene.nodes[index].chain;expanded.insert(id);KillTimer(windowHandle,1);updateEntries();reveals.push_back({id,origin,GetTickCount64(),static_cast<int>(navigation.size()),false,0,openingDuration()});render();return;}if(std::find(navigation.begin(),navigation.end(),id)!=navigation.end())return;navigation=rings.sectors[index].chain;expanded=std::set<std::string>(navigation.begin(),navigation.end());KillTimer(windowHandle,1);GetCursorPos(&openedPointer);hoverArmed=false;
    const float required=radial::radius(static_cast<int>(navigation.size()),configSection("appearance").value("ringStep",6.f))+16;if(required>-worldMinX){POINT p{};GetCursorPos(&p);prepareViewport(p);}
    updateEntries();render();}
void back(){if(navigation.empty())hide();else{if(webView()){const auto id=navigation.back();collapseGroup(id);render();return;}navigation.pop_back();expanded=std::set<std::string>(navigation.begin(),navigation.end());updateEntries();render();}}
void selectItem(int index){
    if(index<0||index>=static_cast<int>(entries.size()))return;Item item=entries[index];
    if(item.group){enterGroup(index);return;}
    wchar_t expandedPath[32768]{};ExpandEnvironmentStringsW(item.path.c_str(),expandedPath,32768);
    SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_FLAG_NO_UI;info.lpVerb=L"open";info.lpFile=expandedPath;info.nShow=SW_SHOWNORMAL;
    std::wstring directory;if(fs::path(expandedPath).is_absolute()){directory=fs::path(expandedPath).parent_path().wstring();info.lpDirectory=directory.c_str();}
    if(ShellExecuteExW(&info)){if(missingProgram(item))iconCache.erase(item.id);hide();return;}
    const auto error=GetLastError();
    if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND||missingProgram(item)){auto& cached=iconCache[item.id];cached.source=item.path;cached.missing=true;cached.image.reset();cached.checked=GetTickCount64();render();}
    else{hide();notify(L"Не удалось открыть «"+item.name+L"». Проверьте путь в настройках.");}
}
void pointerMoved(){movePending=false;if(!visible)return;POINT p{};GetCursorPos(&p);const bool armed=!hoverArmed&&std::hypot(double(p.x-openedPointer.x),double(p.y-openedPointer.y))>6;if(armed)hoverArmed=true;const int next=hit(p);trackTooltip(next,p);if(next!=hover||armed){hover=next;KillTimer(windowHandle,1);if(next>=0&&entries[next].group&&hoverArmed&&!expanded.count(entries[next].id)){const int delay=preference("hoverDelayMs",180);if(!delay){enterGroup(next);return;}SetTimer(windowHandle,1,delay,nullptr);}render();}scheduleCollapse(worldPoint(p));}
void advanceAnimation(){
    if(!visible){reveals.clear();stopAnimationTimer();return;}
    const bool wasAnimating=animationTimerInterval==16;
    const auto now=GetTickCount64();bool moving=false;
    if(webView()){
        std::vector<std::string> finished;for(const auto& r:reveals)if(r.closing&&now-r.started>=r.duration)finished.push_back(r.group);
        for(const auto& id:finished)finishCollapseGroup(id);
        reveals.erase(std::remove_if(reveals.begin(),reveals.end(),[&](const auto& r){return now-r.started>=r.duration;}),reveals.end());moving=!reveals.empty();
    }else for(const auto& pair:ringOpened)if(now-pair.second<ringTransitions[pair.first].duration){moving=true;break;}
    render();if(!moving){if(rgbActive())startAnimationTimer(33);else{stopAnimationTimer();renderSurface.reset();}if(wasAnimating)pointerMoved();}
}
void collapseExpired(){KillTimer(windowHandle,3);if(!visible)return;if(webView()){POINT p{};GetCursorPos(&p);const auto point=worldPoint(p);const auto frame=tree::animationFrame(scene,reveals,GetTickCount64());const auto now=GetTickCount64();std::vector<std::string> remove;for(const auto& pair:collapseAt)if(pair.second<=now&&!tree::inside(scene,point,32,pair.first)&&!tree::inside(scene,frame,point,32,pair.first))remove.push_back(pair.first);for(const auto& id:remove)collapseGroup(id);render();scheduleCollapse(point);return;}POINT p{};GetCursorPos(&p);const auto point=worldPoint(p);const auto now=GetTickCount64();const auto pending=collapseAt.find("parent-rings");if(pending!=collapseAt.end()&&pending->second<=now&&ringKeepAt(point)==pendingRingKeep&&pendingRingKeep<navigation.size()){navigation.resize(pendingRingKeep);expanded=std::set<std::string>(navigation.begin(),navigation.end());collapseAt.clear();updateEntries();render();return;}const auto outside=collapseAt.find("rings");if(outside!=collapseAt.end()&&outside->second<=now&&!insideRingMenu(point,16)){navigation.clear();expanded.clear();collapseAt.clear();updateEntries();render();}else scheduleCollapse(point);}
