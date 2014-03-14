/*
 * Created by "ngld" for the FreeSpace2 Source 
 * Code Project.
 *
 * You may not sell or otherwise commercially exploit the source or things you
 * create based on the source.
 */

#include <cstdlib>
#include <algorithm>
#include <typeinfo>
#include <stack>

#include "ai/ai_profiles.h"
#include "bmpman/bmpman.h"
#include "freespace2/freespace.h"
#include "gamesequence/gamesequence.h"
#include "graphics/2d.h"
#include "graphics/font.h"
#include "graphics/generic.h"
#include "io/key.h"
#include "io/mouse.h"
#include "io/timer.h"
#include "menuui/tblui.h"
#include "menuui/snazzyui.h"
#include "mission/missionparse.h"
#include "missionui/missionscreencommon.h"
#include "model/model.h"
#include "parse/parselo.h"

#define TBLUI_OPTION(opt, code) if (option == opt) { code; return true; } else
#define TBLUI_OPTION_END { return super::parse_option(option); }

TbluiElement *Tblui_cur_ui = NULL, *Tblui_active_el = NULL;
bool Tblui_mouse_down = false;
float Tblui_frametime = 0.0f;
int Tblui_mouse_px = 0, Tblui_mouse_py = 0;
SCP_map<SCP_string,TbluiElement> Tblui_uis;
SCP_map<SCP_string,SCP_string> Tblui_templates;

void tblui_parse_children(TbluiElement*);

// Reference to missionscreencommon.cpp
extern int anim_timer_start;

// Reference to parselo.cpp
extern char *Mp;

void draw_pline(vec3d *line, int line_len, int thickness)
{
    vec3d *pline[line_len];
    
    for (int idx = 0; idx < line_len; idx++) {
        pline[idx] = &line[idx];
    }
    
    gr_pline_special(pline, line_len, thickness);
}

void draw_aaline(int x1, int y1, int x2, int y2, int thickness)
{
    vertex v1, v2;
    
    for (int idx = 0; idx < thickness; idx++) {
        v1.screen.xyw.x = x1;
        v1.screen.xyw.y = y1;
        
        v2.screen.xyw.x = x2;
        v2.screen.xyw.y = y2;
        
        if (x2 > x1 && y2 > y1) {
            v1.screen.xyw.y += idx;
            v2.screen.xyw.x -= idx;
        } else if(x2 < x1 && y2 > y1) {
            v1.screen.xyw.x += idx;
            v2.screen.xyw.y += idx;
        } else if(x2 < x1 && y2 < y1) {
            v1.screen.xyw.y -= idx;
            v2.screen.xyw.x += idx;
        } else {
            v1.screen.xyw.x += idx;
            v2.screen.xyw.y += idx;
        }
        
        gr_aaline(&v1, &v2);
    }
}

void parse_color(color *clr)
{
    if (optional_string("#")) {
        // A hex string.
        
        SCP_string temp, part;
        int r, g, b;
        stuff_string(temp, F_RAW, " ");
        
        part = "0x" + temp.substr(0, 2);
        r = (int) strtod(part.c_str(), NULL);
        
        part = "0x" + temp.substr(2, 2);
        g = (int) strtod(part.c_str(), NULL);
        
        part = "0x" + temp.substr(4, 2);
        b = (int) strtod(part.c_str(), NULL);
        
        gr_init_color(clr, r, g, b);
    } else {
        int color[] = {0, 0, 0, 0};
        
        stuff_int_list(color, 4);
        gr_init_alphacolor(clr, color[0], color[1], color[2], color[3], AC_TYPE_BLEND);
    }
}

void parse_message(SCP_string& msg)
{
    if (optional_string("([")) {
        stuff_string_until(msg, "])");
        required_string("])");
    } else {
        stuff_string(msg, F_MESSAGE);
    }
}

