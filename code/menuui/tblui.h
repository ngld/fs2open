/*
 * Created by "ngld" for the FreeSpace2 Source 
 * Code Project.
 *
 * You may not sell or otherwise commercially exploit the source or things you
 * create based on the source.
 */

#ifndef _TBLUI_H
#define _TBLUI_H

#include <cstdlib>
#include "globalincs/globals.h"
#include "globalincs/pstypes.h"
#include "graphics/2d.h"
#include "graphics/font.h"
#include "graphics/generic.h"

#define TBLUI_BTN_NORMAL 0
#define TBLUI_BTN_HOVER  1
#define TBLUI_BTN_DOWN   2

struct tblui_text_chunk
{
    SCP_string content;
    int font;
    color font_color;
    
    int w, h;
    bool lnbr;
};
typedef SCP_vector<tblui_text_chunk> tblui_text;

class TbluiElement
{
protected:
    TbluiElement *parent;
    SCP_vector<TbluiElement*> children;

public:
    int x, y, ox, oy;
    int calc_w, calc_h;
    
    int p_top, p_right, p_bottom, p_left;
    bool auto_w, auto_h;
    
    int top;
    int right;
    int bottom;
    int left;
    
    int width;
    int height;
    
    int padding;
    bool center_x, center_y;
    
    bool dragable;
    
    void (*click_handler)(bool, void*);
    void (*hover_handler)(bool, void*);
    void (*drag_handler)(int, int, void*);
    
    void *click_params;
    void *hover_params;
    void *drag_params;
    
    void set_click_handler(void (*handler)(bool, void*), void *params);
    void set_hover_handler(void (*handler)(bool, void*), void *params);
    void set_drag_handler(void (*handler)(int, int, void*), void *params);
    
    TbluiElement();
    virtual ~TbluiElement();
    
    virtual void add_child(TbluiElement *child);
    virtual TbluiElement* get_root();
    virtual bool parse_option(SCP_string&);
    
    virtual void reposition();
    virtual void reposition_children();
    virtual void load();
    virtual void unload();
    virtual TbluiElement* find_element(int x, int y);
    virtual void render();
    virtual void click(bool);
    virtual void hover(bool);
    virtual void drag(int, int);
};

class TbluiBox : public TbluiElement
{
private:
    typedef TbluiElement super;
    
public:
    TbluiBox();
    
    color border_color;
    color bg_color;
    int border_size;
    int border_radius;
    
    virtual bool parse_option(SCP_string&);
    virtual void render();
};

class TbluiText : public TbluiElement
{
private:
    typedef TbluiElement super;
    
public:
    color text_color;
    int text_font;
    SCP_string raw_text;
    tblui_text text;
    
    TbluiText();
    
    virtual bool parse_option(SCP_string&);
    void set_font(SCP_string filename);
    
    virtual void reposition();
    virtual void load();
    virtual void unload();
    virtual void render();
};

class TbluiImage : public TbluiElement
{
private:
    typedef TbluiElement super;
    
public:
    SCP_string image_file;
    int image;
    bool stretch;
    
    TbluiImage() : image(0), stretch(false) {}
    virtual bool parse_option(SCP_string&);
    
    virtual void reposition();
    virtual void load();
    virtual void unload();
    virtual void render();
};

class TbluiAnim : public TbluiElement
{
private:
    typedef TbluiElement super;

public:
    SCP_string anim_file;
    generic_anim anim;
    bool noloop;
    
    TbluiAnim() : noloop(false) {}
    virtual bool parse_option(SCP_string&);
    
    virtual void reposition();
    virtual void load();
    virtual void unload();
    virtual void render();
};

class TbluiButton : public TbluiImage
{
private:
    typedef TbluiImage super;
    
public:
    SCP_string hover_file;
    SCP_string click_file;
    int hover_img;
    int click_img;
    
    // TODO: Sound?
    bool disabled;
    int state;
    
    TbluiButton() : hover_img(0), click_img(0), disabled(false), state(0) {}
    virtual bool parse_option(SCP_string&);
    
    virtual void render();
    virtual void load();
    virtual void unload();
    virtual void click(bool);
    virtual void hover(bool);
};

class TbluiSlider : public TbluiElement
{
private:
    typedef TbluiElement super;

public:
    int spos, spos_save, smax, area;
    bool horizontal;
    
    TbluiButton *up, *down, *circle;
    
    TbluiSlider();
    
    virtual bool parse_option(SCP_string& option);
    void set_max(int);
    
    virtual void reposition();
    virtual void render();
};

class TbluiTextBox : public TbluiText
{
private:
    typedef TbluiText super;

public:
    TbluiSlider *slider;
    int display_lines;
    
    TbluiTextBox();
    
    virtual bool parse_option(SCP_string& option);
    virtual void reposition();
    // virtual void load();
    // virtual void unload();
    virtual void render();
};

class TbluiModelViewer : public TbluiElement
{
private:
    typedef TbluiElement super;

public:
    SCP_string model_file;
    int model;
    float rotation;
    bool should_rotate;
    
    TbluiModelViewer();
    
    void load_model(SCP_string& filename);
    
    virtual bool parse_option(SCP_string& option);
    virtual void reposition();
    virtual void load();
    virtual void unload();
    virtual void render();
};

void tblui_open(SCP_string name);
void tblui_deinit();

void tblui_state_init();
void tblui_do_frame(float frametime);
void tblui_state_close();

void tblui_parse_table(SCP_string filename);

#endif