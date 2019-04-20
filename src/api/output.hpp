#ifndef OUTPUT_HPP
#define OUTPUT_HPP

#include "plugin.hpp"
#include "object.hpp"
#include "view.hpp"

class workspace_manager;

namespace wf
{
class render_manager;

class output_t : public wf_object_base
{
  public:
    /**
     * The wlr_output that this output represents
     */
    wlr_output* handle;

    /**
     * The render manager of this output
     */
    std::unique_ptr<render_manager> render;

    /**
     * The workspace manager of this output
     */
    std::unique_ptr<workspace_manager> workspace;

    /**
     * Get a textual representation of the output
     */
    std::string to_string() const;

    /**
     * Get the logical resolution of the output, i.e if an output has mode
     * 3860x2160, scale 2 and transform 90, then get_screen_size will report
     * that it has logical resolution of 1080x1920
     */
    std::tuple<int, int> get_screen_size() const;

    /**
     * Same as get_screen_size() but returns a wf_geometry with x,y = 0
     */
    wf_geometry get_relative_geometry() const;

    /**
     * Returns the output geometry as the output layout sees it. This is
     * typically the same as get_relative_geometry() but with meaningful x and y
     */
    wf_geometry get_layout_geometry() const;

    /**
     * Moves the pointer so that it is inside the output
     */
    void ensure_pointer() const;

    /**
     * Gets the cursor position relative to the output
     */
    std::tuple<int, int> get_cursor_position() const;

    /**
     * Activates a plugin. Note that this may not succeed, if a plugin with the
     * same abilities is already active. However the same plugin might be
     * activated twice.
     *
     * @return true if the plugin was successfully activated, false otherwise.
     */
    virtual bool activate_plugin(wayfire_grab_interface owner) = 0;

    /**
     * Deactivates a plugin once, i.e if the plugin was activated more than
     * once, only one activation is removed.
     *
     * @return true if the plugin remains activated, false otherwise.
     */
    virtual bool deactivate_plugin(wayfire_grab_interface owner) = 0;

    /**
     * @return true if a grab interface with the given name is activated, false
     *              otherwise.
     */
    virtual bool is_plugin_active(owner_t owner_name) const = 0;

    /**
     * @return The topmost view in the workspace layer
     */
    wayfire_view get_top_view() const;

    /**
     * @return The currently focused view for the given output. The might not,
     * however, be actually focused, if the output isn't focused itself.
     */
    virtual wayfire_view get_active_view() const = 0;

    /**
     * Move view to the top of its layer without changing keyboard focus
     */
    void bring_to_front(wayfire_view v);

    /**
     * Sets the active view for the given seat, but without changing stacking
     * order.
     */
    virtual void set_active_view(wayfire_view v, wlr_seat *seat = nullptr) = 0;

    /**
     * Focuses the given view and raises it to the top of the stack.
     */
    void focus_view(wayfire_view v, wlr_seat *seat = nullptr);

    /**
     * Attach the view to the current output. This will change its output to
     * this output, and add it to the workspace layer.
     */
    void attach_view(wayfire_view v);

    /**
     * Detach the view from the current output, and remove it from its layer.
     * Note that the view output isn't reset.
     */
    void detach_view(wayfire_view v);

    /**
     * Switch the workspace so that view becomes visible.
     * @return true if workspace switch really occured
     */
    bool ensure_visible(wayfire_view view);

    /**
     * Force refocus the topmost view in one of the layers marked in layers
     * and which isn't skip_view
     */
    void refocus(wayfire_view skip_view, uint32_t layers);

    /**
     * Refocus the topmost focuseable view != skip_view, preferring regular views
     */
    void refocus(wayfire_view skip_view = nullptr);

    /**
     * the add_* functions are used by plugins to register bindings. They pass
     * a wf_option, which means that core will always use the latest binding
     * which is in the option.
     *
     * Adding a binding happens on a per-output basis. If a plugin registers
     * bindings on each output, it will receive for ex. a keybinding only on
     * the currently focused one.
     *
     * @return The wf_binding which can be used to unregister the binding.
     */
    wf_binding *add_key(wf_option key, key_callback *);
    wf_binding *add_axis(wf_option axis, axis_callback *);
    wf_binding *add_touch(wf_option mod, touch_callback *);
    wf_binding *add_button(wf_option button, button_callback *);
    wf_binding *add_gesture(wf_option gesture, gesture_callback *);
    wf_binding *add_activator(wf_option activator, activator_callback *);

    /**
     * Remove the given binding, regardless of its type.
     */
    void rem_binding(wf_binding *binding);

    /**
     * Remove all bindings which have the given callback, regardless of the type.
     */
    void rem_binding(void *callback);

    virtual ~output_t();
  protected:
    /* outputs are instantiated internally by core */
    output_t(wlr_output *handle);
};
}
#endif /* end of include guard: OUTPUT_HPP */