/* Text renderer */
tblui_text parse_text(SCP_string& text, int font, color& font_color)
{
    tblui_text result;
    tblui_text_chunk temp;
    SCP_string tag;
    SCP_string::size_type next, pos = 0;
    
    std::stack<int> fonts;
    std::stack<color> font_colors;
    
    // Initialise the first chunk
    temp.font = font;
    temp.font_color = font_color;
    temp.w = 0;
    temp.h = 0;
    temp.lnbr = false;
    
    gr_set_font(temp.font);
    
    while (pos < text.size()) {
        while (text[pos] == '{') {
            // Found a tag
            
            next = text.find("}", pos);
            tag = text.substr(pos + 1, next - pos - 1);
            pos = next + 1;
            
            if (tag == "/font") {
                temp.font = fonts.top();
                fonts.pop();
                
                gr_set_font(temp.font);
                
            } else if (tag == "/color" || tag == "/red" || tag == "/green" || tag == "/blue") {
                temp.font_color = font_colors.top();
                font_colors.pop();
                
            } else if (tag == "red") {
                font_colors.push(temp.font_color);
                gr_init_color(&temp.font_color, 255, 0, 0);
                
            } else if (tag == "green") {
                font_colors.push(temp.font_color);
                gr_init_color(&temp.font_color, 0, 255, 0);
                
            } else if (tag == "blue") {
                font_colors.push(temp.font_color);
                gr_init_color(&temp.font_color, 0, 0, 255);
                
            } else if (tag.find("font ") == 0) {
                fonts.push(temp.font);
                temp.font = gr_get_fontnum((char*) tag.substr(5).c_str());
                
                gr_set_font(temp.font);
                
            } else if (tag.find("color ") == 0) {
                font_colors.push(temp.font_color);
                
                pause_parse();
                reset_parse((char*) tag.substr(6).c_str());
                parse_color(&temp.font_color);
                unpause_parse();
            } else {
                Warning(LOCATION, "Encountered unknown tag '%s' in some text!", tag.c_str());
            }
        }
        
        next = text.find_first_of(" {\n", pos + 1);
        temp.content = text.substr(pos, next - pos);
        pos = next;
        
        // Try to add all following whitespace to this chunk.
        // We don't want any chunks to start with a space because during
        // rendering the wrapping will be done per chunk and if the chunk
        // starts with a space, so would the new line.
        while (text[pos] == ' ') {
            temp.content += ' ';
            pos++;
        }
        
        while (temp.content[0] == '\n') {
            tblui_text_chunk lnbr;
            lnbr.lnbr = true;
            lnbr.h = temp.h;
            lnbr.w = 0;
            result.push_back(lnbr);
            
            temp.content = temp.content.substr(1);
        }
        
        // Fill in the size and push it off on the stack.
        gr_get_string_size(&temp.w, &temp.h, temp.content.c_str(), temp.content.size());
        result.push_back(temp);
    }
    
    return result;
}


void render_text(int x, int y, int width, int height, tblui_text& text, int skip_lines = 0)
{
    int lx = 0, ly = 0, lh = 0;
    tblui_text::iterator it;
    
    for (it = text.begin(); it != text.end(); it++) {
        if ((width != 0 && height != 0) && ((lx + it->w) > width || it->lnbr)) {
            if (it->w > width) {
                Warning(LOCATION, "Skipping text chunk because it didn't fit on the line. (%s)", it->content.c_str());
                continue;
            }
            
            // Start a new line.
            ly += lh;
            if (ly + lh > height && skip_lines < 1) break;
            
            lx = 0;
            lh = 0;
            
            if (skip_lines > 1) {
                skip_lines--;
            } else if (skip_lines == 1) {
                ly = 0;
                skip_lines--;
            }
            
            if (it->lnbr) continue;
        }
        
        if (skip_lines < 1) {
            gr_set_font(it->font);
            gr_set_color_fast(&it->font_color);
            gr_string(x + lx, y + ly, it->content.c_str());
        }
        
        lx += it->w;
        lh = std::max(lh, it->h);
    }
}

void get_text_size(int *w, int *h, tblui_text& text, int forced_width = -1)
{
    *w = 0;
    *h = 0;
    
    tblui_text::iterator it;
    
    int x = 0, lh = 0;
    
    for (it = text.begin(); it != text.end(); it++) {
        if (it->lnbr || (forced_width > 0 && (x + it->w) > forced_width)) {
            *h += lh;
            
            x = 0;
            lh = 0;
        }
        
        x += it->w;
        lh = std::max(lh, it->h);
        *w = std::max(*w, x);
    }
    
    *h += lh;
}

TbluiElement::TbluiElement() {
    x = 0;
    y = 0;
    
    ox = 0;
    oy = 0;
    
    calc_w = 0;
    calc_h = 0;
    
    parent = NULL;
    
    p_top = 0;
    p_right = 0;
    p_bottom = 0;
    p_left = 0;
    
    top = -1;
    right = -1;
    bottom = -1;
    left = -1;
    
    width = 0;
    height = 0;
    
    padding = 0;
    center_x = false;
    center_y = false;
    
    auto_w = false;
    auto_h = false;
    
    dragable = false;
    
    click_handler = NULL;
    hover_handler = NULL;
    drag_handler = NULL;
}

TbluiElement::~TbluiElement()
{
    //this->unload();
    
    for (int idx = 0; idx < (int) children.size(); idx++) {
        delete children.at(idx);
    }
    
    children.clear();
}

void TbluiElement::set_click_handler(void (*handler)(bool, void*), void *params)
{
    this->click_handler = handler;
    this->click_params = params;
}

void TbluiElement::set_hover_handler(void (*handler)(bool, void*), void *params)
{
    this->hover_handler = handler;
    this->hover_params = params;
}

