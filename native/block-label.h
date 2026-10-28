#pragma once
struct BlockFontSpec {const wchar_t* family;INT style;};
const BlockFontSpec& blockFontSpec(){
    static const BlockFontSpec spec=[](){FontFamily family(L"Segoe UI Semibold");return family.GetLastStatus()==Ok&&family.IsStyleAvailable(FontStyleRegular)?BlockFontSpec{L"Segoe UI Semibold",FontStyleRegular}:BlockFontSpec{L"Segoe UI",FontStyleBold};}();
    return spec;
}
std::wstring missingProgramMessage(const std::wstring& name){return L"Программа\n«"+name+L"»\nне найдена :(";}
bool missingProgram(const Item& item){const auto found=iconCache.find(item.id);return !item.group&&found!=iconCache.end()&&found->second.source==item.path&&found->second.missing;}
std::map<std::tuple<std::wstring,int,int,int>,float> wrappedFontSizes;
struct ArcBlockLayout {float size;std::vector<std::wstring> lines;std::vector<std::vector<std::pair<std::wstring,float>>> glyphs;std::vector<float> totals;};
std::map<std::tuple<std::wstring,int,int,int,int>,ArcBlockLayout> arcBlockLayouts;
void clearWrappedLabels(){wrappedFontSizes.clear();arcBlockLayouts.clear();}
void drawWrappedLabel(Graphics& graphics,const std::wstring& value,RectF box,float maximum,Color color){
    if(box.Width<=0||box.Height<=0)return;
    StringFormat format;format.SetAlignment(StringAlignmentCenter);format.SetLineAlignment(StringAlignmentCenter);format.SetTrimming(StringTrimmingNone);
    const auto key=std::make_tuple(value,static_cast<int>(box.Width*10),static_cast<int>(box.Height*10),static_cast<int>(maximum*10));
    auto found=wrappedFontSizes.find(key);float size=maximum;
    if(found!=wrappedFontSizes.end())size=found->second;
    else {
        for(;size>.5f;size-=.25f){Font font(blockFontSpec().family,size,blockFontSpec().style,UnitPixel);RectF measured;graphics.MeasureString(value.c_str(),-1,&font,RectF(0,0,box.Width,100000),&format,&measured);if(measured.Height<=box.Height&&measured.Width<=box.Width+.1f)break;}
        if(wrappedFontSizes.size()>256)wrappedFontSizes.clear();wrappedFontSizes.emplace(key,size);
    }
    Font font(blockFontSpec().family,size,blockFontSpec().style,UnitPixel);SolidBrush ink(color);graphics.DrawString(value.c_str(),-1,&font,box,&format,&ink);
}

