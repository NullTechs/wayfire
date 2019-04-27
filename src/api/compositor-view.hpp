#ifndef COMPOSITOR_VIEW_HPP
#define COMPOSITOR_VIEW_HPP

#include "compositor-surface.hpp"

/* Subclass this if you want the view to be able to interact with keyboard
 * and to receive focus */
class wayfire_compositor_interactive_view
{
    public:
        void handle_keyboard_enter() {}
        void handle_keyboard_leave() {}
        void handle_key(uint32_t key, uint32_t state) {}
};

wayfire_compositor_interactive_view *interactive_view_from_view(wayfire_view_t *view);

/* a base class for writing compositor views
 *
 * It can be used by plugins to create views with compositor-generated content */
class wayfire_compositor_view_t : public wayfire_compositor_surface_t, public wayfire_view_t
{
    protected:
        /* Implement _wlr_render_box to get something on screen */
        virtual void _wlr_render_box(const wf_framebuffer& fb, int x, int y, const wlr_box& scissor) { assert(false); }

    public:
        virtual bool is_mapped() { return _is_mapped; }
        virtual void send_frame_done(const timespec& now) {}
        virtual void subtract_opaque(wf_region& region, int x, int y) {}

        /* override this if you want to get pointer events or to stop input passthrough */
        virtual bool accepts_input(int32_t sx, int32_t sy) { return false; }

    public:
        wayfire_compositor_view_t();
        virtual ~wayfire_compositor_view_t() {}

        /* By default, use move/resize/set_geometry to set the size */
        virtual wf_point get_output_position();
        virtual wf_geometry get_output_geometry();
        virtual wf_geometry get_wm_geometry();
        virtual void set_geometry(wf_geometry g);

        virtual void activate(bool active) {}
        virtual void close();

        virtual wlr_surface *get_keyboard_focus_surface() { return nullptr; };

        virtual std::string get_app_id() { return "wayfire-compositor-view"; }
        virtual std::string get_title() { return "wayfire-compositor-view-" + this->object_base_t::to_string(); }

        virtual bool should_be_decorated() { return false; }

        /* Usually compositor view implementations don't need to override this */
        virtual void render_fb(const wf_region& region, const wf_framebuffer& fb);

        /* NON-API functions which don't have a meaning for compositor views */
        virtual bool update_size() { assert(false); }

        virtual void get_child_position(int &x, int &y) { x = y = 0; }
        virtual bool is_subsurface() { return false; }

        virtual void get_child_offset(int &x, int &y) { x = y = 0;}

        virtual void map();
        virtual void map(wlr_surface *surface) {map();}
        virtual void unmap();

        virtual wlr_buffer *get_buffer() { return NULL; }
        virtual bool can_take_snapshot() { return is_mapped(); }
        virtual void commit() {assert(false); }
};

/* A special type of compositor view - mirror view.
 * It takes another view and has the same size & contents, plus it "inherits"
 * all the transforms of the original view. However, it can have additional transforms,
 * be on another output, etc.
 *
 * The lifetime of a mirrored view isn't longer than that of the real view:
 * once the base view gets unmapped, this one is automatically unmapped as well */
class wayfire_mirror_view_t : public wayfire_compositor_view_t
{
    protected:
    signal_callback_t base_view_unmapped, base_view_damaged;

    virtual void _wlr_render_box(const wf_framebuffer& fb, int x, int y, const wlr_box& scissor);
    wayfire_view original_view;
    /* sets original_view to NULL and removes signal handlers */
    void unset_original_view();

    virtual wf_geometry get_untransformed_bounding_box();

    public:
    wayfire_mirror_view_t(wayfire_view original_view);
    virtual ~wayfire_mirror_view_t();

    virtual bool can_take_snapshot();
    virtual void take_snapshot();

    virtual void render_fb(const wf_region& damage, const wf_framebuffer& fb);
    virtual void simple_render(const wf_framebuffer& fb, int x, int y, const wf_region& damage);


    virtual wf_point get_output_position();
    virtual wf_geometry get_output_geometry();
    virtual wf_geometry get_wm_geometry();

    virtual void unmap();

    virtual wayfire_view get_original_view() { return original_view; }
};

/* A specialization of wayfire_compositor_view_t
 * Provides a simple view which is a colored rectangle with a border */
class wayfire_color_rect_view_t : public wayfire_compositor_view_t
{
    void _render_rect(const wf_framebuffer& fb, float projection[9],
        int x, int y, int w, int h, const wf_color& color);

    protected:
        wf_color _color;
        wf_color _border_color;
        int border;

        virtual void _wlr_render_box(const wf_framebuffer& fb, int x, int y, const wlr_box& scissor);

    public:
        wayfire_color_rect_view_t();

        /* The color settings accept non-premultiplied alpha */
        virtual void set_color(wf_color color);
        virtual void set_border_color(wf_color border);
        virtual void set_border(int width);
};

void emit_view_map(wayfire_view view);
void emit_view_unmap(wayfire_view view);

#endif /* end of include guard: COMPOSITOR_VIEW_HPP */