void TbluiElement::set_drag_handler(void (*handler)(int, int, void*), void *params)
{
    this->drag_handler = handler;
    this->drag_params = params;
}

void TbluiElement::add_child(TbluiElement *child)
{
    children.push_back(child);
    child->parent = this;
}

TbluiElement* TbluiElement::get_root()
{
    if (!parent) {
        return this;
    }
    
    TbluiElement *el = parent;
    while (el->parent) {
        el = el->parent;
    }
    
    return el;
}

bool TbluiElement::parse_option(SCP_string& option)
{
    TBLUI_OPTION("Top",
        stuff_int(&top);
        if (optional_string("%")) {
            p_top = top;
        }
    )
    TBLUI_OPTION("Right",
        stuff_int(&right);
        if (optional_string("%")) {
            p_right = right;
        }
    )
    TBLUI_OPTION("Bottom",
        stuff_int(&bottom);
        if (optional_string("%")) {
            p_bottom = bottom;
        }
    )
    TBLUI_OPTION("Left",
        stuff_int(&left);
        if (optional_string("%")) {
            p_left = left;
        }
    )
    TBLUI_OPTION("Width",
        stuff_int(&width);
    )
    TBLUI_OPTION("Height",
        stuff_int(&height);
    )
    TBLUI_OPTION("Padding",
        stuff_int(&padding);
    )
    TBLUI_OPTION("CenterX",
        center_x = true;
    )
    TBLUI_OPTION("CenterY",
        center_y = true;
    )
    TBLUI_OPTION("Drag",
        dragable = true;
    )
    {
        return false;
    }
}

void TbluiElement::reposition()
{
    // Figure if we have to calculate the width and/or the height.
    if (!auto_w) auto_w = width == 0 && (left == -1 || right == -1);
    if (!auto_h) auto_h = height == 0 && (top == -1 || bottom == -1);
    
    if (parent == NULL) {
        // The top element!
        
        x = y = 0;
        calc_w = gr_screen.max_w;
        calc_h = gr_screen.max_h;
    } else {
        x = parent->x + parent->padding;
        y = parent->y + parent->padding;
        
        calc_w = parent->calc_w - parent->padding * 2;
        calc_h = parent->calc_h - parent->padding * 2;
    }
    
    // Percentages
    if (p_top > 0) {
        top = p_top * calc_h / 100;
    }
    
    if (p_right > 0) {
        right = p_right * calc_w / 100;
    }
    
    if (p_bottom > 0) {
        bottom = p_bottom * calc_h / 100;
    }
    
    if (p_left > 0) {
        left = p_left * calc_w / 100;
    }
    
    if (auto_w || auto_h) {
        // Now reposition all children.
        this->reposition_children();
        
        // Try to determine our size.
        if (auto_w) width = 1;
        if (auto_h) height = 1;
        
        for (int idx = 0; idx < (int) children.size(); idx++) {
            if (auto_w) width = std::max(width, children.at(idx)->x - x + children.at(idx)->calc_w);
            if (auto_h) height = std::max(height, children.at(idx)->y - y + children.at(idx)->calc_h);
        }
        
        if (auto_w) width += padding;
        if (auto_h) height += padding;
    }
    
    if (width > 0) {
        // This element has a fixed width.
        
        if ((left > -1 && right > -1)) {
            Warning(LOCATION, "A TbluiElement with a fixed width has left AND right coordinates! (%d, %d, %d, %d)", top, right, bottom, left);
        }
        
        if (center_x) {
            x = (calc_w - width) / 2;
        } else {
            if (left == -1) {
                x += calc_w - right - width;
            } else {
                x += left;
            }
        }
        
        calc_w = width;
    } else {
        x += left;
        calc_w -= left + right;
    }
    
    if (height > 0) {
        // This element has a fixed height.
        
        if ((top > -1 && bottom > -1)) {
            Warning(LOCATION, "A TbluiElement with a fixed height has top AND bottom coordinates! (%d, %d, %d, %d)", top, right, bottom, left);
        }
        
        if (center_y) {
            y = (calc_h - height) / 2;
        } else {
            if (top == -1) {
                y += calc_h - bottom - height;
            } else {
                y += top;
            }
        }
        
        calc_h = height;
    } else {
        y += top;
        calc_h -= top + bottom;
    }
    
    this->reposition_children();
}

void TbluiElement::reposition_children()
{
    // Now reposition all children.
    for (int idx = 0; idx < (int) children.size(); idx++) {
        children.at(idx)->reposition();
    }
}

void TbluiElement::load()
{
    for (int idx = 0; idx < (int) children.size(); idx++) {
        children.at(idx)->load();
    }
}

void TbluiElement::unload()
{
    for (int idx = 0; idx < (int) children.size(); idx++) {
        children.at(idx)->unload();
    }
}