std::vector<std::wstring> wrapArcLines(Graphics& graphics,const std::wstring& value,float width,float size){
    Font font(blockFontSpec().family,size,blockFontSpec().style,UnitPixel);StringFormat format(StringFormat::GenericTypographic());format.SetFormatFlags(format.GetFormatFlags()|StringFormatFlagsMeasureTrailingSpaces);
    const auto measured=[&](const std::wstring& line){RectF box;graphics.MeasureString(line.c_str(),static_cast<INT>(line.size()),&font,PointF(0,0),&format,&box);return box.Width;};
    std::vector<std::wstring> lines;std::wstring line;
    for(size_t i=0;i<value.size();){
        if(value[i]==L'\n'){lines.push_back(line);line.clear();++i;continue;}
        const size_t begin=i++;if(value[begin]>=0xd800&&value[begin]<=0xdbff&&i<value.size()&&value[i]>=0xdc00&&value[i]<=0xdfff)++i;
        const auto glyph=value.substr(begin,i-begin);line+=glyph;
        if(line.size()>glyph.size()&&measured(line)>width){const auto space=line.find_last_of(L" \t");if(space!=std::wstring::npos&&space>0){lines.push_back(line.substr(0,space));line=line.substr(space+1);}else{lines.push_back(line.substr(0,line.size()-glyph.size()));line=glyph;}}
    }
    if(!line.empty())lines.push_back(line);return lines;
}
void drawRingBlockLabel(Graphics& graphics,const radial::Sector& sector,const Item& item,float alpha){
    const bool missing=missingProgram(item),lower=std::sin(sector.angle)>.1;const auto label=missing?missingProgramMessage(item.name):menuLabel(item.name,20);
    const float availableHeight=std::max(1.f,sector.radius-sector.inner-8);const double sweep=std::max(.02,std::min(sector.sweep-.09,PI*.95));
    const auto key=std::make_tuple(label,static_cast<int>(sector.inner*10),static_cast<int>(sector.radius*10),static_cast<int>(sweep*1000),static_cast<int>(sector.layerFactor*1000));
    if(arcBlockLayouts.size()>256)arcBlockLayouts.clear();
    auto [found,inserted]=arcBlockLayouts.try_emplace(key);auto& layout=found->second;
    if(inserted){
        layout.size=13*sector.layerFactor;
        for(;layout.size>.5;layout.size-=.25f){const float width=static_cast<float>((sector.inner+layout.size*.75f+4)*sweep);layout.lines=wrapArcLines(graphics,label,width,layout.size);if(layout.lines.size()*layout.size*1.15f<=availableHeight)break;}
        Font measuredFont(blockFontSpec().family,layout.size,blockFontSpec().style,UnitPixel);StringFormat measuredFormat(StringFormat::GenericTypographic());measuredFormat.SetFormatFlags(measuredFormat.GetFormatFlags()|StringFormatFlagsMeasureTrailingSpaces);
        for(const auto& line:layout.lines){std::vector<std::pair<std::wstring,float>> glyphs;float total=0;for(size_t i=0;i<line.size();){size_t begin=i++;if(line[begin]>=0xd800&&line[begin]<=0xdbff&&i<line.size()&&line[i]>=0xdc00&&line[i]<=0xdfff)++i;auto glyph=line.substr(begin,i-begin);RectF box;graphics.MeasureString(glyph.c_str(),static_cast<INT>(glyph.size()),&measuredFont,PointF(0,0),&measuredFormat,&box);glyphs.push_back({glyph,box.Width});total+=box.Width;}layout.glyphs.push_back(std::move(glyphs));layout.totals.push_back(total);}
    }
    const float size=layout.size;const auto& lines=layout.lines;
    const auto saved=graphics.Save();GraphicsPath shape;sectorShape(shape,sector);graphics.SetClip(&shape,CombineModeIntersect);
    Font font(blockFontSpec().family,size,blockFontSpec().style,UnitPixel);StringFormat format(StringFormat::GenericTypographic());format.SetFormatFlags(format.GetFormatFlags()|StringFormatFlagsMeasureTrailingSpaces);
    SolidBrush ink(Color(static_cast<BYTE>(255*alpha),missing?255:245,missing?201:247,missing?201:250));const float mid=(sector.inner+sector.radius)/2,sign=lower?-1.f:1.f;
    for(size_t row=0;row<lines.size();++row){
        const float radius=mid+(lower?1.f:-1.f)*(static_cast<float>(row)-(lines.size()-1)/2.f)*size*1.15f;
        float offset=-layout.totals[row]/2;for(const auto& [glyph,width]:layout.glyphs[row]){const double angle=sector.angle+sign*(offset+width/2)/radius;const auto state=graphics.Save();graphics.TranslateTransform(sector.center.x+static_cast<float>(std::cos(angle)*radius),sector.center.y+static_cast<float>(std::sin(angle)*radius));graphics.RotateTransform(static_cast<float>((angle+sign*PI/2)*180/PI));graphics.DrawString(glyph.c_str(),static_cast<INT>(glyph.size()),&font,PointF(-width/2,-size*.65f),&format,&ink);graphics.Restore(state);offset+=width;}
    }
    graphics.Restore(saved);
}
