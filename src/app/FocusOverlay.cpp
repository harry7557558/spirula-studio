#include "app/FocusOverlay.h"

#include "core/AtomicFile.h"
#include "data/JsonWrite.h"
#include "external/stb_image.h"
#include "i18n/catalog/Focus.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace app {

std::vector<uint8_t> read_focus_overlay_mask(const std::string& path, int width, int height) {
    int w = 0, h = 0, channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> mask(
        stbi_load(path.c_str(), &w, &h, &channels, 1), stbi_image_free);
    if (!mask) throw std::runtime_error("cannot read overlay mask: " + path);
    if (w != width || h != height) throw std::runtime_error("overlay mask and raw image sizes differ: " + path);
    return {mask.get(), mask.get() + (size_t)w * h};
}

void write_focus_overlay(const nn::Image& image, const std::vector<uint8_t>& weight,
                         const std::filesystem::path& output) {
    if (image.empty() || image.channels != 3 || weight.size() != (size_t)image.width * image.height)
        throw std::runtime_error("invalid focus overlay image or mask");
    const double scale = std::min(1.0, 2000.0 / std::max(image.width, image.height));
    const int width = std::max(1, (int)std::lround(image.width * scale));
    const int height = std::max(1, (int)std::lround(image.height * scale));
    nn::Image preview = nn::resize_image(image, width, height);
    nn::Image mask;
    mask.width = image.width;
    mask.height = image.height;
    mask.channels = 1;
    mask.data = weight;
    mask = nn::resize_image(mask, width, height);
    const float tint[] = {40.0f, 80.0f, 255.0f};
    for (size_t i = 0; i < preview.data.size(); ++i) {
        const float alpha = 0.65f * (1.0f - mask.data[i / 3] / 255.0f);
        preview.data[i] = (uint8_t)std::lround(preview.data[i] * (1.0f - alpha) + tint[i % 3] * alpha);
    }
    std::filesystem::create_directories(output.parent_path());
    auto temporary = output;
    temporary += ".part.jpg";
    if (!nn::save_image(preview, temporary.string(), 90)) throw std::runtime_error("cannot write focus overlay");
    spirula::replace_file(temporary, output);
}

void write_focus_overlay_gallery(const std::vector<FocusOverlayEntry>& entries,
                                 const std::filesystem::path& directory) {
    namespace F = spirula::i18n::msg::focus;
    JsonWriter writer;
    writer.object().field("title", F::overlay_title.get()).field("legend", F::overlay_legend.get())
        .key("images").array();
    for (const auto& entry : entries)
        writer.object().field("name", entry.image_relative).field("preview", entry.image_relative + ".jpg")
            .field("weight", entry.mean_weight).end();
    writer.end().end();
    std::string data;
    for (char c : writer.str()) {
        if (c == '<') data += "\\u003c";
        else data += c;
    }
    std::filesystem::create_directories(directory);
    const auto temporary = directory / "index.html.part";
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream << R"HTML(<!doctype html><meta charset="utf-8"><title></title>
<style>body{background:#121722;color:#e9efff;font:16px sans-serif;margin:24px}
header{position:sticky;top:0;background:#121722;padding:10px;z-index:1}
button,input{font:inherit;padding:8px;margin:4px;background:#26314a;color:white;border:1px solid #52617c}
main{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:14px}
figure{margin:0;background:#1d2638;padding:8px}img{width:100%;height:240px;object-fit:contain}
figcaption{font-size:13px;overflow-wrap:anywhere}</style>
<header><h2></h2><p></p><label>&#128270; <input type="search"></label>
<button id="prev">&#8592;</button><span id="count"></span><button id="next">&#8594;</button></header><main></main>
<script>const data=)HTML" << data << R"HTML(;
document.title=document.querySelector('h2').textContent=data.title;
document.querySelector('p').textContent=data.legend;
const input=document.querySelector('input'),main=document.querySelector('main');let page=0;
function render(){const list=data.images.filter(x=>x.name.toLowerCase().includes(input.value.toLowerCase()));
const pages=Math.max(1,Math.ceil(list.length/60));page=Math.max(0,Math.min(page,pages-1));main.replaceChildren();
for(const r of list.slice(page*60,(page+1)*60)){const f=document.createElement('figure'),a=document.createElement('a'),
i=document.createElement('img'),c=document.createElement('figcaption');a.href=r.preview.split('/').map(encodeURIComponent).join('/');
a.target='_blank';i.src=a.href;i.loading='lazy';i.alt=r.name;a.append(i);
c.textContent=r.name+' · '+new Intl.NumberFormat(undefined,{style:'percent',maximumFractionDigits:1}).format(r.weight);
f.append(a,c);main.append(f)}document.querySelector('#count').textContent=list.length+' · '+(page+1)+' / '+pages;
document.querySelector('#prev').disabled=page===0;document.querySelector('#next').disabled=page===pages-1}
input.oninput=()=>{page=0;render()};document.querySelector('#prev').onclick=()=>{page--;render()};
document.querySelector('#next').onclick=()=>{page++;render()};render();</script>)HTML";
    stream.close();
    if (!stream) throw std::runtime_error("cannot write focus overlay gallery");
    spirula::replace_file(temporary, directory / "index.html");
}

}  // namespace app