TbluiElement* TbluiElement::find_element(int px, int py)
{
    if (px < x || py < y || px > (x + calc_w) || py > (y + calc_h)) {
        return NULL;
    }
    
    TbluiElement *child = NULL;
    SCP_vector<TbluiElement*>::iterator it;
    
    for (it = children.begin(); it != children.end(); it++) {
        child = (*it)->find_element(px, py);
        if (child != NULL) {
            return child;
        }
    }
    
    return this;
}

void TbluiElement::render()
{
    // Render the children...
    
    for (int idx = 0; idx < (int) children.size(); idx++) {
        children.at(idx)->render();
    }
}

void TbluiElement::click(bool down)
{
    if (down && dragable) {
        ox = x;
        oy = y;
    }
    
    if (click_handler != NULL) {
        click_handler(down, click_params);
    }
}

void TbluiElement::hover(bool mouse_over)
{
    if (hover_handler != NULL) {
        hover_handler(mouse_over, hover_params);
    }
}

void TbluiElement::drag(int dx, int dy)
{
    if (dragable) {
        x = ox + dx;
        y = oy + dy;
    }
    
    if (drag_handler != NULL) {
        drag_handler(dx, dy, drag_params);
    }
}


/**
 * A simple box.
 */
TbluiBox::TbluiBox()
{
    border_size = 0;
    border_radius = 0;
    
    gr_init_alphacolor(&border_color, 0, 0, 0, 0, AC_TYPE_BLEND);
    gr_init_alphacolor(&bg_color, 0, 0, 0, 0, AC_TYPE_BLEND);
}

bool TbluiBox::parse_option(SCP_string& option)
{
    TBLUI_OPTION("Border",
        stuff_int(&border_size);
        parse_color(&border_color);
        padding = border_size;
    )
    TBLUI_OPTION("BorderRadius",
        stuff_int(&border_radius);
    )
    TBLUI_OPTION("Background",
        parse_color(&bg_color);
    )
    TBLUI_OPTION_END
}

void TbluiBox::render()
{
    if (border_size > 0) {
        // Render the border first.
        gr_set_color_fast(&border_color);
        
        if (bg_color.alpha < 255 || border_radius > 0) {
            // The background is transparent the border would shine through so we have to draw sperate lines
            // or we want nice round corners.
            
            if (border_radius > 0) {
                // top right
                draw_aaline(x + calc_w - border_radius, y, x + calc_w, y + border_radius, border_size);
                
                // lower right
                draw_aaline(x + calc_w - border_size, y + calc_h - border_radius, x + calc_w - border_radius, y + calc_h - border_size, border_size);
                
                // lower left
                draw_aaline(x + border_radius, y + calc_h, x, y + calc_h - border_radius, border_size);
                
                // upper left
                draw_aaline(x, y + border_radius, x + border_radius, y, border_size);
            }
            
            // top
            gr_rect(x + border_radius, y, calc_w - border_radius * 2, border_size);
            
            // right
            gr_rect(x + calc_w - border_size, y + border_radius, border_size, calc_h - border_radius * 2);
            
            // bottom
            gr_rect(x + border_radius, y + calc_h - border_size, calc_w - border_radius * 2, border_size);
            
            // left
            gr_rect(x, y + border_radius, border_size, calc_h - border_radius * 2);
        } else {
            gr_rect(x, y, calc_w, calc_h);
        }
    }
    
    // Now render the background.
    gr_set_color_fast(&bg_color);
    gr_rect(x + border_size, y + border_size, calc_w - (border_size * 2), calc_h - (border_size * 2));
    
    // And the children...
    super::render();
}

/**
 * A text display.
 */
TbluiText::TbluiText()
{
    this->set_font("font01.vf");
    gr_init_color(&text_color, 255, 255, 255);
}

bool TbluiText::parse_option(SCP_string& option)
{
    SCP_string temp;
    
    TBLUI_OPTION("Color",
        parse_color(&text_color);
    )
    TBLUI_OPTION("Font",
        stuff_string(temp, F_FILESPEC);
        this->set_font(temp);
    )
    
    TBLUI_OPTION("Text",
        parse_message(raw_text);
    )
    TBLUI_OPTION_END
}

void TbluiText::set_font(SCP_string filename)
{
    int my_font = gr_get_fontnum((char*) filename.c_str());
    
    if (my_font < 0) {
        Warning(LOCATION, "Could not load font '%s'!\n", filename.c_str());
        return;
    }
    
    text_font = my_font;
}

void TbluiText::reposition()
{
    if (width == 0 || height == 0) {
        get_text_size(&width, &height, text);
    }
    
    super::reposition();
}

void TbluiText::load()
{
    text = parse_text(raw_text, text_font, text_color);
    super::load();
}

void TbluiText::unload()
{
    super::unload();
    text.clear();
}

void TbluiText::render()
{
    render_text(x, y, calc_w, calc_h, text);
    
    super::render();
}


/**
 * A simple image.
 */
bool TbluiImage::parse_option(SCP_string& option)
{
    TBLUI_OPTION("Image",
        stuff_string(image_file, F_FILESPEC);
    )
    TBLUI_OPTION("Stretch",
        stretch = true;
    )
    TBLUI_OPTION_END
}

void TbluiImage::reposition()
{
    if (!stretch) {
        bm_get_info(image, &width, &height);
    }
    
    super::reposition();
}

void TbluiImage::load()
{
    int bmp = bm_load(image_file);
    
    if (bmp < 0) {
        Warning(LOCATION, "Failed to load image '%s'!", image_file.c_str());
        return;
    }
    
    image = bmp;
    super::load();
}

void TbluiImage::unload()
{
    super::unload();
    if (image > 0) bm_release(image);
}

void TbluiImage::render()
{
    gr_set_bitmap(image);
    
    if (stretch) {
        gr_bitmap_uv(x, y, calc_w, calc_h, 0.0f, 0.0f, 1.0f, 1.0f);
    } else {
        gr_bitmap(x, y);
    }
    
    super::render();
}

/**
 * A generic animation.
 */
bool TbluiAnim::parse_option(SCP_string& option)
{
    TBLUI_OPTION("Anim",
        stuff_string(anim_file, F_FILESPEC);
    )
    TBLUI_OPTION("Noloop",
        noloop = true;
    )
    TBLUI_OPTION_END
}

void TbluiAnim::reposition()
{
    if(anim.streaming) {
        width = anim.width;
        height = anim.height;
    } else {
        bm_get_info(anim.first_frame, &width, &height);
    }
    
    super::reposition();
}

void TbluiAnim::load()
{
    generic_anim_init(&anim, anim_file);
    generic_anim_stream(&anim);
    
    super::load();
}

void TbluiAnim::unload()
{
    super::unload();
    generic_anim_unload(&anim);
}

void TbluiAnim::render()
{
    generic_anim_render(&anim, Tblui_frametime, x, y);
    
    super::render();
}


/**
 * A simple button.
 */
bool TbluiButton::parse_option(SCP_string& option)
{
    TBLUI_OPTION("HoverImage",
        stuff_string(hover_file, F_FILESPEC);
    )
    TBLUI_OPTION("ClickImage",
        stuff_string(click_file, F_FILESPEC);
    )
    TBLUI_OPTION_END
}

void TbluiButton::load()
{
    if (!hover_file.empty()) hover_img = bm_load(hover_file);
    if (!click_file.empty()) click_img = bm_load(click_file);
    
    super::load();
}

void TbluiButton::unload()
{
    super::unload();
    
    if (hover_img > 0) bm_release(hover_img);
    if (click_img > 0) bm_release(click_img);
}

void TbluiButton::hover(bool mouse_over)
{
    if (!disabled) {
        state = (mouse_over ? TBLUI_BTN_HOVER : TBLUI_BTN_NORMAL);
        super::hover(mouse_over);
    }
}

void TbluiButton::click(bool down)
{
    if (!disabled) {
        state = (down ? TBLUI_BTN_DOWN : TBLUI_BTN_HOVER);
        super::click(down);
    }
}

void TbluiButton::render()
{
    int img = image;
    
    switch (state) {
        case TBLUI_BTN_NORMAL:
            break;
        case TBLUI_BTN_HOVER:
            if (hover_img > 0) image = hover_img;
            break;
        case TBLUI_BTN_DOWN:
            if (click_img > 0) image = click_img;
            break;
    }
    
    super::render();
    image = img;
}

/**
 * A simple slider.
 */
void tbluislider_up_handler(bool down, void *p)
{
    TbluiSlider *slider = (TbluiSlider*) p;
    
    if (down && slider->spos > 0) {
        slider->spos--;
    }
}

void tbluislider_down_handler(bool down, void *p)
{
    TbluiSlider *slider = (TbluiSlider*) p;
    
    if (down && slider->spos < slider->smax) {
        slider->spos++;
    }
}

void tbluislider_circle_click_handler(bool down, void *p)
{
    TbluiSlider *slider = (TbluiSlider*) p;
    
    if (down) {
        slider->spos_save = slider->spos;
    }
}

void tbluislider_circle_drag_handler(int dx, int dy, void *p)
{
    TbluiSlider *slider = (TbluiSlider*) p;
    
    if (slider->smax == 0) {
        slider->spos = 0;
    } else if (slider->horizontal) {
        slider->spos = slider->spos_save + (dx * slider->smax / slider->area);
    } else {
        slider->spos = slider->spos_save + (dy * slider->smax / slider->area);
    }
    
    // Sanity
    slider->spos = std::max(0, std::min(slider->spos, slider->smax));
}

TbluiSlider::TbluiSlider()
{
    spos = 0;
    smax = 1;
    
    horizontal = false;
    
    up = new TbluiButton;
    down = new TbluiButton;
    circle = new TbluiButton;
    
    up->top = 0;
    up->left = 0;
    
    down->bottom = 0;
    down->left = 0;
    
    circle->top = 0;
    circle->left = 0;
    
    up->image_file = "2_BAB_100001";
    up->hover_file = "2_BAB_100002";
    
    down->image_file = "2_BAB_090001";
    down->hover_file = "2_BAB_090002";
    
    circle->image_file = "2_HCB_140001";
    
    up->set_click_handler(tbluislider_up_handler, (void*)this);
    down->set_click_handler(tbluislider_down_handler, (void*)this);
    circle->set_click_handler(tbluislider_circle_click_handler, (void*)this);
    circle->set_drag_handler(tbluislider_circle_drag_handler, (void*)this);
    
    this->add_child(up);
    this->add_child(down);
    this->add_child(circle);
}

bool TbluiSlider::parse_option(SCP_string& option)
{
    SCP_string temp;
    
    TBLUI_OPTION("Max",
        stuff_int(&smax);
        this->set_max(smax);
    )
    TBLUI_OPTION("Horizontal",
        horizontal = true;
    )
    TBLUI_OPTION("Image",
        stuff_string(circle->image_file, F_FILESPEC);
    )
    TBLUI_OPTION("ImageHover",
        stuff_string(circle->hover_file, F_FILESPEC);
    )
    TBLUI_OPTION("ImageClick",
        stuff_string(circle->click_file, F_FILESPEC);
    )
    TBLUI_OPTION("UpImage",
        stuff_string(up->image_file, F_FILESPEC);
    )
    TBLUI_OPTION("UpImageHover",
        stuff_string(up->hover_file, F_FILESPEC);
    )
    TBLUI_OPTION("UpImageClick",
        stuff_string(up->click_file, F_FILESPEC);
    )
    TBLUI_OPTION("DownImage",
        stuff_string(down->image_file, F_FILESPEC);
    )
    TBLUI_OPTION("DownImageHover",
        stuff_string(down->hover_file, F_FILESPEC);
    )
    TBLUI_OPTION("DownImageClick",
        stuff_string(down->click_file, F_FILESPEC);
    )
    TBLUI_OPTION_END
}

void TbluiSlider::set_max(int max)
{
    smax = max;
    spos = std::min(spos, smax);
}

void TbluiSlider::reposition()
{
    this->reposition_children();
    
    if (horizontal) {
        height = std::max(up->height, std::max(down->height, circle->height));
    } else {
        width = std::max(up->width, std::max(down->width, circle->width));
    }
    
    super::reposition();
    
    if (horizontal) {
        area = calc_w - up->calc_w - circle->calc_w - down->calc_w;
    } else {
        area = calc_h - up->calc_h - circle->calc_h - down->calc_h;
    }
}

void TbluiSlider::render()
{
    int slider_pos;
    
    if (smax == 0) {
        if (horizontal) {
            circle->x = x + up->calc_w;
        } else {
            circle->y = y + up->calc_h;
        }
    } else if (horizontal) {
        slider_pos = area * spos / smax;
        circle->x = x + up->calc_w + slider_pos;
    } else {
        slider_pos = area * spos / smax;
        circle->y = y + up->calc_h + slider_pos;
    }
    
    super::render();
}

/**
 * A scrolling text box.
 */
TbluiTextBox::TbluiTextBox()
{
    display_lines = 0;
    
    slider = new TbluiSlider;
    
    slider->top = 0;
    slider->right = 0;
    slider->bottom = 0;
    
    this->add_child(slider);
}

bool TbluiTextBox::parse_option(SCP_string& option)
{
    if (super::parse_option(option)) {
        return true;
    } else {
        return slider->parse_option(option);
    }
}

void TbluiTextBox::reposition()
{
    int w, h, fw, fh;
    
    TbluiElement::reposition();
    get_text_size(&w, &h, text, calc_w - slider->calc_w - 10);
    
    if (h > calc_h) {
        gr_set_font(text_font);
        gr_get_string_size(&fw, &fh, "g'A", 3);
        
        display_lines = calc_h / fh;
        slider->set_max((h / fh) - display_lines + 1);
    } else {
        display_lines = 1;
        slider->set_max(0);
    }
}

void TbluiTextBox::render()
{
    render_text(x, y, calc_w - slider->calc_w - 10, calc_h, text, slider->spos);
    
    TbluiElement::render();
}

/**
 * A model viewer.
 */
TbluiModelViewer::TbluiModelViewer()
{
    model = -1;
    rotation = 0;
    should_rotate = false;
}

bool TbluiModelViewer::parse_option(SCP_string& option)
{
    TBLUI_OPTION("Model",
        stuff_string(model_file, F_FILESPEC);
    )
    TBLUI_OPTION("Rotate",
        should_rotate = true;
    )
    TBLUI_OPTION_END
}

void TbluiModelViewer::reposition()
{
    super::reposition();
    
    mprintf(("ModelViewer at (%d, %d) %d x %d.\n", x, y, calc_w, calc_h));
}

void TbluiModelViewer::load()
{
    // The model renderer needs an AI profile....
    if (!The_mission.ai_profile) {
        The_mission.ai_profile = &Ai_profiles[Default_ai_profile];
    }
    
    model = model_load((char*) model_file.c_str(), 0, NULL, 0);
    model_page_in_textures(model);
    
    anim_timer_start = timer_get_milliseconds();
    
    super::load();
}

void TbluiModelViewer::unload()
{
    super::unload();
    
    if (model > -1) model_unload(model);
}

void TbluiModelViewer::render()
{
    if (should_rotate) {
        draw_model_rotating(model, x, y, calc_w, calc_h, &rotation);
    } else {
        draw_model_icon(model, MR_LOCK_DETAIL | MR_AUTOCENTER, 1.0f, x, y, calc_w, calc_h);
    }
    
    
    super::render();
}


// Helpers

void tblui_open(SCP_string name)
{
    if (!Tblui_uis.count(name)) {
        SCP_map<SCP_string,TbluiElement>::iterator it;
        SCP_string list;
        
        for (it = Tblui_uis.begin(); it != Tblui_uis.end(); it++) {
            list += ", " + it->first;
        }
        
        Error(LOCATION, "The table interface '%s' wasn't found! Loaded: %s.\n", name.c_str(), list.substr(2).c_str());
    }
    
    Tblui_cur_ui = &Tblui_uis[name];
    
    gameseq_post_event(GS_EVENT_TBLUI);
}

void tblui_close()
{
    Tblui_cur_ui = NULL;
    Tblui_active_el = NULL;
    Tblui_uis.clear();
}

// state stuff

void tblui_state_init()
{
    if (Tblui_cur_ui == NULL) {
        Warning(LOCATION, "GS_STATE_TBLUI initated but no interface was selected!\n");
        
        // Try default. (Will most likely fail...)
        Tblui_cur_ui = &Tblui_uis["default"];
    }
    
    Tblui_cur_ui->load();
    Tblui_cur_ui->reposition();
    
    // Disable screen stretching.
    gr_reset_screen_scale();
}

void tblui_do_frame(float frametime)
{
    if (!Tblui_cur_ui) {
        Error(LOCATION, "GS_STATE_TBLUI failed because we have no interface!\n");
    }
    
    int mx, my;
    mouse_get_pos(&mx, &my);
    
    Tblui_frametime = frametime;
    
    if (mouse_down(MOUSE_LEFT_BUTTON) && Tblui_active_el != NULL) {
        if (!Tblui_mouse_down) {
            Tblui_mouse_down = true;
            Tblui_mouse_px = mx;
            Tblui_mouse_py = my;
            
            Tblui_active_el->click(true);
        } else {
            Tblui_active_el->drag(mx - Tblui_mouse_px, my - Tblui_mouse_py);
        }
    } else if(Tblui_mouse_down) {
        Tblui_mouse_down = false;
        if (Tblui_active_el != NULL) {
            Tblui_active_el->click(false);
        }
    } else {
        TbluiElement *active_el = Tblui_cur_ui->find_element(mx, my);
        if (active_el != Tblui_active_el) {
            if (Tblui_active_el != NULL) {
                Tblui_active_el->hover(false);
            }
            
            Tblui_active_el = active_el;
            Tblui_active_el->hover(true);
        }
    }
    
    gr_reset_clip();
    gr_clear();
    
    Tblui_cur_ui->render();
    
    gr_flip();
    
    int key = game_check_key();
    if (key == KEY_ESC) {
        gameseq_post_event(GS_EVENT_MAIN_MENU);
    }
}

void tblui_state_close()
{
    Tblui_cur_ui->unload();
    Tblui_cur_ui = NULL;
    
    // Re-enable screen stretching.
    gr_set_screen_scale(1024, 768);
}

// parse stuff
void tblui_parse_templates()
{
    SCP_string temp;
    
    while (optional_string("#Template")) {
        stuff_string(temp, F_RAW);
        
        stuff_string_until(Tblui_templates[temp], "#End Template");
        required_string("#End Template");
    }
}

// Well... this is kind of hacky but it works!
void tblui_compile_template(SCP_string& name, TbluiElement *parent)
{
    SCP_string tpl = Tblui_templates[name];
    SCP_string options, var, value;
    SCP_string::size_type pos;
    
    while (optional_string("-")) {
        stuff_string(var, F_RAW, ":");
        
        if (optional_string(":")) {
            // This template var has a value. Replace all ###VAR### markers with the value.
            
            if (optional_string("[")) {
                stuff_string_until(value, "];");
                required_string("];");
            } else {
                stuff_string(value, F_RAW);
            }
            
            var = "###" + var + "###";
            while ((pos = tpl.find(var)) != SCP_string::npos) {
                //tpl = tpl.substr(0, pos) + value + tpl.substr(pos + var.size());
                tpl.replace(pos, var.size(), value);
            }
        } else {
            // This variable has no value. Just remove all ###VAR### markers
            
            var = "###" + var + "###";
            while ((pos = tpl.find(var)) != SCP_string::npos) {
                //tpl = tpl.substr(0, pos) + tpl.substr(pos + var.size());
                tpl.replace(pos, var.size(), "");
            }
        }
    }
    
    while (optional_string("+")) {
        stuff_string(var, F_RAW);
        
        if (var.find("([") != SCP_string::npos && var.find("])") == SCP_string::npos) {
            stuff_string_until(value, "])");
            required_string("])");
            
            var += value + "])";
        }
        
        options += "+" + var + "\n";
    }
    
    while ((pos = tpl.find("###OPTS###")) != SCP_string::npos) {
        tpl.replace(pos, 10, options);
    }
    
    // Remove all lines left with ###VAR### markers in them.
    while ((pos = tpl.find("###")) != SCP_string::npos) {
        pos = tpl.rfind("\n", pos);
        tpl.replace(pos, tpl.find("\n", pos + 1) - pos, "");
    }
    
    // NOTE: We can't pause_parse() here since parse_text() might pause again which causes an assert...
    // pause_parse();
    // reset_parse((char*) tpl.c_str());
    char *backup = Mp;
    Mp = (char*) tpl.c_str();
    
    tblui_parse_children(parent);
    
    Mp = backup;
}

void tblui_parse_children(TbluiElement *parent)
{
    TbluiElement *child;
    SCP_string option;
    
    required_string("[");
    while (!check_for_string("]")) {
        if (optional_string("%"))  {
            stuff_string(option, F_RAW);
            
            if (!Tblui_templates.count(option)) {
                Error(LOCATION, "Couldn't found template '%s'!", option.c_str());
            }
            
            tblui_compile_template(option, parent);
            continue;
        } else if (optional_string("$Box")) {
            child = new TbluiBox;
        } else if (optional_string("$TextBox")) {
            child = new TbluiTextBox;
        } else if (optional_string("$Text")) {
            child = new TbluiText;
        } else if (optional_string("$Image:")) {
            child = new TbluiImage;
            
            stuff_string(((TbluiImage*) child)->image_file, F_FILESPEC);
        } else if (optional_string("$Anim:")) {
            child = new TbluiAnim;
            
            stuff_string(((TbluiAnim*) child)->anim_file, F_FILESPEC);
        } else if (optional_string("$Button")) {
            child = new TbluiButton;
        } else if (optional_string("$Slider")) {
            child = new TbluiSlider;
        } else if (optional_string("$ModelViewer")) {
            child = new TbluiModelViewer;
        } else {
            stuff_string(option, F_RAW);
            Error(LOCATION, "Expected tblui element! Found [%s]!", option.c_str());
        }
        
        while (optional_string("+")) {
            stuff_string(option, F_RAW, ":");
            optional_string(":");
            
            if (!child->parse_option(option)) {
                // Skip this line.
                advance_to_eoln(NULL);
                Warning(LOCATION, "Unknown tblui option '%s'!", option.c_str());
            }
        }
        
        if (check_for_string("[")) {
            tblui_parse_children(child);
        }
        
        parent->add_child(child);
    }
    
    required_string("]");
}

void tblui_parse_table(SCP_string filename)
{
    SCP_string temp;
    
    read_file_text(filename.c_str(), CF_TYPE_TABLES);
    reset_parse();
    
    tblui_parse_templates();
    
    required_string("#Interface Start");
    while (optional_string("$Name:")) {
        stuff_string(temp, F_NAME);
        
        if (Tblui_uis.count(temp)) {
            mprintf(("TBLUI => Duplicate interface '%s'! Aborting.\n", temp.c_str()));
            
            stop_parse();
            return;
        }
        
        mprintf(("TBLUI => Parsing interface '%s'.\n", temp.c_str()));
        
        Tblui_uis[temp].top = 0;
        Tblui_uis[temp].right = 0;
        Tblui_uis[temp].bottom = 0;
        Tblui_uis[temp].left = 0;
        
        tblui_parse_children(&Tblui_uis[temp]);
    }
    
    required_string("#Interface End");
    stop_parse();
}
